#ifndef GRAPHINGPROTOCOL_H
#define GRAPHINGPROTOCOL_H

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace GraphingProtocol {

enum class MessageKind {
    Request,
    Response
};

struct Message {
    MessageKind kind;
    std::uint64_t correlationId;
    std::string type;
    std::vector<std::string> parameters;
    bool success;
    int errorCode;
    std::string errorMessage;

    static Message request(std::uint64_t requestId, const std::string& type, const std::vector<std::string>& parameters = std::vector<std::string>()) {
        Message message;
        message.kind = MessageKind::Request;
        message.correlationId = requestId;
        message.type = type;
        message.parameters = parameters;
        message.success = false;
        message.errorCode = 0;
        return message;
    }

    static Message responseSuccess(std::uint64_t askRequestId, const std::vector<std::string>& parameters = std::vector<std::string>()) {
        Message message;
        message.kind = MessageKind::Response;
        message.correlationId = askRequestId;
        message.parameters = parameters;
        message.success = true;
        message.errorCode = 0;
        return message;
    }

    static Message responseError(std::uint64_t askRequestId, int errorCode, const std::string& errorMessage) {
        Message message;
        message.kind = MessageKind::Response;
        message.correlationId = askRequestId;
        message.success = false;
        message.errorCode = errorCode;
        message.errorMessage = errorMessage;
        return message;
    }
};

namespace detail {
    inline std::string trimLineEndings(const std::string& value) {
        std::string result = value;
        while (!result.empty() && (result[result.size() - 1] == '\n' || result[result.size() - 1] == '\r')) {
            result.erase(result.size() - 1);
        }

        return result;
    }

    inline bool isEscaped(const std::string& value, std::size_t position) {
        std::size_t slashCount = 0;
        while (position > 0) {
            --position;
            if (value[position] != '\\') {
                break;
            }

            ++slashCount;
        }

        return slashCount % 2 == 1;
    }

    inline std::vector<std::string> splitEscaped(const std::string& value, char delimiter) {
        std::vector<std::string> parts;
        std::string current;

        for (std::size_t i = 0; i < value.size(); ++i) {
            char symbol = value[i];
            if (symbol == delimiter && !isEscaped(value, i)) {
                parts.push_back(current);
                current.clear();
                continue;
            }

            current.push_back(symbol);
        }

        parts.push_back(current);
        return parts;
    }

    inline std::string decodeEscapes(const std::string& value) {
        std::string decoded;
        decoded.reserve(value.size());

        for (std::size_t i = 0; i < value.size(); ++i) {
            char symbol = value[i];
            if (symbol != '\\') {
                decoded.push_back(symbol);
                continue;
            }

            if (i + 1 >= value.size()) {
                throw std::runtime_error("Invalid trailing escape");
            }

            char escaped = value[++i];
            if (escaped == 'n') {
                decoded.push_back('\n');
            } else if (escaped == 'r') {
                decoded.push_back('\r');
            } else if (escaped == '\\') {
                decoded.push_back('\\');
            } else if (escaped == ' ') {
                decoded.push_back(' ');
            } else if (escaped == '|') {
                decoded.push_back('|');
            } else {
                throw std::runtime_error("Unsupported escape sequence");
            }
        }

        return decoded;
    }

    inline std::string encodeEscapes(const std::string& value) {
        std::string encoded;
        encoded.reserve(value.size());

        for (std::size_t i = 0; i < value.size(); ++i) {
            char symbol = value[i];
            if (symbol == '\n') {
                encoded.append("\\n");
            } else if (symbol == '\r') {
                encoded.append("\\r");
            } else if (symbol == '\\') {
                encoded.append("\\\\");
            } else if (symbol == ' ') {
                encoded.append("\\ ");
            } else if (symbol == '|') {
                encoded.append("\\|");
            } else {
                encoded.push_back(symbol);
            }
        }

        return encoded;
    }

    inline std::vector<std::string> decodeList(const std::string& raw) {
        std::vector<std::string> rawParts = splitEscaped(raw, '|');
        std::vector<std::string> decoded;
        decoded.reserve(rawParts.size());
        for (std::size_t i = 0; i < rawParts.size(); ++i) {
            decoded.push_back(decodeEscapes(rawParts[i]));
        }

        return decoded;
    }

    inline std::string encodeList(const std::vector<std::string>& values) {
        std::string encoded;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                encoded.push_back('|');
            }

            encoded.append(encodeEscapes(values[i]));
        }

        return encoded;
    }

    inline std::uint64_t parseUnsigned(const std::string& value, const char* fieldName) {
        if (value.empty()) {
            throw std::runtime_error(std::string(fieldName) + " cannot be empty");
        }

        std::uint64_t result = 0;
        for (std::size_t i = 0; i < value.size(); ++i) {
            char symbol = value[i];
            if (symbol < '0' || symbol > '9') {
                throw std::runtime_error(std::string(fieldName) + " must be an unsigned integer");
            }

            std::uint64_t digit = static_cast<std::uint64_t>(symbol - '0');
            if (result > (UINT64_MAX - digit) / 10) {
                throw std::runtime_error(std::string(fieldName) + " is too large");
            }

            result = result * 10 + digit;
        }

        return result;
    }

    inline int parseInt(const std::string& value, const char* fieldName) {
        if (value.empty()) {
            throw std::runtime_error(std::string(fieldName) + " cannot be empty");
        }

        std::stringstream stream(value);
        int result = 0;
        stream >> result;
        if (stream.fail() || !stream.eof()) {
            throw std::runtime_error(std::string(fieldName) + " must be an integer");
        }

        return result;
    }

    inline std::vector<std::string> splitSpaceTokens(const std::string& line) {
        std::vector<std::string> tokens;
        std::string current;

        for (std::size_t i = 0; i < line.size(); ++i) {
            char symbol = line[i];
            if (symbol == ' ' && !isEscaped(line, i)) {
                tokens.push_back(current);
                current.clear();
                continue;
            }

            current.push_back(symbol);
        }

        tokens.push_back(current);
        return tokens;
    }
}

