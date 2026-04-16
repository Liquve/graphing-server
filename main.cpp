#include <QCoreApplication>
#include "GraphingCalculation.h"
#include "GraphingErrorCode.h"
#include "TCPGraphingServerManager.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".UTF8");

    QCoreApplication qtApp(argc, argv);

    TCPGraphingServerManager manager(QHostAddress::AnyIPv4, 13579);

    QObject::connect(&manager, &TCPGraphingServerManager::commandRequested, &manager, [&manager](const GraphingServerRequest& request) {
        if (request.commandId == "login") {
            if (request.parameters.length() != 2) {
                manager.failRequest(
                    request.clientId,
                    request.requestId,
                    (int)GraphingErrorCode::BadRequest,
                    "Login requires 2 params: login|password"
                );
                return;
            }

            if (request.parameters[0] == "admin" && request.parameters[1] == "123456") {
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

        if (request.commandId == "register") {
            if (request.parameters.length() < 3 || request.parameters.length() > 4) {
                manager.failRequest(
                    request.clientId,
                    request.requestId,
                    (int)GraphingErrorCode::BadRequest,
                    "Register requires 3 or 4 params"
                );
                return;
            }

            manager.failRequest(
                request.clientId,
                request.requestId,
                (int)GraphingErrorCode::NotImplemented,
                "Registration not implemented"
            );
            return;
        }

        if (request.commandId == "calculate") {
            if (request.parameters.length() != 3) {
                manager.failRequest(
                    request.clientId,
                    request.requestId,
                    (int)GraphingErrorCode::BadRequest,
                    "Calculate requires 3 integer params"
                );
                return;
            }

            bool aOk;
            bool bOk;
            bool cOk;

            int a = request.parameters[0].toInt(&aOk);
            int b = request.parameters[1].toInt(&bOk);
            int c = request.parameters[2].toInt(&cOk);

            if (!aOk || !bOk || !cOk) {
                manager.failRequest(
                    request.clientId,
                    request.requestId,
                    (int)GraphingErrorCode::BadRequest,
                    "Calculate params must be integers"
                );
                return;
            }

            QString result = GraphingCalculation::getCalculationResult(a, b, c);
            manager.completeRequest(request.clientId, request.requestId, result.split("|"));
            return;
        }

        manager.failRequest(
            request.clientId,
            request.requestId,
            (int)GraphingErrorCode::NotImplemented,
            QString("Command \"%1\" is not implemented").arg(request.commandId)
        );
    }, Qt::QueuedConnection);

    try {
        manager.startServer();
    } catch (const std::exception& e) {
        qFatal() << QString("Fatal exception while trying to start TCP Server: %1").arg(e.what());
        return 1;
    }

    return qtApp.exec();
}
