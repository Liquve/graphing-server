#ifndef GRAPHINGMESSAGEPARSER_H
#define GRAPHINGMESSAGEPARSER_H

#include <QString>
#include "GraphingMessage.h"

class GraphingMessageParser
{
public:
    GraphingMessageParser() = default;

    GraphingMessage parse(QString);
    QString serialize(const GraphingMessage&) const;

    static QString getMessageDescription(const GraphingMessage&);
};

#endif // GRAPHINGMESSAGEPARSER_H
