// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_registry.h>
#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <limits>
#include <string_view>

namespace cybou {
namespace {
void Append64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

bool Nonzero(std::span<const unsigned char> value)
{
    return std::any_of(value.begin(), value.end(), [](unsigned char b) { return b != 0; });
}

std::optional<IdentityKeyId> Hash(std::string_view domain, std::span<const unsigned char> bytes)
{
    IdentityKeyId digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), bytes}, digest.data())) return std::nullopt;
    return digest;
}
} // namespace

std::optional<IdentityKeyId> ComputeIdentityRotateDigest(const uint256& network_id, const IdentityRotate& request)
{
    if (network_id.IsNull() || request.account_id.IsNull()) return std::nullopt;
    const auto recovery_id = ComputeRecoveryKeyId(request.new_recovery_key);
    const auto authorization_id = ComputeAuthorizationKeyId(request.new_authorization_key);
    const auto account = request.account_id.Value();
    const auto package_id = recovery_id && authorization_id ? ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{network_id.begin(), 32},
        std::span<const unsigned char, 32>{account.begin(), 32}, request.key_epoch,
        request.new_kem_package) : std::nullopt;
    if (!recovery_id || !authorization_id || !package_id) return std::nullopt;

    std::vector<unsigned char> preimage;
    constexpr std::string_view domain{"CYBOU/IDENTITY-ROTATE/V1"};
    preimage.insert(preimage.end(), domain.begin(), domain.end());
    preimage.insert(preimage.end(), network_id.begin(), network_id.end());
    preimage.insert(preimage.end(), account.begin(), account.end());
    Append64(preimage, request.nonce);
    Append64(preimage, request.key_epoch);
    preimage.insert(preimage.end(), recovery_id->begin(), recovery_id->end());
    preimage.insert(preimage.end(), authorization_id->begin(), authorization_id->end());
    preimage.insert(preimage.end(), package_id->begin(), package_id->end());
    return Hash("CYBOU/IDENTITY-ROTATE-DIGEST/V1", preimage);
}

std::optional<IdentityKeyId> ComputeIdentityOperationDigest(
    const uint256& network_id, const IdentityOperationAuthorization& request)
{
    const auto kind = static_cast<uint8_t>(request.kind);
    if (kind < 1 || kind > static_cast<uint8_t>(IdentityOperationKind::RESOURCE_RELEASE) || !Nonzero(request.payload_commitment) ||
        network_id.IsNull() || request.account_id.IsNull()) return std::nullopt;
    std::vector<unsigned char> preimage;
    constexpr std::string_view domain{"CYBOU/IDENTITY-OP/V2"};
    preimage.insert(preimage.end(), domain.begin(), domain.end());
    preimage.insert(preimage.end(), network_id.begin(), network_id.end());
    const auto account = request.account_id.Value();
    preimage.insert(preimage.end(), account.begin(), account.end());
    Append64(preimage, request.nonce);
    Append64(preimage, request.key_epoch);
    preimage.push_back(kind);
    preimage.insert(preimage.end(), request.payload_commitment.begin(), request.payload_commitment.end());
    return Hash("CYBOU/IDENTITY-OP-DIGEST/V2", preimage);
}

IdentityRegistryError IdentityRegistry::Register(const AccountCreateOp& create,
    const uint256& network_id, const uint64_t block_height, const CybouProtocolParameters& params)
{
    if (ValidateAccountCreateOp(create, network_id, block_height, params) != AccountCreateError::NONE) return IdentityRegistryError::INVALID_CREATE;
    if (m_accounts.contains(create.account_id)) return IdentityRegistryError::ACCOUNT_EXISTS;
    const auto recovery_id = ComputeRecoveryKeyId(create.authorization.recovery_root);
    const auto authorization_id = ComputeAuthorizationKeyId(create.authorization.authorization_key);
    const auto account = create.account_id.Value();
    const auto package_id = ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{network_id.begin(), 32},
        std::span<const unsigned char, 32>{account.begin(), 32}, 0, create.kem_package);
    if (!recovery_id || !authorization_id || !package_id) return IdentityRegistryError::INVALID_KEY;
    if (m_recovery_index.contains(*recovery_id)) return IdentityRegistryError::RECOVERY_KEY_EXISTS;
    IdentityRecord record{
        .recovery_key = create.authorization.recovery_root,
        .authorization_key = create.authorization.authorization_key,
        .kem_package_id = *package_id,
        .nonce = 0,
        .key_epoch = 0,
    };
    m_accounts.emplace(create.account_id, std::move(record));
    m_recovery_index.emplace(*recovery_id, create.account_id);
    return IdentityRegistryError::NONE;
}

