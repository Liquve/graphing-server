#ifndef MAILSERVICECLIENT_H
#define MAILSERVICECLIENT_H

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include "FailableOperationResult.h"

class MailServiceClient : public QObject
{
    Q_OBJECT
private:
    QNetworkAccessManager networkManager;
    QUrl baseUrl;
    QString fromUser;
    QString senderAlias;

    QString getEnvValue(const char*, const char* = nullptr) const;
    QUrl buildMessageUrl(const QString&) const;
    QString buildPasswordResetText(const QString&, const QString&) const;
    QString buildPasswordResetHtml(const QString&, const QString&) const;
    QString extractFailureMessage(const QJsonArray&) const;
public:
    explicit MailServiceClient(QObject *parent = nullptr);

    FailableOperationResult configure();
    void sendPasswordResetMessage(
        quint64 clientId,
        quint64 requestId,
        const QString& token,
        const QString& recipientEmail,
        const QString& recipientName,
        const QString& verificationCode
    );
signals:
    void passwordResetMailSent(quint64 clientId, quint64 requestId, QString token);
    void passwordResetMailFailed(quint64 clientId, quint64 requestId, QString token, int errorCode, QString errorMessage);
};

#endif // MAILSERVICECLIENT_H
