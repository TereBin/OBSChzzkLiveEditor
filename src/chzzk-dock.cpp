#include "chzzk-dock.hpp"

#include <QComboBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QTextBoundaryFinder>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kCategoryTypeRole = Qt::UserRole + 1;
constexpr int kCategoryIdRole = Qt::UserRole + 2;
constexpr int kCategorySearchDelayMs = 350;
constexpr int kCompactLayoutWidth = 420;
constexpr auto kDefaultReleaseUrl = "https://github.com/TereBin/OBSChzzkLiveEditor-Releases/releases/latest";

int graphemeCount(const QString &text)
{
	QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
	int count = 0;
	finder.toStart();
	while (finder.toNextBoundary() != -1)
		++count;
	return count;
}

bool sameTags(QStringList left, QStringList right)
{
	left.sort(Qt::CaseSensitive);
	right.sort(Qt::CaseSensitive);
	return left == right;
}
} // namespace

void CategoryLineEdit::inputMethodEvent(QInputMethodEvent *event)
{
	const bool composing = !event->preeditString().isEmpty();
	const bool changed = composing_ != composing;
	composing_ = composing;
	QLineEdit::inputMethodEvent(event);
	if (changed)
		emit compositionChanged(composing_);
}

ChzzkDock::ChzzkDock(QWidget *parent) : QWidget(parent), client_(this), updateChecker_(this)
{
	buildUi();
	connectUi();
	updateLoginState(client_.isLoggedIn());
	if (client_.isLoggedIn()) {
		QTimer::singleShot(0, &client_, &ChzzkApiClient::loadUserProfile);
		QTimer::singleShot(0, &client_, &ChzzkApiClient::loadSettings);
	}
	QTimer::singleShot(5000, &updateChecker_, [this]() { updateChecker_.checkForUpdates(); });
}

void ChzzkDock::connectUi()
{
	connect(loginButton_, &QPushButton::clicked, &client_, &ChzzkApiClient::beginLogin);
	connect(logoutButton_, &QPushButton::clicked, &client_, &ChzzkApiClient::logout);
	connect(diagnosticsButton_, &QPushButton::clicked, &client_, &ChzzkApiClient::runDiagnostics);
	connect(refreshButton_, &QPushButton::clicked, &client_, &ChzzkApiClient::loadSettings);
	connect(applyButton_, &QPushButton::clicked, this, &ChzzkDock::applyChanges);
	connect(clearCategoryButton_, &QPushButton::clicked, this, &ChzzkDock::clearCategory);
	connect(&client_, &ChzzkApiClient::loginStateChanged, this, &ChzzkDock::updateLoginState);
	connect(&client_, &ChzzkApiClient::userProfileLoaded, this, &ChzzkDock::showUserProfile);
	connect(&client_, &ChzzkApiClient::settingsLoaded, this, &ChzzkDock::showSettings);
	connect(&client_, &ChzzkApiClient::categoriesLoaded, this, &ChzzkDock::showCategories);
	connect(&client_, &ChzzkApiClient::diagnosticsCompleted, this, &ChzzkDock::showDiagnostics);
	connect(&client_, &ChzzkApiClient::operationSucceeded, this, &ChzzkDock::showSuccess);
	connect(&client_, &ChzzkApiClient::operationFailed, this, &ChzzkDock::showError);
	connect(&client_, &ChzzkApiClient::clientUpdateRequired, this,
		[this](const QString &latestVersion, const QString &minimumVersion, const QString &level,
		       const QString &message, const QUrl &releaseUrl) {
			UpdateInfo info;
			info.latestVersion = latestVersion.isEmpty() ? minimumVersion : latestVersion;
			info.minimumVersion = minimumVersion;
			info.message = message;
			info.releaseUrl = releaseUrl;
			info.level = level == QStringLiteral("security") ? UpdateLevel::Security
									 : UpdateLevel::Required;
			showUpdate(info);
		});
	connect(&updateChecker_, &UpdateChecker::updateAvailable, this, &ChzzkDock::showUpdate);
	connect(&updateChecker_, &UpdateChecker::noticeAvailable, this, &ChzzkDock::showNotice);
	connect(downloadUpdateButton_, &QPushButton::clicked, this, &ChzzkDock::openUpdatePage);
	connect(skipUpdateButton_, &QPushButton::clicked, this, [this]() {
		updateChecker_.skipVersion(pendingUpdate_.latestVersion);
		updateBanner_->hide();
	});
	connect(openNoticeButton_, &QPushButton::clicked, this, [this]() {
		if (pendingNotice_.url.scheme() == QStringLiteral("https"))
			QDesktopServices::openUrl(pendingNotice_.url);
	});
	connect(dismissNoticeButton_, &QPushButton::clicked, this, [this]() {
		updateChecker_.dismissNotice(pendingNotice_.id);
		noticeBanner_->hide();
	});
	connect(&client_, &ChzzkApiClient::busyChanged, this, [this](bool busy) {
		busy_ = busy;
		updateControls();
	});
	connect(categoryTimer_, &QTimer::timeout, this, [this]() { client_.searchCategories(categoryEdit_->text()); });
	connect(categoryEdit_, &QLineEdit::textEdited, this, [this](const QString &) {
		resetCategorySelection(false);
		categoryCombo_->hidePopup();
		scheduleCategorySearch();
	});
	connect(categoryEdit_, &CategoryLineEdit::compositionChanged, this, [this](bool composing) {
		if (composing)
			categoryTimer_->stop();
		else
			scheduleCategorySearch();
	});
	connect(categoryCombo_, qOverload<int>(&QComboBox::activated), this, [this](int index) {
		selectedCategoryType_ = categoryCombo_->itemData(index, kCategoryTypeRole).toString();
		selectedCategoryId_ = categoryCombo_->itemData(index, kCategoryIdRole).toString();
		removeCategory_ = false;
		updateChangeSummary();
	});
	connect(titleEdit_, &QLineEdit::textChanged, this, &ChzzkDock::updateInputStatus);
	connect(tagsEdit_, &QLineEdit::textChanged, this, &ChzzkDock::updateInputStatus);
}

