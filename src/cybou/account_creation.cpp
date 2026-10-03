// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/account_creation.h>
#include <cybou/identity_crypto.h>
#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <bit>
#include <string_view>

namespace cybou {
namespace {
constexpr unsigned char VERSION{4};
constexpr size_t ROOT_SIG_SIZE{3309};
constexpr size_t AUTHORIZATION_SIG_SIZE{2420};

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
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), bytes}, digest.data())) return std::nullopt;
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
        op.authorization_pop.ml_dsa.size() == AUTHORIZATION_SIG_SIZE;
}
} // namespace

std::optional<std::array<unsigned char, ACCOUNT_CREATE_WORK_SIZE>> SerializeAccountCreationWork(
    const AccountCreationWork& work)
{
    if (work.network_binding.IsNull() || work.account_id.IsNull()) return std::nullopt;
    std::array<unsigned char, ACCOUNT_CREATE_WORK_SIZE> bytes{};
    bytes[0] = VERSION;
    std::copy_n(work.network_binding.begin(), 32, bytes.begin() + 1);
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
    return HashWithDomain("CYBOU/ACCOUNT-CREATE-WORK/V3", *bytes);
}

std::optional<std::array<unsigned char, 32>> ComputeAccountCreatePopDigest(
    const cybou::Hash256& network_binding, const AccountId& account_id,
    const IdentityAuthorization& authorization,
    std::span<const unsigned char, 32> kem_package_id)
{
    if (network_binding.IsNull() || account_id.IsNull()) return std::nullopt;
    const auto commitment = ComputeAccountCreateAuthorizationCommitment(authorization, kem_package_id);
    if (!commitment) return std::nullopt;
    std::array<unsigned char, 96> body{};
    std::copy_n(network_binding.begin(), 32, body.begin());
    std::copy_n(account_id.Value().begin(), 32, body.begin() + 32);
    std::copy(commitment->begin(), commitment->end(), body.begin() + 64);
    return HashWithDomain("CYBOU/ACCOUNT-POP/V3", body);
}

std::optional<std::array<unsigned char, 32>> ComputeAccountCreateAuthorizationCommitment(
    const IdentityAuthorization& authorization,
    std::span<const unsigned char, 32> kem_package_id)
{
    const auto auth_commitment = ComputeIdentityAuthorizationCommitment(authorization);
    if (!auth_commitment || std::all_of(kem_package_id.begin(), kem_package_id.end(),
            [](unsigned char byte) { return byte == 0; })) return std::nullopt;
    constexpr std::string_view domain{"CYBOU/ACCOUNT-AUTHORIZATION/V3"};
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), *auth_commitment, kem_package_id}, digest.data())) {
        return std::nullopt;
    }
    return digest;
}

std::optional<std::array<unsigned char, ACCOUNT_CREATE_SIZE>> SerializeAccountCreateOp(
    const AccountCreateOp& op)
{
    const auto auth = SerializeIdentityAuthorization(op.authorization);
    const auto work = SerializeAccountCreationWork(op.work);
    if (op.account_id.IsNull() || !auth || !work || !DecodeIdentityKemPackage(op.kem_package) ||
        !ValidSignatures(op)) return std::nullopt;
    std::array<unsigned char, ACCOUNT_CREATE_SIZE> bytes{};
    bytes[0] = VERSION;
    std::copy_n(op.account_id.Value().begin(), 32, bytes.begin() + 1);
    std::copy(auth->begin(), auth->end(), bytes.begin() + 33);
    std::copy(op.kem_package.begin(), op.kem_package.end(), bytes.begin() + 3364);
    std::copy(work->begin(), work->end(), bytes.begin() + 4583);
    std::copy(op.recovery_pop.ed25519.begin(), op.recovery_pop.ed25519.end(), bytes.begin() + 4696);
    std::copy(op.recovery_pop.ml_dsa.begin(), op.recovery_pop.ml_dsa.end(), bytes.begin() + 4760);
    std::copy(op.authorization_pop.ed25519.begin(), op.authorization_pop.ed25519.end(), bytes.begin() + 8069);
    std::copy(op.authorization_pop.ml_dsa.begin(), op.authorization_pop.ml_dsa.end(), bytes.begin() + 8133);
    return bytes;
}

