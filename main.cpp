#include <QCoreApplication>
#include "GraphingCalculation.h"
#include "GraphingErrorCode.h"
#include "TCPGraphingServerManager.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".UTF8");

    QCoreApplication qtApp(argc, argv);

    TCPGraphingServerManager manager(QHostAddress::AnyIPv4, 13579);

    QObject::connect(&manager, &TCPGraphingServerManager::authenticationRequested, &manager, [&manager](const GraphingAuthenticationRequest& request) {
        // временная авторизация без базы данных по жёстко установленным значениям
        if (request.type == GraphingMessageType::Login) {
            if (request.login == "admin" && request.password == "123456") {
                manager.completeAuthentication(request.clientId, FailableOperationResult::ok());
                return;
            }

            manager.completeAuthentication(
                request.clientId,
                FailableOperationResult::error((int)GraphingErrorCode::Forbidden, "Incorrect credentials"),
                false
            );
            return;
        }

        manager.completeAuthentication(
            request.clientId,
            FailableOperationResult::error((int)GraphingErrorCode::NotImplemented, "Registration not implemented"),
            false
        );
    }, Qt::QueuedConnection);

    QObject::connect(&manager, &TCPGraphingServerManager::calculationRequested, &manager, [&manager](const GraphingCalculationRequest& request) {
        manager.completeCalculation(request.clientId, GraphingCalculation::getCalculationResult(request.a, request.b, request.c));
    }, Qt::QueuedConnection);

    try {
        manager.startServer();
    } catch (const std::exception& e) {
        qFatal() << QString("Fatal exception while trying to start TCP Server: %1").arg(e.what());
        return 1;
    }

    return qtApp.exec();
}
