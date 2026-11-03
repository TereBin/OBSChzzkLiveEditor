#include "network-error.hpp"
#include "settings-utils.hpp"
#include "update-utils.hpp"

#include <QCoreApplication>
#include <cassert>

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	bool valid = false;
	assert(SettingsUtils::validatedTags(QStringLiteral("게임, 치지직, 게임"), valid) ==
	       QStringList({QStringLiteral("게임"), QStringLiteral("치지직")}));
	assert(valid);
	SettingsUtils::validatedTags(QStringLiteral("잘못된 태그"), valid);
	assert(!valid);
	assert(SettingsUtils::graphemeCount(QString::fromUtf8("가🙂")) == 2);
	assert(SettingsUtils::sameTags({QStringLiteral("a"), QStringLiteral("b")},
				       {QStringLiteral("b"), QStringLiteral("a")}));

	BroadcastSettings current{QStringLiteral("이전"), {}, QStringLiteral("old"), {}, {QStringLiteral("a")}};
	BroadcastSettings expected{QStringLiteral("새 제목"), {}, {}, {}, {QStringLiteral("b")}};
	BroadcastSettings actual{QStringLiteral("새 제목"), {}, QStringLiteral("old"), {}, {QStringLiteral("b")}};
	assert(SettingsUtils::settingsMatch(current, expected, false, actual));
	assert(friendlyNetworkError(QNetworkReply::TimeoutError, 0, {}, {}).contains(QStringLiteral("초과")));
	assert(friendlyNetworkError(QNetworkReply::UnknownNetworkError, 429, {}, {}).contains(QStringLiteral("너무 많")));
	assert(UpdateUtils::isNewerVersion(QStringLiteral("0.4.0"), QStringLiteral("0.10.0")));
	assert(!UpdateUtils::isNewerVersion(QStringLiteral("0.4.0"), QStringLiteral("0.4.0")));
	assert(UpdateUtils::isBelowMinimum(QStringLiteral("0.3.6"), QStringLiteral("0.4.0")));
	return 0;
}
