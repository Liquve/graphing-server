#ifndef FAILABLEOPERATIONRESULT_H
#define FAILABLEOPERATIONRESULT_H

#include <QString>

struct FailableOperationResult {
    bool success;
    int errorCode;
    QString message;

    static FailableOperationResult ok() {
        return {true, 0, ""};
    }

    static FailableOperationResult error(int code, const QString& message) {
        return {false, code, message};
    }
};

#endif
