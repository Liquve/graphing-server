#include <QCoreApplication>
#include "GraphingCalculation.h"
#include "GraphingErrorCode.h"
#include "TCPGraphingServerManager.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".UTF8");

    QCoreApplication qtApp(argc, argv);

    TCPGraphingServerManager manager(QHostAddress::AnyIPv4, 13579);

    manager.setLoginHook([](const QString& login, const QString& password) {
        // временная авторизация без базы данных по жёстко установленным значениям
        if (login == "admin" && password == "123456") {
            return FailableHookResult::ok();
        }

        return FailableHookResult::error((int)GraphingErrorCode::Forbidden, "Incorrect credentials");
    });
    manager.setRegistrationHook([](const QString& login, const QString& password, const QString& name, const QString& email) {
        return FailableHookResult::error((int)GraphingErrorCode::NotImplemented, "Registration not implemented");
    });

    manager.setCalculateFunction([](int a, int b, int c) {
        return GraphingCalculation::getCalculationResult(a, b, c);
    });

    try {
        manager.startServer();
    } catch (const std::exception& e) {
        qFatal() << QString("Fatal exception while trying to start TCP Server: %1").arg(e.what());
        return 1;
    }

    return qtApp.exec();
}
