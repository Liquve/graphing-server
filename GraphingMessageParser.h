#ifndef GRAPHINGMESSAGEPARSER_H
#define GRAPHINGMESSAGEPARSER_H

#include <QString>
#include "GraphingMessage.h"

class GraphingMessageParser
{
public:
    GraphingMessageParser() = default;

    GraphingMessage parse(QString message);

    static QString getMessageDescription(GraphingMessage message);
};

#endif // GRAPHINGMESSAGEPARSER_H
