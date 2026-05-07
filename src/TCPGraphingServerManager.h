#ifndef GRAPHINGTCPSERVER_H
#define GRAPHINGTCPSERVER_H

#include <QByteArray>
#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QtNetwork>
#include "GraphingProtocol.h"

struct GraphingServerRequest {
    quint64 clientId;
    QString clientDescription;
    quint64 requestId;
    QString type;
    QStringList parameters;
};

struct GraphingLoginRequest {
    quint64 clientId;
    QString clientDescription;
    quint64 requestId;
    QString login;
    QString password;
};

struct GraphingRegistrationRequest {
    quint64 clientId;
    QString clientDescription;
    quint64 requestId;
    QString login;
    QString password;
    QString name;
    QString email;
};

struct GraphingCalculationRequest {
    quint64 clientId;
    QString clientDescription;
    quint64 requestId;
    QString type;
    int a;
    int b;
    int c;
};

struct GraphingPasswordResetRequest {
    quint64 clientId;
    QString clientDescription;
    quint64 requestId;
    QString loginOrEmail;
};

struct GraphingPasswordResetVerificationRequest {
    quint64 clientId;
    QString clientDescription;
    quint64 requestId;
    QString token;
    QString verificationCode;
};

struct GraphingPasswordResetUpdateRequest {
    quint64 clientId;
    QString clientDescription;
    quint64 requestId;
    QString token;
    QString newPassword;
};

struct GraphingServerResponse {
    quint64 clientId;
    QString payload;
};

Q_DECLARE_METATYPE(GraphingServerRequest)
Q_DECLARE_METATYPE(GraphingLoginRequest)
Q_DECLARE_METATYPE(GraphingRegistrationRequest)
Q_DECLARE_METATYPE(GraphingCalculationRequest)
Q_DECLARE_METATYPE(GraphingPasswordResetRequest)
Q_DECLARE_METATYPE(GraphingPasswordResetVerificationRequest)
Q_DECLARE_METATYPE(GraphingPasswordResetUpdateRequest)
Q_DECLARE_METATYPE(GraphingServerResponse)
Q_DECLARE_METATYPE(GraphingProtocol::MessageKind)
Q_DECLARE_METATYPE(GraphingProtocol::Message)

// Данный класс управляет TCP-сервером и маршрутизирует клиентские request-сообщения.
class TCPGraphingServerManager : public QObject
{
    Q_OBJECT
private:
    struct ClientSession {
        QPointer<QTcpSocket> socket;
        QByteArray buffer;
        bool authenticated = false;
        QString description;
        QHash<quint64, QString> pendingTypes;
    };

    QHostAddress address;
    quint16 port;
    quint64 nextClientId = 1;

    QTcpServer* server;

    QHash<quint64, ClientSession> clients;

    QString getListenDescription() const;
    QString getSocketDescription(const QTcpSocket&) const;
    static QString fromProtocolString(const std::string&);
    static std::string toProtocolString(const QString&);
    static QStringList fromProtocolList(const std::vector<std::string>&);
    static bool isSixDigitCode(const QString&);

    quint64 registerClient(QTcpSocket*);
    void unregisterClient(quint64);
    ClientSession* findClient(quint64);
    void queueParsedMessage(quint64, const GraphingProtocol::Message&);
    void queueProtocolMessage(quint64, const GraphingProtocol::Message&);
    void queueErrorResponse(quint64, quint64, int, const QString&);
public:
    explicit TCPGraphingServerManager(QHostAddress, quint16, QObject *parent = nullptr);
    ~TCPGraphingServerManager();

    void startServer();
    void stopServer();
signals:
    void requestReceived(GraphingServerRequest);
    void loginRequested(GraphingLoginRequest);
    void registrationRequested(GraphingRegistrationRequest);
    void calculationRequested(GraphingCalculationRequest);
    void passwordResetRequested(GraphingPasswordResetRequest);
    void passwordResetVerificationRequested(GraphingPasswordResetVerificationRequest);
    void passwordResetUpdateRequested(GraphingPasswordResetUpdateRequest);
    void responseReady(GraphingServerResponse);
private slots:
    void onRemoteConnection();
    void onRemoteDisconnected(quint64);
    void onRemoteDataChunk(quint64);

    void onRequestReceived(GraphingServerRequest);
    void onResponseReady(GraphingServerResponse);
public slots:
    void completeLogin(quint64 clientId, quint64 askRequestId, const QStringList& responseParameters = {});
    void completeRequest(quint64 clientId, quint64 askRequestId, const QStringList& responseParameters = {});
    void failRequest(quint64 clientId, quint64 askRequestId, int errorCode, const QString& errorMessage);
};

#endif // GRAPHINGTCPSERVER_H
