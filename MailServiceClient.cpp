#include "MailServiceClient.h"
#include "GraphingErrorCode.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>

MailServiceClient::MailServiceClient(QObject *parent) : QObject(parent) {
    this->fromUser = "admin";
    this->senderAlias = "Graphing Server";
}

QString MailServiceClient::getEnvValue(const char* primaryName, const char* fallbackName) const {
    QString value = qEnvironmentVariable(primaryName);
    if (!value.isEmpty()) {
        return value;
    }

    if (fallbackName) {
        value = qEnvironmentVariable(fallbackName);
        if (!value.isEmpty()) {
            return value;
        }
    }

    return "";
}

FailableOperationResult MailServiceClient::configure() {
    QString baseUrlValue = this->getEnvValue("MAIL_SERVICE_BASE_URL");
    if (baseUrlValue.isEmpty()) {
        return FailableOperationResult::error(
            (int)GraphingErrorCode::InternalError,
            "MAIL_SERVICE_BASE_URL is not configured"
        );
    }

    QUrl parsedBaseUrl(baseUrlValue);
    if (!parsedBaseUrl.isValid() || parsedBaseUrl.scheme().isEmpty() || parsedBaseUrl.host().isEmpty()) {
        return FailableOperationResult::error(
            (int)GraphingErrorCode::InternalError,
            QString("MAIL_SERVICE_BASE_URL is invalid: %1").arg(baseUrlValue)
        );
    }

    this->baseUrl = parsedBaseUrl;

    QString configuredFromUser = this->getEnvValue("MAIL_SERVICE_FROM_USER");
    if (!configuredFromUser.isEmpty()) {
        this->fromUser = configuredFromUser;
    }

    QString configuredSenderAlias = this->getEnvValue("MAIL_SERVICE_SENDER_ALIAS");
    if (!configuredSenderAlias.isEmpty()) {
        this->senderAlias = configuredSenderAlias;
    }

    return FailableOperationResult::ok();
}

QUrl MailServiceClient::buildMessageUrl(const QString& recipientEmail) const {
    QUrl url = this->baseUrl;
    QString path = url.path();
    if (path.isEmpty()) {
        path = "/";
    }
    if (!path.endsWith('/')) {
        path.append('/');
    }
    path.append("message");
    url.setPath(path);

    QUrlQuery query(url);
    query.addQueryItem("from", this->fromUser);
    query.addQueryItem("to", recipientEmail);
    url.setQuery(query);

    return url;
}

QString MailServiceClient::buildPasswordResetText(const QString& recipientName, const QString& verificationCode) const {
    QString greetingName = recipientName.isEmpty() ? "user" : recipientName;
    return QString(
        "Hello, %1!\n\n"
        "Your password reset code is: %2\n"
        "This code is valid for 24 hours.\n\n"
        "If you did not request a password reset, just ignore this email."
    ).arg(greetingName, verificationCode);
}

QString MailServiceClient::buildPasswordResetHtml(const QString& recipientName, const QString& verificationCode) const {
    QString greetingName = recipientName.toHtmlEscaped();
    if (greetingName.isEmpty()) {
        greetingName = "user";
    }

    return QString(
        "<html><body>"
        "<p>Hello, %1!</p>"
        "<p>Your password reset code is: <strong>%2</strong></p>"
        "<p>This code is valid for 24 hours.</p>"
        "<p>If you did not request a password reset, just ignore this email.</p>"
        "</body></html>"
    ).arg(greetingName, verificationCode.toHtmlEscaped());
}

QString MailServiceClient::extractFailureMessage(const QJsonArray& logs) const {
    for (QJsonArray::const_iterator it = logs.begin(); it != logs.end(); ++it) {
        if (!it->isObject()) {
            continue;
        }

        QJsonObject logEntry = it->toObject();
        QJsonValue errorValue = logEntry.value("error");
        if (!errorValue.isObject()) {
            continue;
        }

        QJsonObject errorObject = errorValue.toObject();
        QString stepType = logEntry.value("step_type").toString("UNKNOWN");
        QString message = errorObject.value("message").toString();
        if (!message.isEmpty()) {
            return QString("Mail service failed at %1: %2").arg(stepType, message);
        }

        return QString("Mail service failed at %1").arg(stepType);
    }

    return "Mail service failed to send password reset email";
}

void MailServiceClient::sendPasswordResetMessage(
    quint64 clientId,
    quint64 requestId,
    const QString& token,
    const QString& recipientEmail,
    const QString& recipientName,
    const QString& verificationCode
) {
    if (!this->baseUrl.isValid()) {
        emit this->passwordResetMailFailed(
            clientId,
            requestId,
            token,
            (int)GraphingErrorCode::InternalError,
            "Mail service is not configured"
        );
        return;
    }

    QNetworkRequest request(this->buildMessageUrl(recipientEmail));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body.insert("title", "Password reset code");
    body.insert("content", this->buildPasswordResetText(recipientName, verificationCode));
    body.insert("html_content", this->buildPasswordResetHtml(recipientName, verificationCode));
    body.insert("sender_alias", this->senderAlias);

    QNetworkReply* reply = this->networkManager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, clientId, requestId, token]() {
        QByteArray responseBody = reply->readAll();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            QString message = QString("Cannot call mail service: %1").arg(reply->errorString());
            qWarning().noquote() << "[-] Mail service request failed:" << message << "body:" << QString::fromUtf8(responseBody);
            emit this->passwordResetMailFailed(clientId, requestId, token, (int)GraphingErrorCode::InternalError, message);
            reply->deleteLater();
            return;
        }

        if (httpStatus < 200 || httpStatus >= 300) {
            QString message = QString("Mail service returned HTTP %1").arg(httpStatus);
            qWarning().noquote() << "[-] Mail service returned non-success status:" << message << "body:" << QString::fromUtf8(responseBody);
            emit this->passwordResetMailFailed(clientId, requestId, token, (int)GraphingErrorCode::InternalError, message);
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        QJsonDocument responseDocument = QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            QString message = QString("Mail service returned invalid JSON: %1").arg(parseError.errorString());
            qWarning().noquote() << "[-] Mail service returned invalid JSON:" << message << "body:" << QString::fromUtf8(responseBody);
            emit this->passwordResetMailFailed(clientId, requestId, token, (int)GraphingErrorCode::InternalError, message);
            reply->deleteLater();
            return;
        }

        QJsonObject responseObject = responseDocument.object();
        if (responseObject.value("success").toBool(false)) {
            emit this->passwordResetMailSent(clientId, requestId, token);
            reply->deleteLater();
            return;
        }

        QString message = this->extractFailureMessage(responseObject.value("logs").toArray());
        qWarning().noquote() << "[-] Mail service could not send password reset email:" << message
                             << "response:" << QString::fromUtf8(responseBody);
        emit this->passwordResetMailFailed(clientId, requestId, token, (int)GraphingErrorCode::InternalError, message);
        reply->deleteLater();
    });
}