inline bool tryParseCorrelationId(const std::string& rawLine, std::uint64_t& correlationId) {
    try {
        std::string line = detail::trimLineEndings(rawLine);
        std::vector<std::string> parts = detail::splitSpaceTokens(line);
        if (parts.size() < 2) {
            return false;
        }

        if (parts[0] != "request" && parts[0] != "response") {
            return false;
        }

        correlationId = detail::parseUnsigned(parts[1], "correlationId");
        return true;
    } catch (...) {
        return false;
    }
}

inline Message parse(const std::string& rawLine) {
    std::string line = detail::trimLineEndings(rawLine);
    std::vector<std::string> parts = detail::splitSpaceTokens(line);
    if (parts.size() < 3) {
        throw std::runtime_error("Incorrect graphing message format");
    }

    if (parts[0] == "request") {
        if (parts.size() < 3 || parts.size() > 4) {
            throw std::runtime_error("Request syntax is: request <requestId> <type> [param1|param2|paramX...]");
        }

        std::uint64_t requestId = detail::parseUnsigned(parts[1], "requestId");
        std::string type = detail::decodeEscapes(parts[2]);
        if (type.empty()) {
            throw std::runtime_error("type cannot be empty");
        }

        std::vector<std::string> parameters;
        if (parts.size() == 4) {
            parameters = detail::decodeList(parts[3]);
        }

        return Message::request(requestId, type, parameters);
    }

    if (parts[0] == "response") {
        if (parts.size() < 3 || parts.size() > 4) {
            throw std::runtime_error("Response syntax is: response <askRequestId> ok [param1|param2|paramX...] or response <askRequestId> error <code>|<message>");
        }

        std::uint64_t askRequestId = detail::parseUnsigned(parts[1], "askRequestId");
        if (parts[2] == "ok") {
            std::vector<std::string> parameters;
            if (parts.size() == 4) {
                parameters = detail::decodeList(parts[3]);
            }

            return Message::responseSuccess(askRequestId, parameters);
        }

        if (parts[2] == "error") {
            if (parts.size() != 4) {
                throw std::runtime_error("Error response syntax is: response <askRequestId> error <code>|<message>");
            }

            std::vector<std::string> errorParts = detail::decodeList(parts[3]);
            if (errorParts.size() != 2) {
                throw std::runtime_error("Error response must contain code and message");
            }

            int errorCode = detail::parseInt(errorParts[0], "errorCode");
            return Message::responseError(askRequestId, errorCode, errorParts[1]);
        }

        throw std::runtime_error("Response kind must be ok or error");
    }

    throw std::runtime_error("Message kind must be request or response");
}

inline std::string serialize(const Message& message) {
    if (message.kind == MessageKind::Request) {
        std::stringstream stream;
        stream << "request " << message.correlationId << " " << detail::encodeEscapes(message.type);
        if (!message.parameters.empty()) {
            stream << " " << detail::encodeList(message.parameters);
        }

        return stream.str();
    }

    if (message.success) {
        std::stringstream stream;
        stream << "response " << message.correlationId << " ok";
        if (!message.parameters.empty()) {
            stream << " " << detail::encodeList(message.parameters);
        }

        return stream.str();
    }

    std::vector<std::string> errorPayload;
    errorPayload.push_back(std::to_string(message.errorCode));
    errorPayload.push_back(message.errorMessage);

    std::stringstream stream;
    stream << "response " << message.correlationId << " error " << detail::encodeList(errorPayload);
    return stream.str();
}

inline std::string describe(const Message& message) {
    std::stringstream stream;

    if (message.kind == MessageKind::Request) {
        stream << "Request(id=" << message.correlationId << ", type=" << message.type << ", params=[";
        for (std::size_t i = 0; i < message.parameters.size(); ++i) {
            if (i > 0) {
                stream << ", ";
            }

            stream << message.parameters[i];
        }

        stream << "])";
        return stream.str();
    }

    if (message.success) {
        stream << "Response(askRequestId=" << message.correlationId << ", ok, params=[";
        for (std::size_t i = 0; i < message.parameters.size(); ++i) {
            if (i > 0) {
                stream << ", ";
            }

            stream << message.parameters[i];
        }

        stream << "])";
        return stream.str();
    }

    stream << "Response(askRequestId=" << message.correlationId << ", error=" << message.errorCode << ", message=" << message.errorMessage << ")";
    return stream.str();
}

} // namespace GraphingProtocol

#endif // GRAPHINGPROTOCOL_H
