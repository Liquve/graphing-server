#ifndef PGDATABASE_H
#define PGDATABASE_H

#include <QSqlDatabase>
#include <QString>
#include "FailableOperationResult.h"

struct PasswordResetCreationResult {
    FailableOperationResult operation;
    QString token;
    QString verificationCode;
    QString email;
    QString name;
};

class PgDatabase
{
private:
    QSqlDatabase database;

    PgDatabase();

    QString getEnvValue(const char*, const char* = nullptr) const;
    FailableOperationResult ensureConnectionAlive();
    FailableOperationResult ensurePgCrypto();
    FailableOperationResult ensurePasswordResetSchema();
    FailableOperationResult dropLegacyPasswordResetCleanupFunction();
    QString generateResetToken() const;
    QString generateVerificationCode() const;
public:
    PgDatabase(const PgDatabase&) = delete;
    PgDatabase& operator=(const PgDatabase&) = delete;

    static PgDatabase& instance();

    FailableOperationResult connect();
    FailableOperationResult sync();
    FailableOperationResult login(const QString&, const QString&);
    FailableOperationResult registerUser(const QString&, const QString&, const QString&, const QString&);
    PasswordResetCreationResult createPasswordReset(const QString&);
    FailableOperationResult cancelPasswordReset(const QString&, const QString& = QString());
    FailableOperationResult verifyPasswordReset(const QString&, const QString&);
    FailableOperationResult resetPassword(const QString&, const QString&);
};

#endif // PGDATABASE_H
