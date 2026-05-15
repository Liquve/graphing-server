#include <QCoreApplication>
#include <QDebug>
#include "GraphingCalculation.h"
#include "GraphingErrorCode.h"
#include "MailServiceClient.h"
#include "PgDatabase.h"
#include "TCPGraphingServerManager.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".UTF8");

    QCoreApplication qtApp(argc, argv);

    PgDatabase& database = PgDatabase::instance();
    FailableOperationResult databaseConnectionResult = database.connect();
    if (!databaseConnectionResult.success) {
        qCritical().noquote() << QString("Fatal database connection error: %1").arg(databaseConnectionResult.message);
        return 1;
    }

    MailServiceClient mailService;
    FailableOperationResult mailServiceConfigurationResult = mailService.configure();
    if (!mailServiceConfigurationResult.success) {
        qCritical().noquote() << QString("Fatal mail service configuration error: %1").arg(mailServiceConfigurationResult.message);
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
        GraphingCalculation::Result result = GraphingCalculation::calculate(request.a, request.b, request.c);
        if (!result.success) {
            manager.failRequest(
                request.clientId,
                request.requestId,
                static_cast<int>(GraphingErrorCode::InternalError),
                result.errorMessage
            );
            return;
        }

        manager.completeRequest(request.clientId, request.requestId, result.value.split("|"));
    }, Qt::QueuedConnection);

    QObject::connect(&manager, &TCPGraphingServerManager::passwordResetRequested, &manager, [&manager, &database, &mailService](const GraphingPasswordResetRequest& request) {
        PasswordResetCreationResult result = database.createPasswordReset(request.loginOrEmail);
        if (!result.operation.success) {
            manager.failRequest(request.clientId, request.requestId, result.operation.errorCode, result.operation.message);
            return;
        }

        mailService.sendPasswordResetMessage(
            request.clientId,
            request.requestId,
            result.token,
            result.email,
            result.name,
            result.verificationCode
        );
    }, Qt::QueuedConnection);

    QObject::connect(&manager, &TCPGraphingServerManager::passwordResetVerificationRequested, &manager, [&manager, &database](const GraphingPasswordResetVerificationRequest& request) {
        FailableOperationResult result = database.verifyPasswordReset(request.token, request.verificationCode);
        if (result.success) {
            manager.completeRequest(request.clientId, request.requestId);
            return;
        }

        manager.failRequest(request.clientId, request.requestId, result.errorCode, result.message);
    }, Qt::QueuedConnection);

    QObject::connect(&manager, &TCPGraphingServerManager::passwordResetUpdateRequested, &manager, [&manager, &database](const GraphingPasswordResetUpdateRequest& request) {
        FailableOperationResult result = database.resetPassword(request.token, request.newPassword);
        if (result.success) {
            manager.completeRequest(request.clientId, request.requestId);
            return;
        }

        manager.failRequest(request.clientId, request.requestId, result.errorCode, result.message);
    }, Qt::QueuedConnection);

    QObject::connect(&mailService, &MailServiceClient::passwordResetMailSent, &manager, [&manager](quint64 clientId, quint64 requestId, const QString& token) {
        manager.completeRequest(clientId, requestId, QStringList() << token);
    }, Qt::QueuedConnection);

    QObject::connect(&mailService, &MailServiceClient::passwordResetMailFailed, &manager, [&manager, &database](quint64 clientId, quint64 requestId, const QString& token, int errorCode, const QString& errorMessage) {
        FailableOperationResult cleanupResult = database.cancelPasswordReset(token, errorMessage);
        if (!cleanupResult.success) {
            qWarning().noquote() << "[-] Cannot mark failed password reset token:" << cleanupResult.message;
        }

        manager.failRequest(clientId, requestId, errorCode, errorMessage);
    }, Qt::QueuedConnection);

    try {
        manager.startServer();
    } catch (const std::exception& e) {
        qCritical().noquote() << QString("Fatal exception while trying to start TCP Server: %1").arg(e.what());
        return 1;
    }

    return qtApp.exec();
}
