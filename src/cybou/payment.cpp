// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/payment.h>

#include <openssl/evp.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string_view>

namespace cybou {

std::optional<std::array<unsigned char, PAYMENT_PAYLOAD_SIZE>> SerializePaymentPayload(const PaymentPayload& payment)
{
    if (payment.recipient.IsNull() || payment.amount == 0) return std::nullopt;
    std::array<unsigned char, PAYMENT_PAYLOAD_SIZE> out{};
    out[0] = 2;
    std::copy(payment.recipient.Value().begin(), payment.recipient.Value().end(), out.begin() + 1);
    for (unsigned i{0}; i < 8; ++i) out[33 + i] = static_cast<unsigned char>(payment.amount >> (8 * i));
    return out;
}

std::optional<PaymentPayload> DeserializePaymentPayload(std::span<const unsigned char> bytes)
{
    if (bytes.size() != PAYMENT_PAYLOAD_SIZE || bytes[0] != 2) return std::nullopt;
    const auto recipient = AccountId::FromBytes(bytes.subspan(1, AccountId::SIZE));
    if (!recipient) return std::nullopt;
    uint64_t amount{0};
    for (unsigned i{0}; i < 8; ++i) amount |= uint64_t{bytes[33 + i]} << (8 * i);
    if (!amount) return std::nullopt;
    return PaymentPayload{*recipient, amount};
}

std::optional<IdentityKeyId> ComputePaymentPayloadCommitment(const PaymentPayload& payment)
{
    constexpr std::string_view domain{"CYBOU/PAYMENT-PAYLOAD/V2"};
    const auto bytes = SerializePaymentPayload(payment);
    if (!bytes) return std::nullopt;
    using DigestCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    DigestCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    IdentityKeyId digest{};
    unsigned int size{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), bytes->data(), bytes->size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), digest.data(), &size) != 1 || size != digest.size()) return std::nullopt;
    return digest;
}

PaymentError ApplyPayment(const AuthorizedPayment& operation,
    const uint256& network_id, const CybouProtocolParameters& params,
    CybouState& state)
{
    if (operation.authorization.kind != DeviceOperationKind::PAYMENT) return PaymentError::INVALID_AUTHORIZATION;
    if (operation.payment.amount == 0) return PaymentError::ZERO_AMOUNT;
    if (operation.payment.recipient.IsNull()) return PaymentError::INVALID_PAYLOAD;
    const auto commitment = ComputePaymentPayloadCommitment(operation.payment);
    if (!commitment || operation.authorization.payload_commitment != *commitment) return PaymentError::INVALID_PAYLOAD;
    const auto sender_id = operation.authorization.account_id;
    if (sender_id == operation.payment.recipient) return PaymentError::SELF_PAYMENT;
    auto sender = state.accounts.find(sender_id);
    if (sender == state.accounts.end()) return PaymentError::SENDER_NOT_FOUND;
    auto recipient = state.accounts.find(operation.payment.recipient);
    if (recipient == state.accounts.end()) return PaymentError::RECIPIENT_NOT_FOUND;
    if (!state.identities.Find(sender_id) || !state.identities.Find(operation.payment.recipient)) return PaymentError::INCONSISTENT_STATE;
    if (sender->second.balance < operation.payment.amount) return PaymentError::INSUFFICIENT_BALANCE;
    if (sender->second.system_balance < params.payment_fee) return PaymentError::INSUFFICIENT_SYSTEM_BALANCE;
    if (recipient->second.balance > std::numeric_limits<uint64_t>::max() - operation.payment.amount) return PaymentError::RECIPIENT_OVERFLOW;
    if (state.pending_fee_pool > std::numeric_limits<uint64_t>::max() - params.payment_fee) return PaymentError::FEE_POOL_OVERFLOW;
    if (state.identities.AuthorizeDeviceOperation(operation.authorization, network_id) != IdentityRegistryError::NONE) return PaymentError::INVALID_AUTHORIZATION;
    sender->second.balance -= operation.payment.amount;
    sender->second.system_balance -= params.payment_fee;
    recipient->second.balance += operation.payment.amount;
    state.pending_fee_pool += params.payment_fee;
    return PaymentError::NONE;
}

std::optional<std::array<unsigned char, SYSTEM_LOCK_PAYLOAD_SIZE>> SerializeSystemLockPayload(const SystemLockPayload& lock)
{
    if (lock.amount == 0) return std::nullopt;
    std::array<unsigned char, SYSTEM_LOCK_PAYLOAD_SIZE> out{};
    out[0] = 2;
    for (unsigned i{0}; i < 8; ++i) out[1 + i] = static_cast<unsigned char>(lock.amount >> (8 * i));
    return out;
}

std::optional<SystemLockPayload> DeserializeSystemLockPayload(std::span<const unsigned char> bytes)
{
    if (bytes.size() != SYSTEM_LOCK_PAYLOAD_SIZE || bytes[0] != 2) return std::nullopt;
    uint64_t amount{0};
    for (unsigned i{0}; i < 8; ++i) amount |= uint64_t{bytes[1 + i]} << (8 * i);
    if (!amount) return std::nullopt;
    return SystemLockPayload{amount};
}

std::optional<IdentityKeyId> ComputeSystemLockPayloadCommitment(const SystemLockPayload& lock)
{
    constexpr std::string_view domain{"CYBOU/SYSTEM-LOCK-PAYLOAD/V2"};
    const auto bytes = SerializeSystemLockPayload(lock);
    if (!bytes) return std::nullopt;
    using DigestCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    DigestCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    IdentityKeyId digest{};
    unsigned int size{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), bytes->data(), bytes->size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), digest.data(), &size) != 1 || size != digest.size()) return std::nullopt;
    return digest;
}

SystemLockError ApplySystemLock(const AuthorizedSystemLock& operation,
    const uint256& network_id,
    CybouState& state)
{
    if (operation.authorization.kind != DeviceOperationKind::SYSTEM_LOCK) return SystemLockError::INVALID_AUTHORIZATION;
    if (operation.lock.amount == 0) return SystemLockError::ZERO_AMOUNT;
    const auto commitment = ComputeSystemLockPayloadCommitment(operation.lock);
    if (!commitment || operation.authorization.payload_commitment != *commitment) return SystemLockError::INVALID_PAYLOAD;
    const auto account_id = operation.authorization.account_id;
    auto account = state.accounts.find(account_id);
    if (account == state.accounts.end()) return SystemLockError::ACCOUNT_NOT_FOUND;
    if (!state.identities.Find(account_id)) return SystemLockError::INCONSISTENT_STATE;
    if (account->second.balance < operation.lock.amount) return SystemLockError::INSUFFICIENT_BALANCE;
    if (account->second.system_balance > std::numeric_limits<uint64_t>::max() - operation.lock.amount) return SystemLockError::SYSTEM_BALANCE_OVERFLOW;
    if (state.identities.AuthorizeDeviceOperation(operation.authorization, network_id) != IdentityRegistryError::NONE) return SystemLockError::INVALID_AUTHORIZATION;
    account->second.balance -= operation.lock.amount;
    account->second.system_balance += operation.lock.amount;
    return SystemLockError::NONE;
}

} // namespace cybou
