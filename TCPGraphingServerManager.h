#ifndef GRAPHINGTCPSERVER_H
#define GRAPHINGTCPSERVER_H

#include <QObject>
#include <QtNetwork>
#include "FailableHookResult.h"
#include "GraphingMessageParser.h"

using AuthenticationHook = std::function<FailableHookResult(const QString&, const QString&)>;

// данный класс управляет TCP-сервером, реализующим протокол с тремя командами: reg, auth, func
class TCPGraphingServerManager : public QObject
{
    Q_OBJECT
private:
    QHostAddress address;
    quint16 port;

    QTcpServer* server;
    GraphingMessageParser* parser;

    // TCP не передаёт "сообщения", а передаёт потоки данных, для выделения сообщений данные надо сначала накопить в буфере
    QHash<QTcpSocket*, QByteArray> buffers;

    AuthenticationHook loginHook;
    AuthenticationHook registrationHook;

    QString getListenDescription();
    QString getSocketDescrption(QTcpSocket& socket);
public:
    explicit TCPGraphingServerManager(QHostAddress address, quint16 port, QObject *parent = nullptr);
    ~TCPGraphingServerManager();

    void startServer();
    void stopServer();

    void setLoginHook(AuthenticationHook hook);
    void setRegistrationHook(AuthenticationHook hook);
private slots:
    void onRemoteConnection();

    void onRemoteDataChunk();
};

#endif // GRAPHINGTCPSERVER_H