void ChzzkDock::buildUi()
{
	setMinimumWidth(300);
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(12, 12, 12, 12);
	root->setSpacing(10);

	accountLayout_ = new QHBoxLayout();
	statusLabel_ = new QLabel(this);
	statusLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	loginButton_ = new QPushButton(QStringLiteral("로그인"), this);
	logoutButton_ = new QPushButton(QStringLiteral("로그아웃"), this);
	diagnosticsButton_ = new QPushButton(QStringLiteral("진단"), this);
	diagnosticsButton_->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
	diagnosticsButton_->setToolTip(QStringLiteral("Worker, TLS, 토큰, 치지직 API 연결 확인"));
	accountLayout_->addWidget(statusLabel_);
	accountLayout_->addWidget(diagnosticsButton_);
	accountLayout_->addWidget(loginButton_);
	accountLayout_->addWidget(logoutButton_);
	root->addLayout(accountLayout_);

	noticeBanner_ = new QWidget(this);
	noticeLayout_ = new QHBoxLayout(noticeBanner_);
	noticeLayout_->setContentsMargins(0, 0, 0, 0);
	noticeLabel_ = new QLabel(noticeBanner_);
	noticeLabel_->setWordWrap(true);
	noticeLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	openNoticeButton_ = new QPushButton(QStringLiteral("자세히"), noticeBanner_);
	dismissNoticeButton_ = new QPushButton(QStringLiteral("확인"), noticeBanner_);
	noticeLayout_->addWidget(noticeLabel_, 1);
	noticeLayout_->addWidget(openNoticeButton_);
	noticeLayout_->addWidget(dismissNoticeButton_);
	noticeBanner_->hide();
	root->addWidget(noticeBanner_);

	updateBanner_ = new QWidget(this);
	updateLayout_ = new QHBoxLayout(updateBanner_);
	updateLayout_->setContentsMargins(0, 0, 0, 0);
	updateLabel_ = new QLabel(updateBanner_);
	updateLabel_->setWordWrap(true);
	updateLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	downloadUpdateButton_ = new QPushButton(QStringLiteral("다운로드"), updateBanner_);
	skipUpdateButton_ = new QPushButton(QStringLiteral("건너뛰기"), updateBanner_);
	updateLayout_->addWidget(updateLabel_, 1);
	updateLayout_->addWidget(downloadUpdateButton_);
	updateLayout_->addWidget(skipUpdateButton_);
	updateBanner_->hide();
	root->addWidget(updateBanner_);

	formLayout_ = new QFormLayout();
	formLayout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	auto *titleRow = new QHBoxLayout();
	titleEdit_ = new QLineEdit(this);
	titleEdit_->setPlaceholderText(QStringLiteral("방송 제목"));
	titleCountLabel_ = new QLabel(QStringLiteral("0자"), this);
	titleCountLabel_->setMinimumWidth(36);
	titleCountLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	titleRow->addWidget(titleEdit_, 1);
	titleRow->addWidget(titleCountLabel_);
	formLayout_->addRow(QStringLiteral("제목"), titleRow);

	auto *categoryRow = new QHBoxLayout();
	categoryCombo_ = new QComboBox(this);
	categoryCombo_->setEditable(true);
	categoryEdit_ = new CategoryLineEdit(categoryCombo_);
	categoryCombo_->setLineEdit(categoryEdit_);
	categoryCombo_->setInsertPolicy(QComboBox::NoInsert);
	categoryCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	categoryCombo_->setMinimumContentsLength(14);
	categoryEdit_->setPlaceholderText(QStringLiteral("두 글자 이상 검색"));
	clearCategoryButton_ = new QPushButton(this);
	clearCategoryButton_->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
	clearCategoryButton_->setToolTip(QStringLiteral("카테고리 제거"));
	clearCategoryButton_->setFixedWidth(34);
	categoryRow->addWidget(categoryCombo_, 1);
	categoryRow->addWidget(clearCategoryButton_);
	formLayout_->addRow(QStringLiteral("카테고리"), categoryRow);

	auto *tagsColumn = new QVBoxLayout();
	tagsColumn->setContentsMargins(0, 0, 0, 0);
	tagsColumn->setSpacing(2);
	tagsEdit_ = new QLineEdit(this);
	tagsEdit_->setPlaceholderText(QStringLiteral("쉼표로 구분"));
	tagsValidationLabel_ = new QLabel(QStringLiteral("태그 없음 · 유효"), this);
	tagsColumn->addWidget(tagsEdit_);
	tagsColumn->addWidget(tagsValidationLabel_);
	formLayout_->addRow(QStringLiteral("태그"), tagsColumn);
	root->addLayout(formLayout_);

	changeLabel_ = new QLabel(this);
	changeLabel_->setWordWrap(true);
	root->addWidget(changeLabel_);

	messageLabel_ = new QLabel(this);
	messageLabel_->setWordWrap(true);
	messageLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	root->addWidget(messageLabel_);

	root->addStretch(1);
	actionLayout_ = new QHBoxLayout();
	refreshButton_ = new QPushButton(QStringLiteral("새로고침"), this);
	refreshButton_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	applyButton_ = new QPushButton(QStringLiteral("적용"), this);
	applyButton_->setDefault(true);
	actionLayout_->addWidget(refreshButton_);
	actionLayout_->addWidget(applyButton_);
	root->addLayout(actionLayout_);

	categoryTimer_ = new QTimer(this);
	categoryTimer_->setSingleShot(true);
	categoryTimer_->setInterval(kCategorySearchDelayMs);
	updateInputStatus();
	updateResponsiveLayout();
}

