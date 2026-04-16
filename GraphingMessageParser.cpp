#include "GraphingMessageParser.h"
#include <QStringList>

namespace {
    quint64 parseId(const QString& value, const QString& fieldName) {
        bool ok;
        qulonglong rawValue = value.toULongLong(&ok);
        if (!ok) {
            throw std::runtime_error(QString("%1 must be an unsigned integer").arg(fieldName).toStdString());
        }

        return rawValue;
    }

    QStringList parseParams(const QString& rawParams) {
        if (rawParams.isEmpty()) {
            return {};
        }

        return rawParams.split("|");
    }
}

GraphingMessage GraphingMessageParser::parse(QString message) {
    QStringList parts = message.split(" ", Qt::KeepEmptyParts);
    if (parts.length() < 3) {
        throw std::runtime_error("Incorrect graphing message format");
    }

    QString messageKind = parts[0];
    if (messageKind == "request") {
        if (parts.length() < 3 || parts.length() > 4) {
            throw std::runtime_error("Request syntax is: request <requestId> <commandId> [param1|param2|paramX...]");
        }

        quint64 requestId = parseId(parts[1], "requestId");
        QString commandId = parts[2];
        if (commandId.isEmpty()) {
            throw std::runtime_error("commandId cannot be empty");
        }

        QStringList parameters = parts.length() == 4 ? parseParams(parts[3]) : QStringList{};
        return GraphingMessage::request(requestId, commandId, parameters);
    }

    if (messageKind == "response") {
        if (parts.length() < 3 || parts.length() > 4) {
            throw std::runtime_error("Response syntax is: response <askRequestId> ok [param1|param2|paramX...] or response <askRequestId> error <code>|<message>");
        }

        quint64 askRequestId = parseId(parts[1], "askRequestId");
        QString responseKind = parts[2];

        if (responseKind == "ok") {
            QStringList parameters = parts.length() == 4 ? parseParams(parts[3]) : QStringList{};
            return GraphingMessage::responseSuccess(askRequestId, parameters);
        }

        if (responseKind == "error") {
            if (parts.length() != 4) {
                throw std::runtime_error("Error response syntax is: response <askRequestId> error <code>|<message>");
            }

            QStringList errorParts = parts[3].split("|", Qt::KeepEmptyParts);
            if (errorParts.length() != 2) {
                throw std::runtime_error("Error response must contain code and message");
            }

            bool ok;
            int errorCode = errorParts[0].toInt(&ok);
            if (!ok) {
                throw std::runtime_error("Error code must be integer");
            }

            return GraphingMessage::responseError(askRequestId, errorCode, errorParts[1]);
        }

        throw std::runtime_error("Response kind must be ok or error");
    }

    throw std::runtime_error("Message kind must be request or response");
}

QString GraphingMessageParser::serialize(const GraphingMessage& message) const {
    if (message.kind == GraphingMessageKind::Request) {
        QString base = QString("request %1 %2").arg(message.correlationId).arg(message.commandId);
        if (!message.parameters.isEmpty()) {
            base.append(QString(" %1").arg(message.parameters.join("|")));
        }

        return base;
    }

    if (message.success) {
        QString base = QString("response %1 ok").arg(message.correlationId);
        if (!message.parameters.isEmpty()) {
            base.append(QString(" %1").arg(message.parameters.join("|")));
        }

        return base;
    }

    return QString("response %1 error %2|%3")
        .arg(message.correlationId)
        .arg(message.errorCode)
        .arg(message.errorMessage);
}

QString GraphingMessageParser::getMessageDescription(const GraphingMessage& message) {
    if (message.kind == GraphingMessageKind::Request) {
        return QString("Request(id=%1, command=%2, params=[%3])")
            .arg(message.correlationId)
            .arg(message.commandId)
            .arg(message.parameters.join(", "));
    }

    if (message.success) {
        return QString("Response(askRequestId=%1, ok, params=[%2])")
            .arg(message.correlationId)
            .arg(message.parameters.join(", "));
    }

    return QString("Response(askRequestId=%1, error=%2, message=%3)")
        .arg(message.correlationId)
        .arg(message.errorCode)
        .arg(message.errorMessage);
}
