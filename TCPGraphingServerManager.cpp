#include "TCPGraphingServerManager.h"
#include "GraphingErrorCode.h"
#include <QDebug>

TCPGraphingServerManager::TCPGraphingServerManager(
    QHostAddress address,
    quint16 port,
    QObject *parent
) : QObject{parent}
{
    this->address = address;
    this->port = port;

    // Эти типы передаются через queued signal/slot соединения.
    // Q_DECLARE_METATYPE в заголовках недостаточно: для асинхронной доставки через очередь событий
    // Qt должен уметь зарегистрировать типы в runtime, чтобы копировать и переносить их между вызовами event loop.
    qRegisterMetaType<GraphingProtocol::MessageKind>("GraphingProtocol::MessageKind");
    qRegisterMetaType<GraphingProtocol::Message>("GraphingProtocol::Message");
    qRegisterMetaType<GraphingServerRequest>("GraphingServerRequest");
    qRegisterMetaType<GraphingAuthenticationRequest>("GraphingAuthenticationRequest");
    qRegisterMetaType<GraphingCalculationRequest>("GraphingCalculationRequest");
    qRegisterMetaType<GraphingServerResponse>("GraphingServerResponse");

    this->server = new QTcpServer(this); // мы родитель, освободится автоматически при вызове деструктора

    // Обработчики строят полностью асинхронный пайплайн через очередь событий Qt.
    connect(this->server, &QTcpServer::newConnection, this, &TCPGraphingServerManager::onRemoteConnection);
    connect(this, &TCPGraphingServerManager::requestReceived, this, &TCPGraphingServerManager::onRequestReceived, Qt::QueuedConnection);
    connect(this, &TCPGraphingServerManager::responseReady, this, &TCPGraphingServerManager::onResponseReady, Qt::QueuedConnection);
}

TCPGraphingServerManager::~TCPGraphingServerManager() {
    this->stopServer();
}

QString TCPGraphingServerManager::getListenDescription() const {
    return QString("%1:%2").arg(this->address.toString()).arg(this->port);
}

QString TCPGraphingServerManager::getSocketDescription(const QTcpSocket& socket) const {
    return QString("%1:%2").arg(socket.peerAddress().toString()).arg(socket.peerPort());
}

QString TCPGraphingServerManager::fromProtocolString(const std::string& value) {
    return QString::fromUtf8(value.c_str(), (qsizetype)value.size());
}

std::string TCPGraphingServerManager::toProtocolString(const QString& value) {
    QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), (std::size_t)utf8.size());
}

QStringList TCPGraphingServerManager::fromProtocolList(const std::vector<std::string>& values) {
    QStringList list;
    for (std::vector<std::string>::const_iterator it = values.begin(); it != values.end(); ++it) {
        list.append(fromProtocolString(*it));
    }

    return list;
}

quint64 TCPGraphingServerManager::registerClient(QTcpSocket* remoteClient) {
    quint64 clientId = this->nextClientId++;

    ClientSession session;
    session.socket = remoteClient;
    session.description = this->getSocketDescription(*remoteClient);

    this->clients.insert(clientId, session);

    return clientId;
}

