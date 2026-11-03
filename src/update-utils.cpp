#include "update-utils.hpp"

#include <QVersionNumber>

namespace UpdateUtils {
bool isNewerVersion(const QString &currentVersion, const QString &latestVersion)
{
	const QVersionNumber current = QVersionNumber::fromString(currentVersion);
	const QVersionNumber latest = QVersionNumber::fromString(latestVersion);
	return !current.isNull() && !latest.isNull() && QVersionNumber::compare(current, latest) < 0;
}

bool isBelowMinimum(const QString &currentVersion, const QString &minimumVersion)
{
	const QVersionNumber current = QVersionNumber::fromString(currentVersion);
	const QVersionNumber minimum = QVersionNumber::fromString(minimumVersion);
	return !current.isNull() && !minimum.isNull() && QVersionNumber::compare(current, minimum) < 0;
}
} // namespace UpdateUtils
