#ifndef GRAPHINGTCPSERVER_H
#define GRAPHINGTCPSERVER_H

#include <QByteArray>
#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QtNetwork>
#include "GraphingMessageParser.h"

struct GraphingServerRequest {
    quint64 clientId;
    QString clientDescription;
    quint64 requestId;
    QString commandId;
    QStringList parameters;
};

struct GraphingServerResponse {
    quint64 clientId;
    QString payload;
};

Q_DECLARE_METATYPE(GraphingServerRequest)
Q_DECLARE_METATYPE(GraphingServerResponse)

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
        QHash<quint64, QString> pendingCommands;
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
    void queueProtocolMessage(quint64, const GraphingMessage&);
    void queueErrorResponse(quint64, quint64, int, const QString&);
public:
    explicit TCPGraphingServerManager(QHostAddress, quint16, QObject *parent = nullptr);
    ~TCPGraphingServerManager();

    void startServer();
    void stopServer();
signals:
    void requestReceived(GraphingServerRequest);
    void commandRequested(GraphingServerRequest);
    void responseReady(GraphingServerResponse);
private slots:
    void onRemoteConnection();
    void onRemoteDisconnected(quint64);
    void onRemoteDataChunk(quint64);

    void onRequestReceived(GraphingServerRequest);
    void onResponseReady(GraphingServerResponse);
public slots:
    void completeRequest(quint64 clientId, quint64 askRequestId, const QStringList& responseParameters = {});
    void failRequest(quint64 clientId, quint64 askRequestId, int errorCode, const QString& errorMessage);
};

#endif // GRAPHINGTCPSERVER_H
