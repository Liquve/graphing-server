#ifndef GRAPHINGMESSAGE_H
#define GRAPHINGMESSAGE_H

#include <QMetaType>
#include <QStringList>

enum class GraphingMessageType {
    Login,
    Register,
    Calculate
};

struct GraphingMessage {
    GraphingMessageType type;
    QStringList parameters;
};

Q_DECLARE_METATYPE(GraphingMessageType)
Q_DECLARE_METATYPE(GraphingMessage)

#endif // GRAPHINGMESSAGE_H