void ChzzkDock::updateLoginState(bool loggedIn)
{
	loggedIn_ = loggedIn;
	statusLabel_->setText(updateRequired_                       ? QStringLiteral("업데이트 필요")
			      : loggedIn && !channelName_.isEmpty() ? QStringLiteral("채널: %1").arg(channelName_)
			      : loggedIn                            ? QStringLiteral("치지직 연결됨")
								    : QStringLiteral("로그인 필요"));
	loginButton_->setVisible(!loggedIn);
	logoutButton_->setVisible(loggedIn);
	diagnosticsButton_->setEnabled(!busy_);
	updateControls();
	if (!loggedIn) {
		channelName_.clear();
		titleEdit_->clear();
		categoryCombo_->clear();
		tagsEdit_->clear();
		resetCategorySelection(false);
		hasCurrentSettings_ = false;
		hasPendingApply_ = false;
		changeLabel_->clear();
	}
	updateInputStatus();
}

void ChzzkDock::showUserProfile(const QString &channelName)
{
	channelName_ = channelName;
	updateLoginState(loggedIn_);
}

void ChzzkDock::updateControls()
{
	const bool available = !busy_ && !updateRequired_;
	const bool canEdit = loggedIn_ && available;
	loginButton_->setEnabled(!loggedIn_ && available);
	logoutButton_->setEnabled(loggedIn_ && available);
	diagnosticsButton_->setEnabled(!busy_ && !updateRequired_);
	titleEdit_->setEnabled(canEdit);
	categoryCombo_->setEnabled(canEdit);
	clearCategoryButton_->setEnabled(canEdit);
	tagsEdit_->setEnabled(canEdit);
	refreshButton_->setEnabled(canEdit);
	bool validTags = false;
	validatedTags(validTags);
	applyButton_->setEnabled(canEdit && !titleEdit_->text().trimmed().isEmpty() && validTags);
}

