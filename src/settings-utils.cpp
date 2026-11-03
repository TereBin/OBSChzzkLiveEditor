#include "settings-utils.hpp"

#include <QRegularExpression>
#include <QTextBoundaryFinder>

namespace SettingsUtils {
int graphemeCount(const QString &text)
{
	QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
	int count = 0;
	finder.toStart();
	while (finder.toNextBoundary() != -1)
		++count;
	return count;
}

QStringList validatedTags(const QString &text, bool &valid)
{
	valid = true;
	QStringList tags;
	const QRegularExpression allowed(QStringLiteral("^[\\p{L}\\p{N}]+$"),
					 QRegularExpression::UseUnicodePropertiesOption);
	for (QString tag : text.split(',', Qt::SkipEmptyParts)) {
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

bool sameTags(QStringList left, QStringList right)
{
	left.sort(Qt::CaseSensitive);
	right.sort(Qt::CaseSensitive);
	return left == right;
}

QString effectiveCategoryId(const BroadcastSettings &current, const BroadcastSettings &edited, bool removeCategory)
{
	if (removeCategory)
		return {};
	return edited.categoryId.isEmpty() ? current.categoryId : edited.categoryId;
}

bool settingsMatch(const BroadcastSettings &current, const BroadcastSettings &expected, bool removeCategory,
		   const BroadcastSettings &actual)
{
	return expected.title == actual.title &&
	       effectiveCategoryId(current, expected, removeCategory) == actual.categoryId &&
	       sameTags(expected.tags, actual.tags);
}
} // namespace SettingsUtils
