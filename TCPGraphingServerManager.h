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

struct GraphingServerRequest {
    quint64 clientId;
    QString clientDescription;
    GraphingMessage message;
};

struct GraphingAuthenticationRequest {
    quint64 clientId;
    QString clientDescription;
    GraphingMessageType type;
    QString login;
    QString password;
    QString name;
    QString email;
};

struct GraphingCalculationRequest {
    quint64 clientId;
    QString clientDescription;
    int a;
    int b;
    int c;
};

struct GraphingServerResponse {
    quint64 clientId;
    QString payload;
    bool updateAuthState;
    bool authenticated;
};

Q_DECLARE_METATYPE(GraphingServerRequest)
Q_DECLARE_METATYPE(GraphingAuthenticationRequest)
Q_DECLARE_METATYPE(GraphingCalculationRequest)
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

    void startServer();
    void stopServer();
signals:
    void requestReceived(GraphingServerRequest);
    void authenticationRequested(GraphingAuthenticationRequest);
    void calculationRequested(GraphingCalculationRequest);
    void responseReady(GraphingServerResponse);
private slots:
    void onRemoteConnection();
    void onRemoteDisconnected(quint64);
    void onRemoteDataChunk(quint64);

    void onRequestReceived(GraphingServerRequest);
    void onResponseReady(GraphingServerResponse);
public slots:
    void completeAuthentication(quint64, FailableOperationResult, bool authenticated = true);
    void completeCalculation(quint64, QString);
};

#endif // GRAPHINGTCPSERVER_H
