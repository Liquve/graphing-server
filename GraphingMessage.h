#ifndef GRAPHINGMESSAGE_H
#define GRAPHINGMESSAGE_H

#include <QMetaType>
#include <QString>
#include <QStringList>

enum class GraphingMessageKind {
    Request,
    Response
};

struct GraphingMessage {
    GraphingMessageKind kind;
    quint64 correlationId;
    QString commandId;
    QStringList parameters;
    bool success;
    int errorCode;
    QString errorMessage;

    static GraphingMessage request(quint64 requestId, const QString& commandId, const QStringList& parameters = {}) {
        return {GraphingMessageKind::Request, requestId, commandId, parameters, false, 0, ""};
    }

    static GraphingMessage responseSuccess(quint64 askRequestId, const QStringList& parameters = {}) {
        return {GraphingMessageKind::Response, askRequestId, "", parameters, true, 0, ""};
    }

    static GraphingMessage responseError(quint64 askRequestId, int errorCode, const QString& errorMessage) {
        return {GraphingMessageKind::Response, askRequestId, "", {}, false, errorCode, errorMessage};
    }
};

Q_DECLARE_METATYPE(GraphingMessageKind)
Q_DECLARE_METATYPE(GraphingMessage)

#endif // GRAPHINGMESSAGE_H