std::optional<AccountCreateOp> DeserializeAccountCreateOp(std::span<const unsigned char> bytes)
{
    if (bytes.size() != ACCOUNT_CREATE_SIZE || bytes[0] != VERSION) return std::nullopt;
    std::array<unsigned char, 32> id_bytes{};
    std::copy_n(bytes.begin() + 1, 32, id_bytes.begin());
    const auto account_id = AccountId::FromBytes(id_bytes);
    const auto auth = DeserializeIdentityAuthorization(bytes.subspan(33, IDENTITY_AUTHORIZATION_SIZE));
    if (!account_id || !auth || bytes[4583] != VERSION) return std::nullopt;
    IdentityKemPackage kem_package{};
    std::copy_n(bytes.begin() + 3364, kem_package.size(), kem_package.begin());
    if (!DecodeIdentityKemPackage(kem_package)) return std::nullopt;
    AccountCreateOp op{.account_id = *account_id, .authorization = *auth,
        .kem_package = kem_package, .work = {}, .recovery_pop = {}, .authorization_pop = {}};
    std::copy_n(bytes.begin() + 4584, 32, op.work.network_binding.begin());
    std::array<unsigned char, 32> work_account{};
    std::copy_n(bytes.begin() + 4616, 32, work_account.begin());
    const auto work_id = AccountId::FromBytes(work_account);
    if (!work_id) return std::nullopt;
    op.work.account_id = *work_id;
    std::copy_n(bytes.begin() + 4648, 32, op.work.authorization_commitment.begin());
    op.work.work_epoch = Read64(bytes.data() + 4680);
    op.work.nonce = Read64(bytes.data() + 4688);
    std::copy_n(bytes.begin() + 4696, 64, op.recovery_pop.ed25519.begin());
    op.recovery_pop.ml_dsa.assign(bytes.begin() + 4760, bytes.begin() + 8069);
    std::copy_n(bytes.begin() + 8069, 64, op.authorization_pop.ed25519.begin());
    op.authorization_pop.ml_dsa.assign(bytes.begin() + 8133, bytes.end());
    return op;
}

AccountCreateError ValidateAccountCreateOp(
    const AccountCreateOp& op, const cybou::Hash256& network_binding,
    uint64_t block_height, const CybouProtocolParameters& params)
{
    if (!SerializeAccountCreateOp(op)) return AccountCreateError::INVALID_FORMAT;
    if (!params.identity_kem_xwing_enabled) return AccountCreateError::INVALID_FORMAT;
    if (op.account_id.IsNull()) return AccountCreateError::NULL_ACCOUNT_ID;
    if (network_binding.IsNull() || op.work.network_binding != network_binding) return AccountCreateError::NETWORK_MISMATCH;
    if (op.work.account_id != op.account_id) return AccountCreateError::ACCOUNT_ID_MISMATCH;
    const auto authorization_id = ComputeAuthorizationKeyId(op.authorization.authorization_key);
    const auto account_bytes = op.account_id.Value();
    const auto package_id = authorization_id ? ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{network_binding.begin(), 32},
        std::span<const unsigned char, 32>{account_bytes.begin(), 32}, 0,
        op.kem_package) : std::nullopt;
    const auto commitment = package_id ? ComputeAccountCreateAuthorizationCommitment(op.authorization, *package_id) : std::nullopt;
    if (!commitment || op.work.authorization_commitment != *commitment) return AccountCreateError::COMMITMENT_MISMATCH;
    const uint64_t epoch = EpochForHeight(block_height, params);
    if (op.work.work_epoch > epoch) return AccountCreateError::FUTURE_WORK_EPOCH;
    if (epoch - op.work.work_epoch > params.account_creation_epoch_lag) return AccountCreateError::EXPIRED_WORK_EPOCH;
    const auto work_hash = ComputeAccountCreateWorkHash(op.work);
    if (!work_hash || !HasWork(*work_hash, params.account_creation_work_bits)) return AccountCreateError::INSUFFICIENT_WORK;
    const auto pop_digest = ComputeAccountCreatePopDigest(network_binding, op.account_id, op.authorization, *package_id);
    if (!pop_digest || !VerifyIdentityMessage(op.authorization.recovery_root, op.recovery_pop, *pop_digest)) {
        return AccountCreateError::INVALID_RECOVERY_POP;
    }
    if (!VerifyIdentityMessage(op.authorization.authorization_key, op.authorization_pop, *pop_digest)) {
        return AccountCreateError::INVALID_AUTHORIZATION_POP;
    }
    return AccountCreateError::NONE;
}

} // namespace cybou