void ChzzkDock::scheduleCategorySearch()
{
	if (categoryEdit_->isComposing() || !loggedIn_) {
		categoryTimer_->stop();
		return;
	}
	categoryTimer_->start();
}

void ChzzkDock::resetCategorySelection(bool removeCategory)
{
	selectedCategoryId_.clear();
	selectedCategoryType_.clear();
	removeCategory_ = removeCategory;
	updateChangeSummary();
}

void ChzzkDock::showSettings(const BroadcastSettings &settings, bool afterApply)
{
	const QString appliedSummary = hasPendingApply_ ? changeSummary(pendingSettings_, pendingRemoveCategory_, false)
							: QString();
	const bool verified = !afterApply ||
			      (hasPendingApply_ && settingsMatch(pendingSettings_, pendingRemoveCategory_, settings));
	currentSettings_ = settings;
	hasCurrentSettings_ = true;
	titleEdit_->setText(settings.title);
	selectedCategoryId_ = settings.categoryId;
	selectedCategoryType_ = settings.categoryType;
	removeCategory_ = false;
	{
		const QSignalBlocker blocker(categoryCombo_);
		categoryCombo_->clear();
		if (!settings.categoryId.isEmpty()) {
			categoryCombo_->addItem(settings.categoryName);
			categoryCombo_->setItemData(0, settings.categoryType, kCategoryTypeRole);
			categoryCombo_->setItemData(0, settings.categoryId, kCategoryIdRole);
			categoryCombo_->setCurrentIndex(0);
		}
	}
	tagsEdit_->setText(settings.tags.join(QStringLiteral(", ")));
	if (afterApply && verified)
		showSuccess(appliedSummary.isEmpty() ? QStringLiteral("방송 정보를 적용하고 서버 값까지 확인했습니다.")
						     : QStringLiteral("적용 및 확인 완료: %1").arg(appliedSummary));
	else if (afterApply)
		showError(QStringLiteral(
			"요청은 처리됐지만 서버에서 확인한 값이 다릅니다. 현재 서버 값을 표시했습니다."));
	else
		showSuccess(QStringLiteral("방송 정보를 불러왔습니다."));
	hasPendingApply_ = false;
	updateInputStatus();
}

void ChzzkDock::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	updateResponsiveLayout();
}

void ChzzkDock::updateResponsiveLayout()
{
	if (!accountLayout_ || !noticeLayout_ || !updateLayout_ || !actionLayout_ || !formLayout_)
		return;
	const bool compact = width() < kCompactLayoutWidth;
	const QBoxLayout::Direction direction = compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
	accountLayout_->setDirection(direction);
	noticeLayout_->setDirection(direction);
	updateLayout_->setDirection(direction);
	actionLayout_->setDirection(direction);
	formLayout_->setRowWrapPolicy(compact ? QFormLayout::WrapAllRows : QFormLayout::DontWrapRows);
}

void ChzzkDock::showNotice(const NoticeInfo &info)
{
	pendingNotice_ = info;
	const QString title = info.title.isEmpty() ? QStringLiteral("공지") : info.title;
	noticeLabel_->setText(QStringLiteral("%1: %2").arg(title, info.message));
	openNoticeButton_->setVisible(info.url.scheme() == QStringLiteral("https"));
	noticeBanner_->show();

	if (info.level == QStringLiteral("important") || info.level == QStringLiteral("critical"))
		QMessageBox::warning(this, title, info.message);
}

void ChzzkDock::showCategories(const QString &query, const QVector<ChzzkCategory> &categories)
{
	const QString typed = categoryCombo_->currentText();
	if (categoryEdit_->isComposing() || typed.trimmed() != query)
		return;
	const QSignalBlocker blocker(categoryCombo_);
	categoryCombo_->clear();
	for (const ChzzkCategory &category : categories) {
		categoryCombo_->addItem(category.name);
		const int index = categoryCombo_->count() - 1;
		categoryCombo_->setItemData(index, category.type, kCategoryTypeRole);
		categoryCombo_->setItemData(index, category.id, kCategoryIdRole);
	}
	categoryCombo_->setEditText(typed);
	if (!categories.isEmpty())
		categoryCombo_->showPopup();
}

