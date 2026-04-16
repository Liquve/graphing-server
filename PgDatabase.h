#ifndef PGDATABASE_H
#define PGDATABASE_H

#include <QSqlDatabase>
#include <QString>
#include "FailableOperationResult.h"

class PgDatabase
{
private:
    QSqlDatabase database;

    PgDatabase();

    QString getEnvValue(const char*, const char* = nullptr) const;
    QString hashPassword(const QString&) const;
public:
    PgDatabase(const PgDatabase&) = delete;
    PgDatabase& operator=(const PgDatabase&) = delete;

    static PgDatabase& instance();

    FailableOperationResult connect();
    FailableOperationResult sync();
    FailableOperationResult login(const QString&, const QString&);
    FailableOperationResult registerUser(const QString&, const QString&, const QString&, const QString&);
};

#endif // PGDATABASE_H
