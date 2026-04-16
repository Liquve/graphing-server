#include <QCoreApplication>
#include "GraphingCalculation.h"
#include "GraphingErrorCode.h"
#include "PgDatabase.h"
#include "TCPGraphingServerManager.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".UTF8");

    QCoreApplication qtApp(argc, argv);

    PgDatabase& database = PgDatabase::instance();
    FailableOperationResult databaseConnectionResult = database.connect();
    if (!databaseConnectionResult.success) {
        qFatal().noquote() << QString("Fatal database connection error: %1").arg(databaseConnectionResult.message);
        return 1;
    }

    TCPGraphingServerManager manager(QHostAddress::AnyIPv4, 13579);

    QObject::connect(&manager, &TCPGraphingServerManager::loginRequested, &manager, [&manager, &database](const GraphingLoginRequest& request) {
        FailableOperationResult result = database.login(request.login, request.password);
        if (result.success) {
            manager.completeLogin(request.clientId, request.requestId);
            return;
        }

        manager.failRequest(
            request.clientId,
            request.requestId,
            result.errorCode,
            result.message
        );
    }, Qt::QueuedConnection);

    QObject::connect(&manager, &TCPGraphingServerManager::registrationRequested, &manager, [&manager, &database](const GraphingRegistrationRequest& request) {
        FailableOperationResult result = database.registerUser(request.login, request.password, request.name, request.email);
        if (result.success) {
            manager.completeRequest(request.clientId, request.requestId);
            return;
        }

        manager.failRequest(request.clientId, request.requestId, result.errorCode, result.message);
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
