// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/account_creation.h>

#include <openssl/evp.h>

#include <algorithm>
#include <bit>
#include <memory>
#include <string_view>

namespace cybou {
namespace {
constexpr unsigned char VERSION{2};
constexpr size_t ROOT_SIG_SIZE{3309};
constexpr size_t DEVICE_SIG_SIZE{2420};
using MdContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

void Write64(unsigned char* out, uint64_t value)
{
    for (unsigned i{0}; i < 8; ++i) out[i] = static_cast<unsigned char>(value >> (8 * i));
}

uint64_t Read64(const unsigned char* in)
{
    uint64_t value{0};
    for (unsigned i{0}; i < 8; ++i) value |= uint64_t{in[i]} << (8 * i);
    return value;
}

std::optional<std::array<unsigned char, 32>> HashWithDomain(
    std::string_view domain, std::span<const unsigned char> bytes)
{
    MdContext ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    std::array<unsigned char, 32> digest{};
    unsigned int length{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), bytes.data(), bytes.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), digest.data(), &length) != 1 || length != digest.size()) return std::nullopt;
    return digest;
}

bool HasWork(std::span<const unsigned char, 32> digest, unsigned required_bits)
{
    if (required_bits > 256) return false;
    unsigned count{0};
    for (const unsigned char byte : digest) {
        if (byte == 0) { count += 8; continue; }
        count += std::countl_zero(byte);
        break;
    }
    return count >= required_bits;
}

bool ValidSignatures(const AccountCreateOp& op)
{
    return op.recovery_pop.ml_dsa.size() == ROOT_SIG_SIZE &&
        op.device_pop.ml_dsa.size() == DEVICE_SIG_SIZE;
}
} // namespace

std::optional<std::array<unsigned char, ACCOUNT_CREATE_WORK_SIZE>> SerializeAccountCreationWork(
    const AccountCreationWork& work)
{
    if (work.network_id.IsNull() || work.account_id.IsNull()) return std::nullopt;
    std::array<unsigned char, ACCOUNT_CREATE_WORK_SIZE> bytes{};
    bytes[0] = VERSION;
    std::copy_n(work.network_id.begin(), 32, bytes.begin() + 1);
    std::copy_n(work.account_id.Value().begin(), 32, bytes.begin() + 33);
    std::copy(work.authorization_commitment.begin(), work.authorization_commitment.end(), bytes.begin() + 65);
    Write64(bytes.data() + 97, work.work_epoch);
    Write64(bytes.data() + 105, work.nonce);
    return bytes;
}

std::optional<std::array<unsigned char, 32>> ComputeAccountCreateWorkHash(const AccountCreationWork& work)
{
    const auto bytes = SerializeAccountCreationWork(work);
    if (!bytes) return std::nullopt;
    return HashWithDomain("CYBOU/ACCOUNT-CREATE-WORK/V2", *bytes);
}

std::optional<std::array<unsigned char, 32>> ComputeAccountCreatePopDigest(
    const uint256& network_id, const AccountId& account_id,
    const IdentityAuthorization& authorization)
{
    if (network_id.IsNull() || account_id.IsNull()) return std::nullopt;
    const auto commitment = ComputeIdentityAuthorizationCommitment(authorization);
    if (!commitment) return std::nullopt;
    std::array<unsigned char, 96> body{};
    std::copy_n(network_id.begin(), 32, body.begin());
    std::copy_n(account_id.Value().begin(), 32, body.begin() + 32);
    std::copy(commitment->begin(), commitment->end(), body.begin() + 64);
    return HashWithDomain("CYBOU/ACCOUNT-POP/V2", body);
}

std::optional<std::array<unsigned char, ACCOUNT_CREATE_SIZE>> SerializeAccountCreateOp(
    const AccountCreateOp& op)
{
    const auto auth = SerializeIdentityAuthorization(op.authorization);
    const auto work = SerializeAccountCreationWork(op.work);
    if (op.account_id.IsNull() || !auth || !work || !ValidSignatures(op)) return std::nullopt;
    std::array<unsigned char, ACCOUNT_CREATE_SIZE> bytes{};
    bytes[0] = VERSION;
    std::copy_n(op.account_id.Value().begin(), 32, bytes.begin() + 1);
    std::copy(auth->begin(), auth->end(), bytes.begin() + 33);
    std::copy(work->begin(), work->end(), bytes.begin() + 3364);
    std::copy(op.recovery_pop.ed25519.begin(), op.recovery_pop.ed25519.end(), bytes.begin() + 3477);
    std::copy(op.recovery_pop.ml_dsa.begin(), op.recovery_pop.ml_dsa.end(), bytes.begin() + 3541);
    std::copy(op.device_pop.ed25519.begin(), op.device_pop.ed25519.end(), bytes.begin() + 6850);
    std::copy(op.device_pop.ml_dsa.begin(), op.device_pop.ml_dsa.end(), bytes.begin() + 6914);
    return bytes;
}