IdentityRegistryError IdentityRegistry::RotateIdentity(const IdentityRotate& request, const uint256& network_id)
{
    auto it = m_accounts.find(request.account_id);
    if (it == m_accounts.end()) return IdentityRegistryError::ACCOUNT_NOT_FOUND;
    auto& record = it->second;
    if (request.nonce != record.nonce || request.key_epoch != record.key_epoch + 1) return IdentityRegistryError::BAD_NONCE;
    if (record.nonce == std::numeric_limits<uint64_t>::max() || record.key_epoch == std::numeric_limits<uint64_t>::max()) {
        return IdentityRegistryError::NONCE_EXHAUSTED;
    }
    const auto old_id = ComputeRecoveryKeyId(record.recovery_key);
    const auto new_id = ComputeRecoveryKeyId(request.new_recovery_key);
    const auto new_auth_id = ComputeAuthorizationKeyId(request.new_authorization_key);
    const auto account = request.account_id.Value();
    const auto package_id = ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{network_id.begin(), 32},
        std::span<const unsigned char, 32>{account.begin(), 32}, request.key_epoch, request.new_kem_package);
    if (!old_id || !new_id || !new_auth_id || !package_id) return IdentityRegistryError::INVALID_KEY;
    if (m_recovery_index.contains(*new_id)) return IdentityRegistryError::RECOVERY_KEY_EXISTS;
    const auto digest = ComputeIdentityRotateDigest(network_id, request);
    if (!digest || !VerifyIdentityMessage(record.recovery_key, request.old_recovery_signature, *digest) ||
        !VerifyIdentityMessage(request.new_recovery_key, request.new_recovery_pop, *digest) ||
        !VerifyIdentityMessage(request.new_authorization_key, request.new_authorization_pop, *digest)) {
        return IdentityRegistryError::INVALID_SIGNATURE;
    }

    m_recovery_index.erase(*old_id);
    m_recovery_index.emplace(*new_id, request.account_id);
    record.recovery_key = request.new_recovery_key;
    record.authorization_key = request.new_authorization_key;
    record.kem_package_id = *package_id;
    ++record.nonce;
    record.key_epoch = request.key_epoch;
    return IdentityRegistryError::NONE;
}

IdentityRegistryError IdentityRegistry::AuthorizeOperation(
    const IdentityOperationAuthorization& request, const uint256& network_id)
{
    auto account = m_accounts.find(request.account_id);
    if (account == m_accounts.end()) return IdentityRegistryError::ACCOUNT_NOT_FOUND;
    auto& record = account->second;
    if (request.nonce != record.nonce || request.key_epoch != record.key_epoch) return IdentityRegistryError::BAD_NONCE;
    if (record.nonce == std::numeric_limits<uint64_t>::max()) return IdentityRegistryError::NONCE_EXHAUSTED;
    const auto digest = ComputeIdentityOperationDigest(network_id, request);
    if (!digest) return IdentityRegistryError::INVALID_PAYLOAD;
    if (!VerifyIdentityMessage(record.authorization_key, request.signature, *digest)) return IdentityRegistryError::INVALID_SIGNATURE;
    ++record.nonce;
    return IdentityRegistryError::NONE;
}

std::optional<AccountId> IdentityRegistry::FindByRecoveryKeyId(const IdentityKeyId& id) const
{
    const auto it = m_recovery_index.find(id);
    if (it == m_recovery_index.end()) return std::nullopt;
    return it->second;
}

const IdentityRecord* IdentityRegistry::Find(const AccountId& id) const
{
    const auto it = m_accounts.find(id);
    return it == m_accounts.end() ? nullptr : &it->second;
}

} // namespace cybou
