#include "GraphingMessageParser.h"
#include <QStringList>

GraphingMessage GraphingMessageParser::parse(QString message) {
    QStringList parts = message.split(" ");
    if (parts.length() != 2) {
        throw std::runtime_error("Incorrect graphing message format; syntax is <type> param1|param2|paramX...");
    }

    QString type = parts[0];

    QStringList params = parts[1].split("|");
    GraphingMessageType typedType;
    if (type == "login") {
        if (params.length() != 2) {
            throw std::runtime_error("Incorrect params length for login, length must be 2");
        }

        typedType = GraphingMessageType::Login;
    } else if (type == "register") {
        if (params.length() < 3 || params.length() > 4) {
            throw std::runtime_error("Incorrect params length for register, length must be 3 or 4");
        }

        typedType = GraphingMessageType::Register;
    } else if (type == "calculate") {
        if (params.length() != 3) {
            throw std::runtime_error("Incorrect params length for calculate, length must be 3");
        }

        typedType = GraphingMessageType::Calculate;
    } else {
        throw std::runtime_error("Incorrect graphing message type; types is login, register, calculate");
    }

    return GraphingMessage{typedType, params};
}

QString GraphingMessageParser::getMessageDescription(GraphingMessage message) {
    QString name;

    if (message.type == GraphingMessageType::Login) {
        name = "Login";
    } else if (message.type == GraphingMessageType::Register) {
        name = "Register";
    } else if (message.type == GraphingMessageType::Calculate) {
        name = "Calculate";
    }

    return QString("%1(%2)").arg(name, message.parameters.join(", "));
}