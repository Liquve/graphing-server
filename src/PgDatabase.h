#ifndef PGDATABASE_H
#define PGDATABASE_H

#include <QString>
#include <memory>
#include <string>
#include "FailableOperationResult.h"

namespace odb {
namespace pgsql {
class database;
}
}

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
    std::unique_ptr<odb::pgsql::database> database;

    PgDatabase();
    ~PgDatabase();

    QString getEnvValue(const char*, const char* = nullptr) const;
    std::string toStdString(const QString&) const;
    QString fromStdString(const std::string&) const;
    FailableOperationResult ensureConnectionAlive();
    FailableOperationResult ensureSchema();
    FailableOperationResult ensureExistingSchemaCompatibility();
    QString generateResetToken() const;
    QString generateVerificationCode() const;
    FailableOperationResult hashWithPgCrypto(const QString&, QString*, const QString&);
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
