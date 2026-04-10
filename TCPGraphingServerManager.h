#ifndef GRAPHINGTCPSERVER_H
#define GRAPHINGTCPSERVER_H

#include <QObject>
#include <QtNetwork>
#include "FailableOperationResult.h"
#include "GraphingMessageParser.h"

using LoginHook = std::function<FailableOperationResult(const QString&, const QString&)>;
using RegistrationHook = std::function<FailableOperationResult(const QString&, const QString&, const QString&, const QString&)>;
using CalculateFunction = std::function<QString(int, int, int)>;

// данный класс управляет TCP-сервером, реализующим протокол с тремя командами: reg, auth, func
class TCPGraphingServerManager : public QObject
{
    Q_OBJECT
private:
    QHostAddress address;
    quint16 port;

    QTcpServer* server;
    GraphingMessageParser parser;

    QList<QTcpSocket*> clients;
    // TCP не передаёт "сообщения", а передаёт потоки данных, для выделения сообщений данные надо сначала накопить в буфере
    QHash<QTcpSocket*, QByteArray> buffers;
    QHash<QTcpSocket*, bool> authStates;

    LoginHook loginHook;
    RegistrationHook registrationHook;
    CalculateFunction calculateFunction;

    QString getListenDescription();
    QString getSocketDescrption(QTcpSocket&);

    void handleMessage(QTcpSocket*, GraphingMessage);
public:

    explicit TCPGraphingServerManager(QHostAddress, quint16, QObject *parent = nullptr);
    ~TCPGraphingServerManager();

    void setLoginHook(LoginHook);
    void setRegistrationHook(RegistrationHook);
    void setCalculateFunction(CalculateFunction);

    void startServer();
    void stopServer();
private slots:
    void onRemoteConnection();

    void onRemoteDataChunk();
};

#endif // GRAPHINGTCPSERVER_H
