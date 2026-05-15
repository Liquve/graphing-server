#include "DbModels.h"

DbUser::DbUser()
    : id_(0)
{
}

DbUser::DbUser(const std::string& login, const std::string& passwordHash, const std::string& name, const std::string& email)
    : id_(0),
      email_(email),
      passwordHash_(passwordHash),
      name_(name),
      login_(login)
{
}

unsigned long long DbUser::id() const {
    return this->id_;
}

const std::string& DbUser::email() const {
    return this->email_;
}

const std::string& DbUser::name() const {
    return this->name_;
}

const std::string& DbUser::login() const {
    return this->login_;
}

void DbUser::setPasswordHash(const std::string& passwordHash) {
    this->passwordHash_ = passwordHash;
}

DbPasswordResetToken::DbPasswordResetToken()
    : id_(0),
      userId_(0)
{
}

DbPasswordResetToken::DbPasswordResetToken(
    unsigned long long userId,
    const std::string& tokenHash,
    const std::string& verificationCodeHash,
    const Timestamp& createdAt,
    const Timestamp& expiresAt
)
    : id_(0),
      userId_(userId),
      tokenHash_(tokenHash),
      verificationCodeHash_(verificationCodeHash),
      expiresAt_(expiresAt),
      createdAt_(createdAt)
{
}

unsigned long long DbPasswordResetToken::id() const {
    return this->id_;
}

unsigned long long DbPasswordResetToken::userId() const {
    return this->userId_;
}

void DbPasswordResetToken::markReplaced(const Timestamp& replacedAt) {
    this->replacedAt_ = replacedAt;
}

void DbPasswordResetToken::markEmailDeliveryFailed(const Timestamp& failedAt, const std::string& failureMessage) {
    this->cancelledAt_ = failedAt;
    this->emailFailedAt_ = failedAt;
    if (!failureMessage.empty()) {
        this->failureMessage_ = failureMessage;
    }
}

void DbPasswordResetToken::markVerified(const Timestamp& verifiedAt) {
    this->verifiedAt_ = verifiedAt;
}

void DbPasswordResetToken::markUsed(const Timestamp& usedAt) {
    this->usedAt_ = usedAt;
}
