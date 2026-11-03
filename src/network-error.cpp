#include "network-error.hpp"

QString friendlyNetworkError(QNetworkReply::NetworkError networkError, int httpStatus, const QString &serverMessage,
			     const QString &fallback)
{
	if (httpStatus == 429)
		return QStringLiteral("요청이 너무 많아 서버가 잠시 제한했습니다. 잠시 후 다시 시도해 주세요.");
	if (httpStatus >= 500)
		return QStringLiteral(
			"치지직 또는 중계 서버가 일시적으로 응답하지 않습니다. 잠시 후 다시 시도해 주세요.");
	if (!serverMessage.isEmpty())
		return serverMessage;

	switch (networkError) {
	case QNetworkReply::HostNotFoundError:
		return QStringLiteral("서버 주소를 찾지 못했습니다. 인터넷 또는 DNS 연결을 확인해 주세요.");
	case QNetworkReply::ConnectionRefusedError:
	case QNetworkReply::RemoteHostClosedError:
		return QStringLiteral("서버 연결이 거부되거나 중간에 종료되었습니다. 잠시 후 다시 시도해 주세요.");
	case QNetworkReply::SslHandshakeFailedError:
		return QStringLiteral("TLS 보안 연결에 실패했습니다. 설치 파일의 TLS 구성요소를 복구해 주세요.");
	case QNetworkReply::TimeoutError:
		return QStringLiteral("서버 응답 시간이 초과되었습니다. 인터넷 연결을 확인하고 다시 시도해 주세요.");
	case QNetworkReply::AuthenticationRequiredError:
	case QNetworkReply::ContentAccessDenied:
		return QStringLiteral("로그인 권한이 만료되었거나 요청 권한이 없습니다.");
	default:
		return fallback.isEmpty() ? QStringLiteral("알 수 없는 네트워크 오류가 발생했습니다.") : fallback;
	}
}
