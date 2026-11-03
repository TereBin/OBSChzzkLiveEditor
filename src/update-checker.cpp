#include "update-checker.hpp"
#include "update-utils.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

namespace {
constexpr auto kManifestUrl =
	"https://raw.githubusercontent.com/TereBin/OBSChzzkLiveEditor-Releases/main/update-manifest.json";
constexpr auto kDefaultReleaseUrl = "https://github.com/TereBin/OBSChzzkLiveEditor-Releases/releases/latest";
constexpr qint64 kCheckIntervalSeconds = 24 * 60 * 60;

UpdateLevel updateLevelFrom(const QString &value)
{
	if (value == QStringLiteral("security"))
		return UpdateLevel::Security;
	if (value == QStringLiteral("required"))
		return UpdateLevel::Required;
	if (value == QStringLiteral("recommended"))
		return UpdateLevel::Recommended;
	return UpdateLevel::Optional;
}

bool isValidReleaseUrl(const QUrl &url)
{
	return url.scheme() == QStringLiteral("https") && url.host() == QStringLiteral("github.com");
}
} // namespace

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {}

QString UpdateChecker::settingsPath() const
{
	char *path = obs_module_config_path("settings.ini");
	const QString result = QString::fromUtf8(path);
	bfree(path);
	return result;
}

bool UpdateChecker::checkedRecently() const
{
	QSettings settings(settingsPath(), QSettings::IniFormat);
	if (settings.value(QStringLiteral("updates/lastCheckedVersion")).toString() !=
	    QString::fromUtf8(PLUGIN_VERSION))
		return false;
	const qint64 lastCheckedAt = settings.value(QStringLiteral("updates/lastCheckedAt"), 0).toLongLong();
	return lastCheckedAt > QDateTime::currentSecsSinceEpoch() - kCheckIntervalSeconds;
}

void UpdateChecker::checkForUpdates(bool force)
{
	if (!force && checkedRecently())
		return;

	QNetworkRequest request(QUrl(QString::fromUtf8(kManifestUrl)));
	request.setRawHeader("Accept", "application/json");
	request.setRawHeader("Cache-Control", "no-cache");
	request.setRawHeader("User-Agent", QByteArray("OBSChzzkLiveEditor/") + PLUGIN_VERSION);
	request.setTransferTimeout(10000);
	QNetworkReply *reply = network_.get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		const QByteArray body = reply->readAll();
		const bool succeeded = reply->error() == QNetworkReply::NoError;
		const QString error = reply->errorString();
		reply->deleteLater();
		if (!succeeded) {
			emit checkCompleted(QStringLiteral("업데이트 서버 연결 실패: %1").arg(error), false);
			return;
		}

		QSettings settings(settingsPath(), QSettings::IniFormat);
		QDir().mkpath(QFileInfo(settings.fileName()).absolutePath());
		settings.setValue(QStringLiteral("updates/lastCheckedAt"), QDateTime::currentSecsSinceEpoch());
		settings.setValue(QStringLiteral("updates/lastCheckedVersion"), QString::fromUtf8(PLUGIN_VERSION));
		handleManifest(body);
		emit checkCompleted(QStringLiteral("업데이트 서버 연결이 정상입니다."), true);
	});
}

void UpdateChecker::skipVersion(const QString &version)
{
	QSettings settings(settingsPath(), QSettings::IniFormat);
	QDir().mkpath(QFileInfo(settings.fileName()).absolutePath());
	settings.setValue(QStringLiteral("updates/skippedVersion"), version);
}

void UpdateChecker::dismissNotice(const QString &id)
{
	QSettings settings(settingsPath(), QSettings::IniFormat);
	QDir().mkpath(QFileInfo(settings.fileName()).absolutePath());
	settings.setValue(QStringLiteral("notices/dismissedId"), id);
}

void UpdateChecker::handleNotice(const QJsonObject &object)
{
	const QJsonObject notice = object.value(QStringLiteral("notice")).toObject();
	NoticeInfo info;
	info.id = notice.value(QStringLiteral("id")).toString().trimmed();
	info.title = notice.value(QStringLiteral("title")).toString().trimmed();
	info.message = notice.value(QStringLiteral("message")).toString().trimmed();
	info.url = QUrl(notice.value(QStringLiteral("url")).toString());
	info.level = notice.value(QStringLiteral("level")).toString().trimmed().toLower();
	if (info.id.isEmpty() || info.message.isEmpty())
		return;

	const QDateTime expiresAt =
		QDateTime::fromString(notice.value(QStringLiteral("expiresAt")).toString(), Qt::ISODate);
	if (expiresAt.isValid() && expiresAt <= QDateTime::currentDateTimeUtc())
		return;

	QSettings settings(settingsPath(), QSettings::IniFormat);
	if (settings.value(QStringLiteral("notices/dismissedId")).toString() == info.id)
		return;
	emit noticeAvailable(info);
}

void UpdateChecker::handleManifest(const QByteArray &body)
{
	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isObject())
		return;

	const QJsonObject object = document.object();
	handleNotice(object);
	UpdateInfo info;
	info.latestVersion = object.value(QStringLiteral("latestVersion")).toString();
	info.minimumVersion = object.value(QStringLiteral("minimumSupportedVersion")).toString();
	info.message = object.value(QStringLiteral("message")).toString();
	info.releaseUrl = QUrl(object.value(QStringLiteral("releaseUrl")).toString());
	info.level = updateLevelFrom(object.value(QStringLiteral("level")).toString().toLower());

	const QString currentVersion = QString::fromUtf8(PLUGIN_VERSION);
	if (!UpdateUtils::isNewerVersion(currentVersion, info.latestVersion))
		return;
	if (UpdateUtils::isBelowMinimum(currentVersion, info.minimumVersion) && !info.blocksUse())
		info.level = UpdateLevel::Required;
	if (!isValidReleaseUrl(info.releaseUrl))
		info.releaseUrl = QUrl(QString::fromUtf8(kDefaultReleaseUrl));

	QSettings settings(settingsPath(), QSettings::IniFormat);
	const QString skippedVersion = settings.value(QStringLiteral("updates/skippedVersion")).toString();
	if (!info.blocksUse() && skippedVersion == info.latestVersion)
		return;
	emit updateAvailable(info);
}
