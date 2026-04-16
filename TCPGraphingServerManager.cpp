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

    qRegisterMetaType<GraphingMessageType>("GraphingMessageType");
    qRegisterMetaType<GraphingMessage>("GraphingMessage");
    qRegisterMetaType<GraphingServerRequest>("GraphingServerRequest");
    qRegisterMetaType<GraphingServerResponse>("GraphingServerResponse");

    this->server = new QTcpServer(this); // мы родитель, освободится автоматически при вызове деструктора

    // обработчики строят полностью асинхронный пайплайн через очередь событий Qt
    connect(this->server, &QTcpServer::newConnection, this, &TCPGraphingServerManager::onRemoteConnection);
    connect(this, &TCPGraphingServerManager::requestReceived, this, &TCPGraphingServerManager::onRequestReceived, Qt::QueuedConnection);
    connect(this, &TCPGraphingServerManager::authenticationRequested, this, &TCPGraphingServerManager::onAuthenticationRequested, Qt::QueuedConnection);
    connect(this, &TCPGraphingServerManager::calculationRequested, this, &TCPGraphingServerManager::onCalculationRequested, Qt::QueuedConnection);
    connect(this, &TCPGraphingServerManager::responseReady, this, &TCPGraphingServerManager::onResponseReady, Qt::QueuedConnection);
}

TCPGraphingServerManager::~TCPGraphingServerManager() {
    // выключим сокет на всякий случай, если он неактивен ничего плохого не будет
    this->stopServer();
}

QString TCPGraphingServerManager::getListenDescription() const {
    return QString("%1:%2").arg(this->address.toString()).arg(this->port);
}

QString TCPGraphingServerManager::getSocketDescription(const QTcpSocket& socket) const {
    return QString("%1:%2").arg(socket.peerAddress().toString()).arg(socket.peerPort());
}

quint64 TCPGraphingServerManager::registerClient(QTcpSocket* remoteClient) {
    quint64 clientId = this->nextClientId++;

    ClientSession session;
    session.socket = remoteClient;
    session.description = this->getSocketDescription(*remoteClient);

    this->clients.insert(clientId, session);
    this->socketClientIds.insert(remoteClient, clientId);

    return clientId;
}

void TCPGraphingServerManager::unregisterClient(quint64 clientId) {
    ClientSession* session = this->findClient(clientId);
    if (!session) {
        return;
    }

    QTcpSocket* remoteClient = session->socket.data();
    if (remoteClient) {
        this->socketClientIds.remove(remoteClient);
    }

    this->clients.remove(clientId);
}

TCPGraphingServerManager::ClientSession* TCPGraphingServerManager::findClient(quint64 clientId) {
    QHash<quint64, ClientSession>::iterator sessionIterator = this->clients.find(clientId);
    if (sessionIterator == this->clients.end()) {
        return nullptr;
    }

    return &sessionIterator.value();
}

void TCPGraphingServerManager::queueParsedMessage(quint64 clientId, const GraphingMessage& message) {
    ClientSession* session = this->findClient(clientId);
    if (!session) {
        return;
    }

    emit this->requestReceived(GraphingServerRequest{clientId, session->description, message});
}

void TCPGraphingServerManager::queueResponse(quint64 clientId, const QString& payload) {
    emit this->responseReady(GraphingServerResponse{clientId, payload, false, false});
}

void TCPGraphingServerManager::queueAuthResponse(quint64 clientId, const QString& payload, bool authenticated) {
    emit this->responseReady(GraphingServerResponse{clientId, payload, true, authenticated});
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
    QList<quint64> clientIds = this->clients.keys();
    for (QList<quint64>::iterator it = clientIds.begin(); it != clientIds.end(); ++it)
    {
        ClientSession* session = this->findClient(*it);
        if (!session || !session->socket) {
            this->unregisterClient(*it);
            continue;
        }

        session->socket->close();
        session->socket->deleteLater();
        this->unregisterClient(*it);
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
        quint64 clientId = this->registerClient(remoteClient);

        connect(remoteClient, &QAbstractSocket::disconnected, this, [this, clientId]() {
            this->onRemoteDisconnected(clientId);
        });
        connect(remoteClient, &QIODevice::readyRead, this, [this, clientId]() {
            this->onRemoteDataChunk(clientId);
        });

        ClientSession* session = this->findClient(clientId);
        if (session) {
            qInfo().noquote() << "[+] New connection:" << session->description;
        }
    }
}

void TCPGraphingServerManager::onRemoteDisconnected(quint64 clientId) {
    ClientSession* session = this->findClient(clientId);
    if (!session) {
        return;
    }

    qInfo().noquote() << "[-] Disconnected" << session->description;

    if (session->socket) {
        session->socket->deleteLater();
    }

    this->unregisterClient(clientId);
}

