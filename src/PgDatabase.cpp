#include "PgDatabase.h"
#include "DbModels.h"
#include "DbModels-odb.hxx"
#include "GraphingErrorCode.h"
#include <QByteArray>
#include <QDebug>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QStringList>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <odb/exception.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/pgsql/exceptions.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/transaction.hxx>
#include <vector>

namespace {
    FailableOperationResult internalError(const QString& message) {
        return FailableOperationResult::error(
            static_cast<int>(GraphingErrorCode::InternalError),
            message
        );
    }

    QString odbErrorMessage(const QString& prefix, const odb::exception& error) {
        return QString("%1: %2").arg(prefix, QString::fromUtf8(error.what()));
    }

    boost::posix_time::ptime currentTimestamp() {
        return boost::posix_time::microsec_clock::universal_time();
    }
}

PgDatabase::PgDatabase()
{
}

PgDatabase::~PgDatabase() = default;

QString PgDatabase::getEnvValue(const char* primaryName, const char* fallbackName) const {
    QString value = qEnvironmentVariable(primaryName);
    if (!value.isEmpty()) {
        return value;
    }

    if (fallbackName) {
        value = qEnvironmentVariable(fallbackName);
        if (!value.isEmpty()) {
            return value;
        }
    }

    return "";
}

std::string PgDatabase::toStdString(const QString& value) const {
    QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString PgDatabase::fromStdString(const std::string& value) const {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

FailableOperationResult PgDatabase::ensureConnectionAlive() {
    if (!this->database) {
        return internalError("Database connection is not configured");
    }

    try {
        odb::transaction transaction(this->database->begin());
        this->database->query_value<DbBoolValue>("SELECT true");
        transaction.commit();
        return FailableOperationResult::ok();
    } catch (const odb::exception& error) {
        this->database.reset();
        return internalError(odbErrorMessage("Database connection became unusable", error));
    }
}

FailableOperationResult PgDatabase::ensureExistingSchemaCompatibility() {
    try {
        QStringList statements{
            "ALTER TABLE public.password_reset_tokens DROP CONSTRAINT IF EXISTS password_reset_tokens_user_id_key",
            "ALTER TABLE public.password_reset_tokens ADD COLUMN IF NOT EXISTS cancelled_at TIMESTAMP NULL",
            "ALTER TABLE public.password_reset_tokens ADD COLUMN IF NOT EXISTS replaced_at TIMESTAMP NULL",
            "ALTER TABLE public.password_reset_tokens ADD COLUMN IF NOT EXISTS used_at TIMESTAMP NULL",
            "ALTER TABLE public.password_reset_tokens ADD COLUMN IF NOT EXISTS email_failed_at TIMESTAMP NULL",
            "ALTER TABLE public.password_reset_tokens ADD COLUMN IF NOT EXISTS failure_message TEXT NULL",
            "DO $$ "
            "DECLARE column_to_convert TEXT; "
            "BEGIN "
            "FOREACH column_to_convert IN ARRAY ARRAY['verified_at', 'expires_at', 'created_at', 'cancelled_at', 'replaced_at', 'used_at', 'email_failed_at'] LOOP "
            "IF EXISTS ("
            "SELECT 1 FROM information_schema.columns c "
            "WHERE table_schema = 'public' "
            "AND table_name = 'password_reset_tokens' "
            "AND c.column_name = column_to_convert "
            "AND data_type = 'timestamp with time zone'"
            ") THEN "
            "EXECUTE format("
            "'ALTER TABLE public.password_reset_tokens ALTER COLUMN %I TYPE TIMESTAMP USING %I AT TIME ZONE ''UTC'''"
            ", column_to_convert, column_to_convert); "
            "END IF; "
            "END LOOP; "
            "END $$",
            "CREATE UNIQUE INDEX IF NOT EXISTS password_reset_tokens_one_active_per_user "
            "ON public.password_reset_tokens (user_id) "
            "WHERE cancelled_at IS NULL "
            "AND replaced_at IS NULL "
            "AND used_at IS NULL "
            "AND email_failed_at IS NULL",
            "CREATE INDEX IF NOT EXISTS password_reset_tokens_user_id_created_at_idx "
            "ON public.password_reset_tokens (user_id, created_at DESC)",
            "DROP FUNCTION IF EXISTS public.cleanup_expired_password_reset_tokens_job()"
        };

        for (QStringList::const_iterator it = statements.begin(); it != statements.end(); ++it) {
            this->database->execute(this->toStdString(*it));
        }

        return FailableOperationResult::ok();
    } catch (const odb::exception& error) {
        return internalError(odbErrorMessage("Cannot ensure existing database schema compatibility", error));
    }
}

FailableOperationResult PgDatabase::ensureSchema() {
    if (!this->database) {
        return internalError("Database connection is not configured");
    }

    try {
        odb::transaction transaction(this->database->begin());
        this->database->execute("CREATE EXTENSION IF NOT EXISTS pgcrypto");

        DbSchemaState schemaState = this->database->query_value<DbSchemaState>(
            "SELECT "
            "to_regclass('public.users') IS NOT NULL, "
            "to_regclass('public.password_reset_tokens') IS NOT NULL"
        );

        if (!schemaState.usersExists && !schemaState.passwordResetTokensExists) {
            odb::schema_catalog::create_schema(*this->database, "", false);
        }

        FailableOperationResult compatibilityResult = this->ensureExistingSchemaCompatibility();
        if (!compatibilityResult.success) {
            return compatibilityResult;
        }

        transaction.commit();
        return FailableOperationResult::ok();
    } catch (const odb::exception& error) {
        return internalError(odbErrorMessage("Cannot ensure database schema", error));
    }
}

QString PgDatabase::generateResetToken() const {
    QString token;
    for (int i = 0; i < 4; ++i) {
        token.append(QString("%1").arg(
            static_cast<qulonglong>(QRandomGenerator::system()->generate64()),
            16,
            16,
            QLatin1Char('0')
        ));
    }

    return token;
}

QString PgDatabase::generateVerificationCode() const {
    return QString("%1").arg(QRandomGenerator::system()->bounded(1000000), 6, 10, QLatin1Char('0'));
}

FailableOperationResult PgDatabase::hashWithPgCrypto(const QString& value, QString* hash, const QString& errorPrefix) {
    try {
        typedef odb::query<DbStringValue> Query;
        odb::transaction transaction(this->database->begin());
        DbStringValue hashedValue = this->database->query_value<DbStringValue>(
            "SELECT crypt(" + Query::_val(this->toStdString(value)) + ", gen_salt('bf', 12))"
        );
        transaction.commit();

        *hash = this->fromStdString(hashedValue.value);
        return FailableOperationResult::ok();
    } catch (const odb::exception& error) {
        return internalError(odbErrorMessage(errorPrefix, error));
    }
}

FailableOperationResult PgDatabase::connect() {
    if (this->database) {
        FailableOperationResult connectionAliveResult = this->ensureConnectionAlive();
        if (connectionAliveResult.success) {
            return FailableOperationResult::ok();
        }

        qWarning().noquote() << "[-] Reopening stale PostgreSQL connection:" << connectionAliveResult.message;
    }

    QString hostName = this->getEnvValue("DB_HOST", "PGHOST");
    QString portValue = this->getEnvValue("DB_PORT", "PGPORT");
    QString databaseName = this->getEnvValue("DB_NAME", "PGDATABASE");
    QString userName = this->getEnvValue("DB_USER", "PGUSER");
    QString password = this->getEnvValue("DB_PASSWORD", "PGPASSWORD");
    QString sslMode = this->getEnvValue("DB_SSLMODE", "PGSSLMODE");
    if (sslMode.isEmpty()) {
        sslMode = "require";
    }

    if (hostName.isEmpty() || portValue.isEmpty() || databaseName.isEmpty() || userName.isEmpty() || password.isEmpty()) {
        return internalError("Database connection env is incomplete; expected DB_HOST/DB_PORT/DB_NAME/DB_USER/DB_PASSWORD");
    }

    bool portOk = false;
    unsigned int port = portValue.toUInt(&portOk);
    if (!portOk) {
        return internalError(QString("Invalid DB_PORT value: %1").arg(portValue));
    }

    QString extraConnectionInfo = QString(
        "sslmode=%1 "
        "connect_timeout=10 "
        "keepalives=1 "
        "keepalives_idle=30 "
        "keepalives_interval=10 "
        "keepalives_count=3"
    ).arg(sslMode);

    try {
        this->database.reset(new odb::pgsql::database(
            this->toStdString(userName),
            this->toStdString(password),
            this->toStdString(databaseName),
            this->toStdString(hostName),
            port,
            this->toStdString(extraConnectionInfo)
        ));

        return this->ensureSchema();
    } catch (const odb::exception& error) {
        this->database.reset();
        return internalError(
            QString("Cannot connect to PostgreSQL %1:%2/%3 as %4: %5")
                .arg(hostName)
                .arg(port)
                .arg(databaseName)
                .arg(userName)
                .arg(QString::fromUtf8(error.what()))
        );
    }
}

PgDatabase& PgDatabase::instance() {
    static PgDatabase database;
    return database;
}

FailableOperationResult PgDatabase::sync() {
    FailableOperationResult connectionResult = this->connect();
    if (!connectionResult.success) {
        return connectionResult;
    }

    try {
        odb::transaction transaction(this->database->begin());
        this->database->execute("CREATE EXTENSION IF NOT EXISTS pgcrypto");
        odb::schema_catalog::create_schema(*this->database);
        transaction.commit();
        return FailableOperationResult::ok();
    } catch (const odb::exception& error) {
        return internalError(odbErrorMessage("Cannot sync database schema", error));
    }
}

FailableOperationResult PgDatabase::login(const QString& login, const QString& password) {
    FailableOperationResult connectionResult = this->connect();
    if (!connectionResult.success) {
        return connectionResult;
    }

    try {
        typedef odb::query<DbBoolValue> Query;
        odb::transaction transaction(this->database->begin());
        std::unique_ptr<DbBoolValue> passwordCheck(this->database->query_one<DbBoolValue>(
            "SELECT password_hash = crypt(" + Query::_val(this->toStdString(password)) + ", password_hash) "
            "FROM users "
            "WHERE login = " + Query::_val(this->toStdString(login))
        ));

        if (!passwordCheck) {
            return FailableOperationResult::error((int)GraphingErrorCode::Forbidden, "User with this login was not found");
        }

        if (!passwordCheck->value) {
            return FailableOperationResult::error((int)GraphingErrorCode::Forbidden, "Incorrect password");
        }

        transaction.commit();
        return FailableOperationResult::ok();
    } catch (const odb::exception& error) {
        return internalError(odbErrorMessage("Login query failed", error));
    }
}

FailableOperationResult PgDatabase::registerUser(const QString& login, const QString& password, const QString& name, const QString& email) {
    FailableOperationResult connectionResult = this->connect();
    if (!connectionResult.success) {
        return connectionResult;
    }

    if (password.isEmpty()) {
        return FailableOperationResult::error((int)GraphingErrorCode::BadRequest, "Password cannot be blank");
    }

    try {
        QString passwordHash;
        FailableOperationResult hashResult = this->hashWithPgCrypto(password, &passwordHash, "Cannot hash registration password");
        if (!hashResult.success) {
            return hashResult;
        }

        odb::transaction transaction(this->database->begin());
        DbUser user(
            this->toStdString(login),
            this->toStdString(passwordHash),
            this->toStdString(name),
            this->toStdString(email)
        );
        this->database->persist(user);
        transaction.commit();
        return FailableOperationResult::ok();
    } catch (const odb::pgsql::database_exception& error) {
        if (error.sqlstate() == "23505") {
            return FailableOperationResult::error((int)GraphingErrorCode::Conflict, "User with this login or email already exists");
        }
        if (error.sqlstate() == "23514") {
            return FailableOperationResult::error((int)GraphingErrorCode::BadRequest, "Registration fields cannot be blank");
        }

        return internalError(odbErrorMessage("Registration query failed", error));
    } catch (const odb::exception& error) {
        return internalError(odbErrorMessage("Registration query failed", error));
    }
}

PasswordResetCreationResult PgDatabase::createPasswordReset(const QString& loginOrEmail) {
    PasswordResetCreationResult result;

    FailableOperationResult connectionResult = this->connect();
    if (!connectionResult.success) {
        result.operation = connectionResult;
        return result;
    }

    if (loginOrEmail.isEmpty()) {
        result.operation = FailableOperationResult::error(
            (int)GraphingErrorCode::BadRequest,
            "Password reset requires login or email"
        );
        return result;
    }

    try {
        typedef odb::query<DbUser> UserQuery;
        std::unique_ptr<DbUser> user;
        {
            odb::transaction transaction(this->database->begin());
            user.reset(this->database->query_one<DbUser>(
                UserQuery::login == this->toStdString(loginOrEmail) ||
                UserQuery::email == this->toStdString(loginOrEmail)
            ));
            transaction.commit();
        }

        if (!user) {
            result.operation = FailableOperationResult::error(
                (int)GraphingErrorCode::BadRequest,
                "User with this login or email was not found"
            );
            return result;
        }

        result.email = this->fromStdString(user->email());
        result.name = this->fromStdString(user->name());
        result.token = this->generateResetToken();
        result.verificationCode = this->generateVerificationCode();

        QString tokenHash;
        FailableOperationResult tokenHashResult = this->hashWithPgCrypto(result.token, &tokenHash, "Cannot hash password reset token");
        if (!tokenHashResult.success) {
            result.operation = tokenHashResult;
            return result;
        }

        QString verificationCodeHash;
        FailableOperationResult codeHashResult = this->hashWithPgCrypto(
            result.verificationCode,
            &verificationCodeHash,
            "Cannot hash password reset verification code"
        );
        if (!codeHashResult.success) {
            result.operation = codeHashResult;
            return result;
        }

        odb::transaction transaction(this->database->begin());
        typedef odb::query<DbPasswordResetToken> TokenQuery;
        typedef odb::result<DbPasswordResetToken> TokenResult;

        std::vector<unsigned long long> activeTokenIds;
        TokenResult activeTokens = this->database->query<DbPasswordResetToken>(
            TokenQuery::userId == user->id() &&
            TokenQuery::cancelledAt.is_null() &&
            TokenQuery::replacedAt.is_null() &&
            TokenQuery::usedAt.is_null() &&
            TokenQuery::emailFailedAt.is_null()
        );
        for (TokenResult::iterator it = activeTokens.begin(); it != activeTokens.end(); ++it) {
            activeTokenIds.push_back(it->id());
        }

        boost::posix_time::ptime now = currentTimestamp();
        for (std::vector<unsigned long long>::const_iterator it = activeTokenIds.begin(); it != activeTokenIds.end(); ++it) {
            std::unique_ptr<DbPasswordResetToken> activeToken(this->database->find<DbPasswordResetToken>(*it));
            if (activeToken) {
                activeToken->markReplaced(now);
                this->database->update(*activeToken);
            }
        }

        DbPasswordResetToken token(
            user->id(),
            this->toStdString(tokenHash),
            this->toStdString(verificationCodeHash),
            now,
            now + boost::posix_time::hours(24)
        );
        this->database->persist(token);
        transaction.commit();

        result.operation = FailableOperationResult::ok();
        return result;
    } catch (const odb::exception& error) {
        result.operation = internalError(odbErrorMessage("Cannot create password reset token", error));
        return result;
    }
}

FailableOperationResult PgDatabase::cancelPasswordReset(const QString& token, const QString& failureMessage) {
    FailableOperationResult connectionResult = this->connect();
    if (!connectionResult.success) {
        return connectionResult;
    }

    if (token.isEmpty()) {
        return FailableOperationResult::error((int)GraphingErrorCode::BadRequest, "Password reset token cannot be blank");
    }

    try {
        typedef odb::query<DbIdValue> Query;
        odb::transaction transaction(this->database->begin());
        std::unique_ptr<DbIdValue> tokenId(this->database->query_one<DbIdValue>(
            "SELECT id "
            "FROM password_reset_tokens "
            "WHERE token_hash = crypt(" + Query::_val(this->toStdString(token)) + ", token_hash) "
            "AND replaced_at IS NULL "
            "AND used_at IS NULL"
        ));

        if (!tokenId) {
            transaction.commit();
            return FailableOperationResult::ok();
        }

        std::unique_ptr<DbPasswordResetToken> passwordResetToken(this->database->find<DbPasswordResetToken>(tokenId->id));
        if (passwordResetToken) {
            passwordResetToken->markEmailDeliveryFailed(currentTimestamp(), this->toStdString(failureMessage));
            this->database->update(*passwordResetToken);
        }
        transaction.commit();
        return FailableOperationResult::ok();
    } catch (const odb::exception& error) {
        return internalError(odbErrorMessage("Cannot cancel password reset token", error));
    }
}

FailableOperationResult PgDatabase::verifyPasswordReset(const QString& token, const QString& verificationCode) {
    FailableOperationResult connectionResult = this->connect();
    if (!connectionResult.success) {
        return connectionResult;
    }

    if (token.isEmpty()) {
        return FailableOperationResult::error((int)GraphingErrorCode::BadRequest, "Password reset token cannot be blank");
    }

    if (verificationCode.isEmpty()) {
        return FailableOperationResult::error((int)GraphingErrorCode::BadRequest, "Verification code cannot be blank");
    }

    try {
        typedef odb::query<DbPasswordResetVerificationState> Query;
        odb::transaction transaction(this->database->begin());
        std::unique_ptr<DbPasswordResetVerificationState> state(this->database->query_one<DbPasswordResetVerificationState>(
            "SELECT id, "
            "verified_at IS NOT NULL, "
            "expires_at > CURRENT_TIMESTAMP, "
            "verification_code_hash = crypt(" + Query::_val(this->toStdString(verificationCode)) + ", verification_code_hash) "
            "FROM password_reset_tokens "
            "WHERE token_hash = crypt(" + Query::_val(this->toStdString(token)) + ", token_hash) "
            "AND cancelled_at IS NULL "
            "AND replaced_at IS NULL "
            "AND used_at IS NULL "
            "AND email_failed_at IS NULL"
        ));

        if (!state || !state->notExpired) {
            return FailableOperationResult::error(
                (int)GraphingErrorCode::BadRequest,
                "Password reset token is invalid or expired"
            );
        }

        if (state->alreadyVerified) {
            transaction.commit();
            return FailableOperationResult::ok();
        }

        if (!state->codeMatches) {
            return FailableOperationResult::error((int)GraphingErrorCode::Forbidden, "Incorrect verification code");
        }

        std::unique_ptr<DbPasswordResetToken> passwordResetToken(this->database->find<DbPasswordResetToken>(state->id));
        if (passwordResetToken) {
            passwordResetToken->markVerified(currentTimestamp());
            this->database->update(*passwordResetToken);
        }
        transaction.commit();
        return FailableOperationResult::ok();
    } catch (const odb::exception& error) {
        return internalError(odbErrorMessage("Cannot verify password reset token", error));
    }
}

FailableOperationResult PgDatabase::resetPassword(const QString& token, const QString& newPassword) {
    FailableOperationResult connectionResult = this->connect();
    if (!connectionResult.success) {
        return connectionResult;
    }

    if (token.isEmpty()) {
        return FailableOperationResult::error((int)GraphingErrorCode::BadRequest, "Password reset token cannot be blank");
    }

    if (newPassword.isEmpty()) {
        return FailableOperationResult::error((int)GraphingErrorCode::BadRequest, "Password cannot be blank");
    }

    try {
        QString newPasswordHash;
        FailableOperationResult hashResult = this->hashWithPgCrypto(newPassword, &newPasswordHash, "Cannot hash new password");
        if (!hashResult.success) {
            return hashResult;
        }

        odb::transaction transaction(this->database->begin());
        typedef odb::query<DbPasswordResetUpdateState> Query;
        std::unique_ptr<DbPasswordResetUpdateState> state(this->database->query_one<DbPasswordResetUpdateState>(
            "SELECT id, user_id, verified_at IS NOT NULL "
            "FROM password_reset_tokens "
            "WHERE token_hash = crypt(" + Query::_val(this->toStdString(token)) + ", token_hash) "
            "AND expires_at > CURRENT_TIMESTAMP "
            "AND cancelled_at IS NULL "
            "AND replaced_at IS NULL "
            "AND used_at IS NULL "
            "AND email_failed_at IS NULL "
            "FOR UPDATE"
        ));

        if (!state) {
            return FailableOperationResult::error(
                (int)GraphingErrorCode::BadRequest,
                "Password reset token is invalid or expired"
            );
        }

        if (!state->verified) {
            return FailableOperationResult::error(
                (int)GraphingErrorCode::Forbidden,
                "Password reset code is not verified"
            );
        }

        std::unique_ptr<DbUser> user(this->database->find<DbUser>(state->userId));
        if (!user) {
            return internalError("Password reset user was not found");
        }
        user->setPasswordHash(this->toStdString(newPasswordHash));
        this->database->update(*user);

        std::unique_ptr<DbPasswordResetToken> passwordResetToken(this->database->find<DbPasswordResetToken>(state->id));
        if (!passwordResetToken) {
            return internalError("Password reset token disappeared during update");
        }
        passwordResetToken->markUsed(currentTimestamp());
        this->database->update(*passwordResetToken);

        transaction.commit();
        return FailableOperationResult::ok();
    } catch (const odb::exception& error) {
        return internalError(odbErrorMessage("Cannot update password", error));
    }
}
