#ifndef DBMODELS_H
#define DBMODELS_H

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <odb/core.hxx>
#include <odb/nullable.hxx>
#include <string>

#pragma db object table("users")
class DbUser
{
public:
    DbUser();
    DbUser(const std::string& login, const std::string& passwordHash, const std::string& name, const std::string& email);

    unsigned long long id() const;
    const std::string& email() const;
    const std::string& name() const;
    const std::string& login() const;

    void setPasswordHash(const std::string& passwordHash);

private:
    friend class odb::access;

    #pragma db id auto column("id")
    unsigned long long id_;

    #pragma db unique type("TEXT") column("email") options("CHECK (btrim(email) <> '')")
    std::string email_;

    #pragma db type("TEXT") column("password_hash") options("CHECK (btrim(password_hash) <> '')")
    std::string passwordHash_;

    #pragma db type("TEXT") column("name") options("CHECK (btrim(name) <> '')")
    std::string name_;

    #pragma db unique type("TEXT") column("login") options("CHECK (btrim(login) <> '')")
    std::string login_;
};

#pragma db object table("password_reset_tokens")
class DbPasswordResetToken
{
public:
    typedef boost::posix_time::ptime Timestamp;

    DbPasswordResetToken();
    DbPasswordResetToken(
        unsigned long long userId,
        const std::string& tokenHash,
        const std::string& verificationCodeHash,
        const Timestamp& createdAt,
        const Timestamp& expiresAt
    );

    unsigned long long id() const;
    unsigned long long userId() const;

    void markReplaced(const Timestamp& replacedAt);
    void markEmailDeliveryFailed(const Timestamp& failedAt, const std::string& failureMessage);
    void markVerified(const Timestamp& verifiedAt);
    void markUsed(const Timestamp& usedAt);

private:
    friend class odb::access;

    #pragma db id auto column("id")
    unsigned long long id_;

    #pragma db column("user_id") options("REFERENCES \"users\" (\"id\") ON DELETE CASCADE")
    unsigned long long userId_;

    #pragma db unique type("TEXT") column("token_hash") options("CHECK (btrim(token_hash) <> '')")
    std::string tokenHash_;

    #pragma db type("TEXT") column("verification_code_hash") options("CHECK (btrim(verification_code_hash) <> '')")
    std::string verificationCodeHash_;

    #pragma db null type("TIMESTAMP") column("verified_at")
    odb::nullable<Timestamp> verifiedAt_;

    #pragma db type("TIMESTAMP") column("expires_at")
    Timestamp expiresAt_;

    #pragma db type("TIMESTAMP") column("created_at") options("DEFAULT CURRENT_TIMESTAMP")
    Timestamp createdAt_;

    #pragma db null type("TIMESTAMP") column("cancelled_at")
    odb::nullable<Timestamp> cancelledAt_;

    #pragma db null type("TIMESTAMP") column("replaced_at")
    odb::nullable<Timestamp> replacedAt_;

    #pragma db null type("TIMESTAMP") column("used_at")
    odb::nullable<Timestamp> usedAt_;

    #pragma db null type("TIMESTAMP") column("email_failed_at")
    odb::nullable<Timestamp> emailFailedAt_;

    #pragma db null type("TEXT") column("failure_message")
    odb::nullable<std::string> failureMessage_;
};

#pragma db index(DbPasswordResetToken::"password_reset_tokens_one_active_per_user") \
    unique member(userId_) \
    options("WHERE cancelled_at IS NULL AND replaced_at IS NULL AND used_at IS NULL AND email_failed_at IS NULL")

#pragma db index(DbPasswordResetToken::"password_reset_tokens_user_id_created_at_idx") \
    member(userId_) member(createdAt_, "DESC")

#pragma db view
struct DbSchemaState
{
    bool usersExists;
    bool passwordResetTokensExists;
};

#pragma db view
struct DbStringValue
{
    std::string value;
};

#pragma db view
struct DbBoolValue
{
    bool value;
};

#pragma db view
struct DbIdValue
{
    unsigned long long id;
};

#pragma db view
struct DbPasswordResetVerificationState
{
    unsigned long long id;
    bool alreadyVerified;
    bool notExpired;
    bool codeMatches;
};

#pragma db view
struct DbPasswordResetUpdateState
{
    unsigned long long id;
    unsigned long long userId;
    bool verified;
};

#endif // DBMODELS_H
