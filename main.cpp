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
        if (request.type == "login") {
            if (request.login == "admin" && request.password == "123456") {
                manager.completeRequest(request.clientId, request.requestId);
                return;
            }

            manager.failRequest(
                request.clientId,
                request.requestId,
                (int)GraphingErrorCode::Forbidden,
                "Incorrect credentials"
            );
            return;
        }

        if (request.type == "register") {
            manager.failRequest(
                request.clientId,
                request.requestId,
                (int)GraphingErrorCode::NotImplemented,
                "Registration not implemented"
            );
            return;
        }
    }, Qt::QueuedConnection);

    QObject::connect(&manager, &TCPGraphingServerManager::calculationRequested, &manager, [&manager](const GraphingCalculationRequest& request) {
        QString result = GraphingCalculation::getCalculationResult(request.a, request.b, request.c);
        manager.completeRequest(request.clientId, request.requestId, result.split("|"));
    }, Qt::QueuedConnection);

    try {
        manager.startServer();
    } catch (const std::exception& e) {
        qFatal() << QString("Fatal exception while trying to start TCP Server: %1").arg(e.what());
        return 1;
    }

    return qtApp.exec();
}