QStringList ChzzkDock::validatedTags(bool &valid) const
{
	valid = true;
	QStringList tags;
	const QRegularExpression allowed(QStringLiteral("^[\\p{L}\\p{N}]+$"),
					 QRegularExpression::UseUnicodePropertiesOption);
	for (QString tag : tagsEdit_->text().split(',', Qt::SkipEmptyParts)) {
		tag = tag.trimmed();
		if (!allowed.match(tag).hasMatch()) {
			valid = false;
			return {};
		}
		if (!tags.contains(tag))
			tags.append(tag);
	}
	return tags;
}

void ChzzkDock::updateInputStatus()
{
	titleCountLabel_->setText(QStringLiteral("%1자").arg(graphemeCount(titleEdit_->text())));
	bool valid = false;
	const QStringList tags = validatedTags(valid);
	tagsValidationLabel_->setText(valid ? (tags.isEmpty() ? QStringLiteral("태그 없음 · 유효")
							      : QStringLiteral("태그 %1개 · 유효").arg(tags.size()))
					    : QStringLiteral("공백·특수문자는 사용할 수 없습니다."));
	tagsValidationLabel_->setStyleSheet(valid ? QString() : QStringLiteral("color: #d94b4b;"));
	updateChangeSummary();
	updateControls();
}

QString ChzzkDock::changeSummary(const BroadcastSettings &settings, bool removeCategory, bool detailed) const
{
	if (!hasCurrentSettings_)
		return {};
	QStringList changes;
	auto display = [](const QString &value) {
		return value.isEmpty() ? QStringLiteral("(없음)") : value;
	};
	if (settings.title != currentSettings_.title)
		changes.append(detailed ? QStringLiteral("제목: %1 → %2")
						  .arg(display(currentSettings_.title), display(settings.title))
					: QStringLiteral("제목"));
	const QString categoryId = effectiveCategoryId(settings, removeCategory);
	if (categoryId != currentSettings_.categoryId) {
		const QString categoryName = removeCategory ? QString() : settings.categoryName;
		changes.append(detailed ? QStringLiteral("카테고리: %1 → %2")
						  .arg(display(currentSettings_.categoryName), display(categoryName))
					: QStringLiteral("카테고리"));
	}
	if (!sameTags(settings.tags, currentSettings_.tags))
		changes.append(detailed ? QStringLiteral("태그: %1 → %2")
						  .arg(display(currentSettings_.tags.join(QStringLiteral(", "))),
						       display(settings.tags.join(QStringLiteral(", "))))
					: QStringLiteral("태그"));
	return changes.join(detailed ? QStringLiteral("\n") : QStringLiteral(", "));
}

bool ChzzkDock::settingsMatch(const BroadcastSettings &expected, bool removeCategory,
			      const BroadcastSettings &actual) const
{
	return expected.title == actual.title && effectiveCategoryId(expected, removeCategory) == actual.categoryId &&
	       sameTags(expected.tags, actual.tags);
}

QString ChzzkDock::effectiveCategoryId(const BroadcastSettings &settings, bool removeCategory) const
{
	if (removeCategory)
		return {};
	return settings.categoryId.isEmpty() ? currentSettings_.categoryId : settings.categoryId;
}

BroadcastSettings ChzzkDock::editedSettings(bool &validTags) const
{
	BroadcastSettings settings;
	settings.title = titleEdit_->text().trimmed();
	settings.categoryId = selectedCategoryId_;
	settings.categoryType = selectedCategoryType_;
	settings.categoryName = categoryCombo_->currentText().trimmed();
	settings.tags = validatedTags(validTags);
	return settings;
}

void ChzzkDock::updateChangeSummary()
{
	if (!hasCurrentSettings_) {
		changeLabel_->clear();
		return;
	}
	bool validTags = false;
	const BroadcastSettings edited = editedSettings(validTags);
	const QString summary = validTags ? changeSummary(edited, removeCategory_, false) : QString();
	changeLabel_->setText(summary.isEmpty() ? QStringLiteral("변경 사항 없음")
						: QStringLiteral("변경 예정: %1").arg(summary));
}

