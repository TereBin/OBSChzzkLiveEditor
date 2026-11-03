#pragma once

#include "chzzk-types.hpp"

#include <QStringList>

namespace SettingsUtils {
int graphemeCount(const QString &text);
QStringList validatedTags(const QString &text, bool &valid);
bool sameTags(QStringList left, QStringList right);
QString effectiveCategoryId(const BroadcastSettings &current, const BroadcastSettings &edited, bool removeCategory);
bool settingsMatch(const BroadcastSettings &current, const BroadcastSettings &expected, bool removeCategory,
		   const BroadcastSettings &actual);
} // namespace SettingsUtils