void TCPGraphingServerManager::onRemoteDataChunk(quint64 clientId) {
    ClientSession* session = this->findClient(clientId);
    if (!session || !session->socket) {
        return;
    }

    QByteArray chunk = session->socket->readAll();

    qDebug().noquote().nospace()
        << "[#] New chunk from "
        << session->description
        << ": \""
        << QString::fromUtf8(chunk).replace("\n", "\\n").replace("\r", "\\r") // экранируем чтобы в логах было понятнее
        << "\"";

    session->buffer.append(chunk);

    // TCP-поток идёт непрерывно и нам требуется установить границу логического сообщения => это будет новая строка
    while (true) {
        qsizetype delimiterPosition = session->buffer.indexOf('\n');
        if (delimiterPosition == -1) // нет такого символа, сообщение не завершено
            break;

        QString message = QString::fromUtf8(session->buffer.left(delimiterPosition));
        session->buffer.remove(0, delimiterPosition + 1);
        // нашли логическое сообщение => удалили из буфера, декодировали в строку

        qDebug().noquote().nospace()
            << "[#] Raw message from "
            << session->description
            << ": \""
            << message
            << "\"";

        GraphingMessage typedMessage;
        try {
            typedMessage = this->parser.parse(message);
        } catch (const std::exception& e) {
            qWarning().noquote().nospace()
                << "[-] Bad message from "
                << session->description
                << ": \""
                << message
                << "\"";

            this->queueResponse(clientId, QString("Parsing error: %1\n").arg(e.what()));
            continue;
        }

        qInfo().noquote().nospace()
            << "[+] Message from "
            << session->description
            << ": "
            << GraphingMessageParser::getMessageDescription(typedMessage);

        this->queueParsedMessage(clientId, typedMessage);
    }
}

void TCPGraphingServerManager::onRequestReceived(GraphingServerRequest request) {
    if (request.message.type == GraphingMessageType::Login || request.message.type == GraphingMessageType::Register) {
        emit this->authenticationRequested(request);
    } else if (request.message.type == GraphingMessageType::Calculate) {
        emit this->calculationRequested(request);
    } else {
        this->queueResponse(request.clientId, "Handling error: cannot determine message type\n");
    }
}

void TCPGraphingServerManager::onAuthenticationRequested(GraphingServerRequest request) {
    ClientSession* session = this->findClient(request.clientId);
    if (!session) {
        return;
    }

    FailableOperationResult result;
    if (session->authenticated) {
        result = FailableOperationResult::error((int)GraphingErrorCode::Conflict, "Already authenticated");
    } else if (request.message.type == GraphingMessageType::Login) {
        if (!this->loginHook) {
            result = FailableOperationResult::error((int)GraphingErrorCode::NotImplemented, "Login hook is not defined");
        } else {
            result = this->loginHook(request.message.parameters[0], request.message.parameters[1]);
        }
    } else if (request.message.type == GraphingMessageType::Register) {
        if (!this->registrationHook) {
            result = FailableOperationResult::error((int)GraphingErrorCode::NotImplemented, "Registration hook is not defined");
        } else {
            QString email = request.message.parameters.length() > 3 ? request.message.parameters[3] : QString();
            result = this->registrationHook(
                request.message.parameters[0],
                request.message.parameters[1],
                request.message.parameters[2],
                email
            );
        }
    } else {
        result = FailableOperationResult::error((int)GraphingErrorCode::Conflict, "Unsupported authentication request");
    }

    if (result.success) {
        this->queueAuthResponse(request.clientId, "OK\n", true);
    } else {
        this->queueResponse(
            request.clientId,
            QString("Authentication error (%1): %2\n").arg(result.errorCode).arg(result.message)
        );
    }
}

void TCPGraphingServerManager::onCalculationRequested(GraphingServerRequest request) {
    ClientSession* session = this->findClient(request.clientId);
    if (!session) {
        return;
    }

    if (!session->authenticated) {
        this->queueResponse(request.clientId, "Calculation error: not authenticated\n");
        return;
    }

    if (!this->calculateFunction) {
        this->queueResponse(request.clientId, "Calculation errror: calculation not supported\n");
        return;
    }

    bool aConverseOK;
    bool bConverseOK;
    bool cConverseOK;

    int a = request.message.parameters[0].toInt(&aConverseOK);
    int b = request.message.parameters[1].toInt(&bConverseOK);
    int c = request.message.parameters[2].toInt(&cConverseOK);

    if (!aConverseOK || !bConverseOK || !cConverseOK) {
        this->queueResponse(request.clientId, "Calculation error: a, b, c must be integers\n");
        return;
    }

    QString result = this->calculateFunction(a, b, c);
    this->queueResponse(request.clientId, result.append("\n"));
}

void TCPGraphingServerManager::onResponseReady(GraphingServerResponse response) {
    ClientSession* session = this->findClient(response.clientId);
    if (!session || !session->socket) {
        return;
    }

    if (response.updateAuthState) {
        session->authenticated = response.authenticated;
    }

    if (session->socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    session->socket->write(response.payload.toUtf8());
}
