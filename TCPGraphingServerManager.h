#ifndef GRAPHINGTCPSERVER_H
#define GRAPHINGTCPSERVER_H

#include <QByteArray>
#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QtNetwork>
#include "FailableOperationResult.h"
#include "GraphingMessageParser.h"

using LoginHook = std::function<FailableOperationResult(const QString&, const QString&)>;
using RegistrationHook = std::function<FailableOperationResult(const QString&, const QString&, const QString&, const QString&)>;
using CalculateFunction = std::function<QString(int, int, int)>;

struct GraphingServerRequest {
    quint64 clientId;
    QString clientDescription;
    GraphingMessage message;
};

struct GraphingServerResponse {
    quint64 clientId;
    QString payload;
    bool updateAuthState;
    bool authenticated;
};

Q_DECLARE_METATYPE(GraphingServerRequest)
Q_DECLARE_METATYPE(GraphingServerResponse)

// данный класс управляет TCP-сервером, реализующим протокол с тремя командами: reg, auth, func
class TCPGraphingServerManager : public QObject
{
    Q_OBJECT
private:
    struct ClientSession {
        QPointer<QTcpSocket> socket;
        QByteArray buffer;
        bool authenticated = false;
        QString description;
    };

    QHostAddress address;
    quint16 port;
    quint64 nextClientId = 1;

    QTcpServer* server;
    GraphingMessageParser parser;

    QHash<quint64, ClientSession> clients;
    QHash<QTcpSocket*, quint64> socketClientIds;

    LoginHook loginHook;
    RegistrationHook registrationHook;
    CalculateFunction calculateFunction;

    QString getListenDescription() const;
    QString getSocketDescription(const QTcpSocket&) const;

    quint64 registerClient(QTcpSocket*);
    void unregisterClient(quint64);
    ClientSession* findClient(quint64);
    void queueParsedMessage(quint64, const GraphingMessage&);
    void queueResponse(quint64, const QString&);
    void queueAuthResponse(quint64, const QString&, bool);
public:

    explicit TCPGraphingServerManager(QHostAddress, quint16, QObject *parent = nullptr);
    ~TCPGraphingServerManager();

    void setLoginHook(LoginHook);
    void setRegistrationHook(RegistrationHook);
    void setCalculateFunction(CalculateFunction);

    void startServer();
    void stopServer();
signals:
    void requestReceived(GraphingServerRequest);
    void authenticationRequested(GraphingServerRequest);
    void calculationRequested(GraphingServerRequest);
    void responseReady(GraphingServerResponse);
private slots:
    void onRemoteConnection();
    void onRemoteDisconnected(quint64);
    void onRemoteDataChunk(quint64);

    void onRequestReceived(GraphingServerRequest);
    void onAuthenticationRequested(GraphingServerRequest);
    void onCalculationRequested(GraphingServerRequest);
    void onResponseReady(GraphingServerResponse);
};

#endif // GRAPHINGTCPSERVER_H
