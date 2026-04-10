#include "TCPGraphingServerManager.h"
#include "GraphingErrorCode.h"
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

    this->server = new QTcpServer(this); // мы родитель, освободится автоматически при вызове деструктора

    // устанавливаем обработчик нового соединения, функция будет вызвана в контексте этого объекта (this) при сигнале newConnection
    connect(this->server, &QTcpServer::newConnection, this, &TCPGraphingServerManager::onRemoteConnection);
}

TCPGraphingServerManager::~TCPGraphingServerManager() {
    // выключим сокет на всякий случай, если он неактивен ничего плохого не будет
    this->stopServer();
}

QString TCPGraphingServerManager::getListenDescription() {
    return QString("%1:%2").arg(this->address.toString()).arg(this->port);
}

QString TCPGraphingServerManager::getSocketDescrption(QTcpSocket& socket) {
    return QString("%1:%2").arg(socket.peerAddress().toString()).arg(socket.peerPort());
}

void TCPGraphingServerManager::setLoginHook(LoginHook hook) {
    this->loginHook = hook;
}

void TCPGraphingServerManager::setRegistrationHook(RegistrationHook hook) {
    this->registrationHook = hook;
}

void TCPGraphingServerManager::setCalculateFunction(CalculateFunction function) {
    this->calculateFunction = function;
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
    // закроем все соединения
    for (QList<QTcpSocket*>::iterator it = this->clients.begin(); it != this->clients.end(); it++)
    {
        QTcpSocket* remoteClient = *it;

        remoteClient->close();
        remoteClient->deleteLater();
    }

    if (this->server) {
        this->server->close();
    }
}

void TCPGraphingServerManager::onRemoteConnection() {
    if (!this->server->hasPendingConnections()) {
        qWarning() << "[?] Remote connection received, but no pending connections";
        return;
    }

    while (this->server->hasPendingConnections()) {
        QTcpSocket* remoteClient = this->server->nextPendingConnection();

        this->clients.append(remoteClient);

        connect(remoteClient, &QAbstractSocket::disconnected, remoteClient, [this, remoteClient]() {
            qInfo().noquote() << "[-] Disconnected" << this->getSocketDescrption(*remoteClient);

            this->clients.removeAll(remoteClient);
            this->buffers.remove(remoteClient);
            this->authStates.remove(remoteClient);

            remoteClient->deleteLater();
        }); // лямбда это анонимная функция, мы передаём обработчик в виде лямбды, и захватываем в неё this и remoteClient (будем их использовать)
        connect(remoteClient, &QIODevice::readyRead, this, &TCPGraphingServerManager::onRemoteDataChunk);

        QString remoteClientDescription = QString("%1:%2").arg(remoteClient->peerAddress().toString()).arg(remoteClient->peerPort());
        qInfo().noquote() << "[+] New connection:" << remoteClientDescription;
    }
}

void TCPGraphingServerManager::onRemoteDataChunk() {
    QTcpSocket* remoteClient = qobject_cast<QTcpSocket*>(sender());
    if (!remoteClient) {
        qWarning() << "[?] Remote data chunk received, but no sender-as-socket";
        return;
    }
    QString remoteClientDescription = this->getSocketDescrption(*remoteClient);

    QByteArray &buffer = this->buffers[remoteClient];
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
        qsizetype delimiterPosition = buffer.indexOf('\n');
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
            typedMessage = this->parser.parse(message);
        } catch (const std::exception& e) {
            qWarning().noquote().nospace()
                << "[-] Bad message from "
                << remoteClientDescription
                << ": \""
                << message
                << "\"";

            remoteClient->write(QString("Parsing error: %1\n").arg(e.what()).toUtf8());
            continue;
        }

        qInfo().noquote().nospace()
            << "[+] Message from "
            << remoteClientDescription
            << ": "
            << GraphingMessageParser::getMessageDescription(typedMessage);

        this->handleMessage(remoteClient, typedMessage);
    }
}

void TCPGraphingServerManager::handleMessage(QTcpSocket* remoteClient, GraphingMessage message) {
    if (message.type == GraphingMessageType::Login || message.type == GraphingMessageType::Register) {
        FailableOperationResult result;
        if (this->authStates[remoteClient]) {
            result = FailableOperationResult::error((int)GraphingErrorCode::Conflict, "Already authenticated");
        } else if (message.type == GraphingMessageType::Login) {
            if (!this->loginHook) {
                result = FailableOperationResult::error((int)GraphingErrorCode::NotImplemented, "Login hook is not defined");
            } else {
                result = this->loginHook(message.parameters[0], message.parameters[1]);
            }
        } else if (message.type == GraphingMessageType::Register) {
            if (!this->registrationHook) {
                result = FailableOperationResult::error((int)GraphingErrorCode::NotImplemented, "Registration hook is not defined");
            } else {
                result = this->registrationHook(message.parameters[0], message.parameters[1], message.parameters[2], message.parameters[3]);
            }
        }

        if (result.success) {
            this->authStates[remoteClient] = true;
            remoteClient->write("OK\n");
        } else {
            remoteClient->write(QString("Authentication error (%1): %2\n").arg(result.errorCode).arg(result.message).toUtf8());
            return;
        }
    } else if (message.type == GraphingMessageType::Calculate) {
        if (!this->authStates[remoteClient]) {
            remoteClient->write("Calculation error: not authenticated\n");
            return;
        } else if (!this->calculateFunction) {
            remoteClient->write("Calculation errror: calculation not supported\n");
            return;
        }

        bool aConverseOK;
        bool bConverseOK;
        bool cConverseOK;

        int a = message.parameters[0].toInt(&aConverseOK);
        int b = message.parameters[1].toInt(&bConverseOK);
        int c = message.parameters[2].toInt(&cConverseOK);

        if (!aConverseOK || !bConverseOK || !cConverseOK) {
            remoteClient->write("Calculation error: a, b, c must be integers\n");
            return;
        }

        QString result = this->calculateFunction(a, b, c);
        remoteClient->write(result.append("\n").toUtf8());
    } else {
        remoteClient->write("Handling error: cannot determine message type\n");
    }
}