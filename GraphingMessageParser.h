#ifndef GRAPHINGMESSAGEPARSER_H
#define GRAPHINGMESSAGEPARSER_H

#include <QString>
#include "GraphingMessage.h"

class GraphingMessageParser
{
public:
    GraphingMessageParser() = default;

    GraphingMessage parse(QString);

    static QString getMessageDescription(GraphingMessage);
};

#endif // GRAPHINGMESSAGEPARSER_H
