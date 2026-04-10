#ifndef FAILABLEHOOKRESULT_H
#define FAILABLEHOOKRESULT_H

#include <QString>

struct FailableHookResult {
    bool success;
    int errorCode;
    QString message;

    static FailableHookResult ok() {
        return {true, 0, ""};
    }

    static FailableHookResult error(int code, const QString& msg) {
        return {false, code, msg};
    }
};

#endif
