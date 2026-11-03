#pragma once

#include <QString>

namespace UpdateUtils {
bool isNewerVersion(const QString &currentVersion, const QString &latestVersion);
bool isBelowMinimum(const QString &currentVersion, const QString &minimumVersion);
} // namespace UpdateUtils
