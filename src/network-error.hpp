#pragma once

#include <QNetworkReply>
#include <QString>

QString friendlyNetworkError(QNetworkReply::NetworkError networkError, int httpStatus,
			     const QString &serverMessage, const QString &fallback);
