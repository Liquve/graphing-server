#include "TCPGraphingServerManager.h"
#include "GraphingMessage.h"
#include <QDebug>

TCPGraphingServerManager::TCPGraphingServerManager(
    QHostAddress address,
    quint16 port,
    QObject *parent
) : QObject{parent}
{
    this->address = address;
    this->port = port;

    this->server = new QTcpServer(this);
    this->parser = new GraphingMessageParser();

    // устанавливаем обработчик нового соединения, функция будет вызвана в контексте этого объекта (this) при сигнале newConnection
    connect(this->server, &QTcpServer::newConnection, this, &TCPGraphingServerManager::onRemoteConnection);
}

TCPGraphingServerManager::~TCPGraphingServerManager() {
    // выключим сокет на всякий случай, если он неактивен ничего плохого не будет
    this->stopServer();
    delete this->parser;
}

QString TCPGraphingServerManager::getListenDescription() {
    return QString("%1:%2").arg(this->address.toString()).arg(this->port);
}

QString TCPGraphingServerManager::getSocketDescrption(QTcpSocket& socket) {
    return QString("%1:%2").arg(socket.peerAddress().toString()).arg(socket.peerPort());
}

void TCPGraphingServerManager::startServer() {
    QString listenDescription = this->getListenDescription();

    qDebug().noquote().nospace() << "[#] Starting TCP Server on " << listenDescription << "...";

    if (this->server->listen(this->address, this->port)) {
        qInfo().noquote().nospace() << "[+] Started TCP Server on " << listenDescription;
    } else {
        // эта ошибка заставит программу вылететь, если она не обработана и покажет понятный текст
        throw std::runtime_error(
            QString("Cannot start TCP Server (%1): %2")
                .arg(this->server->serverError())
                .arg(this->server->errorString())
                .toStdString()
            );
    }
}

void TCPGraphingServerManager::stopServer() {
    if (this->server) {
        this->server->close();
    }
}

void TCPGraphingServerManager::onRemoteConnection() {
    if (!this->server->hasPendingConnections()) {
        qWarning() << "[?] Remote connection received, but no pending connections";
        return;
    }

    QTcpSocket* remoteClient = this->server->nextPendingConnection();
    connect(remoteClient, &QAbstractSocket::disconnected, remoteClient, [this, remoteClient]() {
        qInfo().noquote() << "[-] Disconnected" << this->getSocketDescrption(*remoteClient);

        this->buffers.remove(remoteClient);
        remoteClient->deleteLater();
    }); // лямбда это анонимная функция, мы передаём обработчик в виде лямбды, и захватываем в неё this и remoteClient (будем их использовать)
    connect(remoteClient, &QIODevice::readyRead, this, &TCPGraphingServerManager::onRemoteDataChunk);
    this->buffers[remoteClient] = QByteArray();

    QString remoteClientDescription = QString("%1:%2").arg(remoteClient->peerAddress().toString()).arg(remoteClient->peerPort());
    qInfo().noquote() << "[+] New connection:" << remoteClientDescription;
}

void TCPGraphingServerManager::onRemoteDataChunk() {
    QTcpSocket* remoteClient = qobject_cast<QTcpSocket*>(sender());
    if (!remoteClient) {
        qWarning() << "[?] Remote data chunk received, but no sender-as-socket";
        return;
    }
    QString remoteClientDescription = this->getSocketDescrption(*remoteClient);

    QHash<QTcpSocket*, QByteArray>::iterator it = this->buffers.find(remoteClient);
    if (it == buffers.end()) {
        qWarning() << "[?] Remote data chunk received, but no created buffer";
        return;
    }

    QByteArray &buffer = it.value();
    QByteArray chunk = remoteClient->readAll();

    qDebug().noquote().nospace()
        << "[#] New chunk from "
        << remoteClientDescription
        << ": \""
        << QString::fromUtf8(chunk).replace("\n", "\\n").replace("\r", "\\r") // экранируем чтобы в логах было понятнее
        << "\"";

    buffer.append(chunk);

    // TCP-поток идёт непрерывно и нам требуется установить границу логического сообщения => это будет новая строка
    while (true) {
        int delimiterPosition = buffer.indexOf('\n');
        if (delimiterPosition == -1) // нет такого символа, сообщение не завершено
            break;

        QString message = QString::fromUtf8(buffer.left(delimiterPosition));
        buffer.remove(0, delimiterPosition + 1);
        // нашли логическое сообщение => удалили из буфера, декодировали в строку

        qDebug().noquote().nospace()
            << "[#] Raw message from "
            << remoteClientDescription
            << ": \""
            << message
            << "\"";

        GraphingMessage typedMessage;
        try {
            typedMessage = this->parser->parse(message);
        } catch (const std::exception& e) {
            qWarning().noquote().nospace()
                << "[-] Bad message from "
                << remoteClientDescription
                << ": \""
                << message
                << "\"";

            remoteClient->write(QString("Parse error: %1\n").arg(e.what()).toStdString().c_str());
            return;
        }

        qInfo().noquote().nospace()
            << "[+] Message from "
            << remoteClientDescription
            << ": "
            << GraphingMessageParser::getMessageDescription(typedMessage);

        remoteClient->write("OK\n");
    }
}