std::optional<AccountCreateOp> DeserializeAccountCreateOp(std::span<const unsigned char> bytes)
{
    if (bytes.size() != ACCOUNT_CREATE_SIZE || bytes[0] != VERSION) return std::nullopt;
    std::array<unsigned char, 32> id_bytes{};
    std::copy_n(bytes.begin() + 1, 32, id_bytes.begin());
    const auto account_id = AccountId::FromBytes(id_bytes);
    const auto auth = DeserializeIdentityAuthorization(bytes.subspan(33, IDENTITY_AUTHORIZATION_SIZE));
    if (!account_id || !auth || bytes[3364] != VERSION) return std::nullopt;
    AccountCreateOp op{.account_id = *account_id, .authorization = *auth,
        .work = {}, .recovery_pop = {}, .device_pop = {}};
    std::copy_n(bytes.begin() + 3365, 32, op.work.network_id.begin());
    std::array<unsigned char, 32> work_account{};
    std::copy_n(bytes.begin() + 3397, 32, work_account.begin());
    const auto work_id = AccountId::FromBytes(work_account);
    if (!work_id) return std::nullopt;
    op.work.account_id = *work_id;
    std::copy_n(bytes.begin() + 3429, 32, op.work.authorization_commitment.begin());
    op.work.work_epoch = Read64(bytes.data() + 3461);
    op.work.nonce = Read64(bytes.data() + 3469);
    std::copy_n(bytes.begin() + 3477, 64, op.recovery_pop.ed25519.begin());
    op.recovery_pop.ml_dsa.assign(bytes.begin() + 3541, bytes.begin() + 6850);
    std::copy_n(bytes.begin() + 6850, 64, op.device_pop.ed25519.begin());
    op.device_pop.ml_dsa.assign(bytes.begin() + 6914, bytes.end());
    return op;
}

AccountCreateError ValidateAccountCreateOp(
    const AccountCreateOp& op, const uint256& network_id,
    uint64_t block_height, const CybouProtocolParameters& params)
{
    if (!SerializeAccountCreateOp(op)) return AccountCreateError::INVALID_FORMAT;
    if (op.account_id.IsNull()) return AccountCreateError::NULL_ACCOUNT_ID;
    if (network_id.IsNull() || op.work.network_id != network_id) return AccountCreateError::NETWORK_MISMATCH;
    if (op.work.account_id != op.account_id) return AccountCreateError::ACCOUNT_ID_MISMATCH;
    const auto commitment = ComputeIdentityAuthorizationCommitment(op.authorization);
    if (!commitment || op.work.authorization_commitment != *commitment) return AccountCreateError::COMMITMENT_MISMATCH;
    const uint64_t epoch = EpochForHeight(block_height, params);
    if (op.work.work_epoch > epoch) return AccountCreateError::FUTURE_WORK_EPOCH;
    if (epoch - op.work.work_epoch > params.account_creation_epoch_lag) return AccountCreateError::EXPIRED_WORK_EPOCH;
    const auto work_hash = ComputeAccountCreateWorkHash(op.work);
    if (!work_hash || !HasWork(*work_hash, params.account_creation_work_bits)) return AccountCreateError::INSUFFICIENT_WORK;
    const auto pop_digest = ComputeAccountCreatePopDigest(network_id, op.account_id, op.authorization);
    if (!pop_digest || !VerifyIdentityMessage(op.authorization.recovery_root, op.recovery_pop, *pop_digest)) {
        return AccountCreateError::INVALID_RECOVERY_POP;
    }
    if (!VerifyIdentityMessage(op.authorization.initial_device, op.device_pop, *pop_digest)) {
        return AccountCreateError::INVALID_DEVICE_POP;
    }
    return AccountCreateError::NONE;
}

} // namespace cybou
