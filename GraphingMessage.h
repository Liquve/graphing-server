#ifndef GRAPHINGMESSAGE_H
#define GRAPHINGMESSAGE_H

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

#endif // GRAPHINGMESSAGE_H