void TCPGraphingServerManager::unregisterClient(quint64 clientId) {
    ClientSession* session = this->findClient(clientId);
    if (!session) {
        return;
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

void TCPGraphingServerManager::queueParsedMessage(quint64 clientId, const GraphingProtocol::Message& message) {
    ClientSession* session = this->findClient(clientId);
    if (!session || message.kind != GraphingProtocol::MessageKind::Request) {
        return;
    }

    emit this->requestReceived(GraphingServerRequest{
        clientId,
        session->description,
        message.correlationId,
        fromProtocolString(message.type),
        fromProtocolList(message.parameters)
    });
}

void TCPGraphingServerManager::queueProtocolMessage(quint64 clientId, const GraphingProtocol::Message& message) {
    emit this->responseReady(GraphingServerResponse{
        clientId,
        fromProtocolString(GraphingProtocol::serialize(message)).append("\n")
    });
}

void TCPGraphingServerManager::queueErrorResponse(quint64 clientId, quint64 askRequestId, int errorCode, const QString& errorMessage) {
    this->queueProtocolMessage(
        clientId,
        GraphingProtocol::Message::responseError(askRequestId, errorCode, toProtocolString(errorMessage))
    );
}

void TCPGraphingServerManager::startServer() {
    QString listenDescription = this->getListenDescription();

    qDebug().noquote().nospace() << "[#] Starting TCP Server on " << listenDescription << "...";

    if (this->server->listen(this->address, this->port)) {
        qInfo().noquote().nospace() << "[+] Started TCP Server on " << listenDescription;
    } else {
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
        << QString::fromUtf8(chunk).replace("\n", "\\n").replace("\r", "\\r")
        << "\"";

    session->buffer.append(chunk);

    while (true) {
        qsizetype delimiterPosition = session->buffer.indexOf('\n');
        if (delimiterPosition == -1)
            break;

        QString message = QString::fromUtf8(session->buffer.left(delimiterPosition));
        session->buffer.remove(0, delimiterPosition + 1);

        qDebug().noquote().nospace()
            << "[#] Raw message from "
            << session->description
            << ": \""
            << message
            << "\"";

        GraphingProtocol::Message typedMessage;
        try {
            typedMessage = GraphingProtocol::parse(toProtocolString(message));
        } catch (const std::exception& e) {
            qWarning().noquote().nospace()
                << "[-] Bad message from "
                << session->description
                << ": \""
                << message
                << "\"";

            this->queueProtocolMessage(
                clientId,
                GraphingProtocol::Message::responseError(
                    0,
                    (int)GraphingErrorCode::BadRequest,
                    toProtocolString(QString("Parsing error: %1").arg(e.what()))
                )
            );
            continue;
        }

        qInfo().noquote().nospace()
            << "[+] Message from "
            << session->description
            << ": "
            << fromProtocolString(GraphingProtocol::describe(typedMessage));

        if (typedMessage.kind != GraphingProtocol::MessageKind::Request) {
            this->queueErrorResponse(
                clientId,
                typedMessage.correlationId,
                (int)GraphingErrorCode::BadRequest,
                "Server accepts request messages only"
            );
            continue;
        }

        this->queueParsedMessage(clientId, typedMessage);
    }
}

void TCPGraphingServerManager::onRequestReceived(GraphingServerRequest request) {
    ClientSession* session = this->findClient(request.clientId);
    if (!session) {
        return;
    }

    if (session->pendingTypes.contains(request.requestId)) {
        this->queueErrorResponse(
            request.clientId,
            request.requestId,
            (int)GraphingErrorCode::Conflict,
            "Request with this requestId is already pending"
        );
        return;
    }

    if (request.type != "login" && request.type != "register" && !session->authenticated) {
        this->queueErrorResponse(
            request.clientId,
            request.requestId,
            (int)GraphingErrorCode::Forbidden,
            "Not authenticated"
        );
        return;
    }

    session->pendingTypes.insert(request.requestId, request.type);

    if (request.type == "login" || request.type == "register") {
        QString name = request.parameters.length() > 2 ? request.parameters[2] : QString();
        QString email = request.parameters.length() > 3 ? request.parameters[3] : QString();

        emit this->authenticationRequested(GraphingAuthenticationRequest{
            request.clientId,
            request.clientDescription,
            request.requestId,
            request.type,
            request.parameters.length() > 0 ? request.parameters[0] : QString(),
            request.parameters.length() > 1 ? request.parameters[1] : QString(),
            name,
            email
        });
        return;
    }

    if (request.type == "calculate") {
        bool aOk = false;
        bool bOk = false;
        bool cOk = false;

        int a = request.parameters.length() > 0 ? request.parameters[0].toInt(&aOk) : 0;
        int b = request.parameters.length() > 1 ? request.parameters[1].toInt(&bOk) : 0;
        int c = request.parameters.length() > 2 ? request.parameters[2].toInt(&cOk) : 0;

        if (request.parameters.length() != 3 || !aOk || !bOk || !cOk) {
            session->pendingTypes.remove(request.requestId);
            this->queueErrorResponse(
                request.clientId,
                request.requestId,
                (int)GraphingErrorCode::BadRequest,
                "Calculate requires 3 integer params"
            );
            return;
        }

        emit this->calculationRequested(GraphingCalculationRequest{
            request.clientId,
            request.clientDescription,
            request.requestId,
            request.type,
            a,
            b,
            c
        });
        return;
    }

    session->pendingTypes.remove(request.requestId);
    this->queueErrorResponse(
        request.clientId,
        request.requestId,
        (int)GraphingErrorCode::NotImplemented,
        QString("Type \"%1\" is not implemented").arg(request.type)
    );
}

void TCPGraphingServerManager::completeRequest(quint64 clientId, quint64 askRequestId, const QStringList& responseParameters) {
    ClientSession* session = this->findClient(clientId);
    if (!session) {
        return;
    }

    QHash<quint64, QString>::iterator pendingRequest = session->pendingTypes.find(askRequestId);
    if (pendingRequest == session->pendingTypes.end()) {
        return;
    }

    QString type = pendingRequest.value();
    session->pendingTypes.erase(pendingRequest);

    if (type == "login" || type == "register") {
        session->authenticated = true;
    }

    std::vector<std::string> protocolParameters;
    protocolParameters.reserve((std::size_t)responseParameters.length());
    for (QStringList::const_iterator it = responseParameters.begin(); it != responseParameters.end(); ++it) {
        protocolParameters.push_back(toProtocolString(*it));
    }

    this->queueProtocolMessage(clientId, GraphingProtocol::Message::responseSuccess(askRequestId, protocolParameters));
}

void TCPGraphingServerManager::failRequest(quint64 clientId, quint64 askRequestId, int errorCode, const QString& errorMessage) {
    ClientSession* session = this->findClient(clientId);
    if (!session) {
        return;
    }

    QHash<quint64, QString>::iterator pendingRequest = session->pendingTypes.find(askRequestId);
    if (pendingRequest == session->pendingTypes.end()) {
        return;
    }

    session->pendingTypes.erase(pendingRequest);
    this->queueErrorResponse(clientId, askRequestId, errorCode, errorMessage);
}

void TCPGraphingServerManager::onResponseReady(GraphingServerResponse response) {
    ClientSession* session = this->findClient(response.clientId);
    if (!session || !session->socket) {
        return;
    }

    if (session->socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    session->socket->write(response.payload.toUtf8());
}