void ChzzkDock::applyChanges()
{
	const QString title = titleEdit_->text().trimmed();
	if (title.isEmpty()) {
		showError(QStringLiteral("방송 제목은 비워 둘 수 없습니다."));
		return;
	}
	if (!removeCategory_ && !categoryCombo_->currentText().trimmed().isEmpty() && selectedCategoryId_.isEmpty()) {
		showError(QStringLiteral("검색 결과에서 카테고리를 선택해 주세요."));
		return;
	}

	bool validTags = false;
	validatedTags(validTags);
	if (!validTags) {
		showError(QStringLiteral("태그에는 글자와 숫자만 사용할 수 있습니다."));
		return;
	}

	BroadcastSettings settings = editedSettings(validTags);
	const QString details = changeSummary(settings, removeCategory_, true);
	if (details.isEmpty()) {
		showSuccess(QStringLiteral("변경된 방송 정보가 없습니다."));
		return;
	}
	if (QMessageBox::question(this, QStringLiteral("방송 정보 변경"),
				  QStringLiteral("다음 내용을 적용할까요?\n\n%1").arg(details),
				  QMessageBox::Apply | QMessageBox::Cancel, QMessageBox::Apply) != QMessageBox::Apply)
		return;
	pendingSettings_ = settings;
	pendingRemoveCategory_ = removeCategory_;
	hasPendingApply_ = true;
	client_.updateSettings(settings, removeCategory_);
}

void ChzzkDock::clearCategory()
{
	categoryTimer_->stop();
	categoryCombo_->hidePopup();
	categoryCombo_->clear();
	resetCategorySelection(true);
	showSuccess(QStringLiteral("적용하면 카테고리가 제거됩니다."));
}

void ChzzkDock::showDiagnostics(const QString &message, bool healthy)
{
	if (healthy)
		showSuccess(QStringLiteral("연결 진단: %1").arg(message));
	else
		showError(QStringLiteral("연결 진단: %1").arg(message));
}

void ChzzkDock::showSuccess(const QString &message)
{
	messageLabel_->setStyleSheet(QStringLiteral("color: palette(highlight);"));
	messageLabel_->setText(message);
}

void ChzzkDock::showError(const QString &message)
{
	messageLabel_->setStyleSheet(QStringLiteral("color: #d94b4b;"));
	messageLabel_->setText(message);
}

void ChzzkDock::showUpdate(const UpdateInfo &info)
{
	pendingUpdate_ = info;
	if (pendingUpdate_.releaseUrl.scheme() != QStringLiteral("https") ||
	    pendingUpdate_.releaseUrl.host() != QStringLiteral("github.com")) {
		pendingUpdate_.releaseUrl = QUrl(QString::fromUtf8(kDefaultReleaseUrl));
	}
	updateRequired_ = info.blocksUse();
	updateControls();

	QString prefix = QStringLiteral("새 버전 %1").arg(info.latestVersion);
	if (info.level == UpdateLevel::Recommended)
		prefix = QStringLiteral("권장 업데이트 %1").arg(info.latestVersion);
	else if (info.level == UpdateLevel::Required)
		prefix = QStringLiteral("필수 업데이트 %1").arg(info.latestVersion);
	else if (info.level == UpdateLevel::Security)
		prefix = QStringLiteral("보안 업데이트 %1").arg(info.latestVersion);
	updateLabel_->setText(info.message.isEmpty() ? prefix : QStringLiteral("%1: %2").arg(prefix, info.message));
	skipUpdateButton_->setVisible(!info.blocksUse());
	updateBanner_->show();

	if (!info.blocksUse() || promptedUpdateVersion_ == info.latestVersion)
		return;
	promptedUpdateVersion_ = info.latestVersion;
	statusLabel_->setText(QStringLiteral("업데이트 필요"));
	const QString title = info.level == UpdateLevel::Security ? QStringLiteral("보안 업데이트 필요")
								  : QStringLiteral("필수 업데이트 필요");
	const QString detail =
		info.message.isEmpty()
			? QStringLiteral(
				  "이 버전은 더 이상 지원되지 않습니다. 계속 사용하려면 %1 이상으로 업데이트하세요.")
				  .arg(info.minimumVersion)
			: info.message;
	if (QMessageBox::critical(this, title, detail, QMessageBox::Open | QMessageBox::Close, QMessageBox::Open) ==
	    QMessageBox::Open) {
		openUpdatePage();
	}
}

void ChzzkDock::openUpdatePage()
{
	if (pendingUpdate_.releaseUrl.isValid())
		QDesktopServices::openUrl(pendingUpdate_.releaseUrl);
}
