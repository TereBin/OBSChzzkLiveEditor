#pragma once

#include "chzzk-api-client.hpp"
#include "update-checker.hpp"

#include <QLineEdit>
#include <QWidget>

class QComboBox;
class QBoxLayout;
class QFormLayout;
class QLabel;
class QPushButton;
class QTimer;
class QResizeEvent;

class CategoryLineEdit final : public QLineEdit {
	Q_OBJECT

public:
	explicit CategoryLineEdit(QWidget *parent = nullptr) : QLineEdit(parent) {}
	bool isComposing() const { return composing_; }

signals:
	void compositionChanged(bool composing);

protected:
	void inputMethodEvent(QInputMethodEvent *event) override;

private:
	bool composing_ = false;
};

class ChzzkDock final : public QWidget {
	Q_OBJECT

public:
	explicit ChzzkDock(QWidget *parent = nullptr);

private slots:
	void updateLoginState(bool loggedIn);
	void showUserProfile(const QString &channelName);
	void showSettings(const BroadcastSettings &settings, bool afterApply);
	void showCategories(const QString &query, const QVector<ChzzkCategory> &categories);
	void showDiagnostics(const QString &message, bool healthy);
	void applyChanges();
	void clearCategory();
	void showSuccess(const QString &message);
	void showError(const QString &message);
	void showUpdate(const UpdateInfo &info);
	void showNotice(const NoticeInfo &info);

private:
	void resizeEvent(QResizeEvent *event) override;
	void buildUi();
	void connectUi();
	void updateResponsiveLayout();
	void updateControls();
	void scheduleCategorySearch();
	void resetCategorySelection(bool removeCategory);
	void updateInputStatus();
	void updateChangeSummary();
	BroadcastSettings editedSettings(bool &validTags) const;
	QString effectiveCategoryId(const BroadcastSettings &settings, bool removeCategory) const;
	QString changeSummary(const BroadcastSettings &settings, bool removeCategory, bool detailed) const;
	bool settingsMatch(const BroadcastSettings &expected, bool removeCategory,
			   const BroadcastSettings &actual) const;
	void openUpdatePage();
	QStringList validatedTags(bool &valid) const;

	ChzzkApiClient client_;
	UpdateChecker updateChecker_;
	QBoxLayout *accountLayout_ = nullptr;
	QBoxLayout *updateLayout_ = nullptr;
	QBoxLayout *noticeLayout_ = nullptr;
	QBoxLayout *actionLayout_ = nullptr;
	QFormLayout *formLayout_ = nullptr;
	QLabel *statusLabel_ = nullptr;
	QLabel *messageLabel_ = nullptr;
	QLabel *changeLabel_ = nullptr;
	QLabel *titleCountLabel_ = nullptr;
	QLabel *tagsValidationLabel_ = nullptr;
	QWidget *updateBanner_ = nullptr;
	QWidget *noticeBanner_ = nullptr;
	QLabel *noticeLabel_ = nullptr;
	QPushButton *openNoticeButton_ = nullptr;
	QPushButton *dismissNoticeButton_ = nullptr;
	QLabel *updateLabel_ = nullptr;
	QPushButton *downloadUpdateButton_ = nullptr;
	QPushButton *skipUpdateButton_ = nullptr;
	QPushButton *loginButton_ = nullptr;
	QPushButton *logoutButton_ = nullptr;
	QPushButton *diagnosticsButton_ = nullptr;
	QLineEdit *titleEdit_ = nullptr;
	QComboBox *categoryCombo_ = nullptr;
	CategoryLineEdit *categoryEdit_ = nullptr;
	QPushButton *clearCategoryButton_ = nullptr;
	QLineEdit *tagsEdit_ = nullptr;
	QPushButton *refreshButton_ = nullptr;
	QPushButton *applyButton_ = nullptr;
	QTimer *categoryTimer_ = nullptr;
	QString selectedCategoryId_;
	QString selectedCategoryType_;
	QString channelName_;
	UpdateInfo pendingUpdate_;
	NoticeInfo pendingNotice_;
	QString promptedUpdateVersion_;
	BroadcastSettings currentSettings_;
	BroadcastSettings pendingSettings_;
	bool loggedIn_ = false;
	bool busy_ = false;
	bool updateRequired_ = false;
	bool removeCategory_ = false;
	bool hasCurrentSettings_ = false;
	bool hasPendingApply_ = false;
	bool pendingRemoveCategory_ = false;
};
