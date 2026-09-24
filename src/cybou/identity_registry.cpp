// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_registry.h>

#include <openssl/evp.h>

#include <array>
#include <algorithm>
#include <limits>
#include <memory>
#include <string_view>

namespace cybou {
namespace {
using DigestCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

std::optional<IdentityKeyId> Digest(std::string_view domain, const uint256& network_id,
    const AccountId& account_id, uint64_t nonce, const IdentityKeyId& key_id)
{
    if (network_id.IsNull() || account_id.IsNull()) return std::nullopt;
    std::array<unsigned char, 8> nonce_le{};
    for (size_t i{0}; i < nonce_le.size(); ++i) nonce_le[i] = static_cast<unsigned char>(nonce >> (8 * i));
    DigestCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    IdentityKeyId result{};
    unsigned int size{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), network_id.begin(), network_id.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), account_id.Value().begin(), AccountId::SIZE) != 1 ||
        EVP_DigestUpdate(ctx.get(), nonce_le.data(), nonce_le.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), key_id.data(), key_id.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), result.data(), &size) != 1 || size != result.size()) return std::nullopt;
    return result;
}
} // namespace

std::optional<IdentityKeyId> ComputeDeviceAddDigest(const uint256& network_id, const DeviceAdd& request)
{
    const auto id = ComputeDeviceKeyId(request.new_device);
    if (!id) return std::nullopt;
    return Digest("CYBOU/DEVICE-ADD/V2", network_id, request.account_id, request.root_nonce, *id);
}

std::optional<IdentityKeyId> ComputeDeviceRevokeDigest(const uint256& network_id, const DeviceRevoke& request)
{
    return Digest("CYBOU/DEVICE-REVOKE/V2", network_id, request.account_id, request.root_nonce, request.device_id);
}

std::optional<IdentityKeyId> ComputeRecoveryRotateDigest(const uint256& network_id, const RecoveryRotate& request)
{
    const auto id = ComputeRecoveryKeyId(request.new_root);
    if (!id) return std::nullopt;
    return Digest("CYBOU/RECOVERY-ROTATE/V2", network_id, request.account_id, request.root_nonce, *id);
}

std::optional<IdentityKeyId> ComputeDeviceOperationDigest(const uint256& network_id, const DeviceAuthorization& request)
{
    const auto kind = static_cast<uint8_t>(request.kind);
    if (kind < 1 || kind > 5 ||
        std::all_of(request.device_id.begin(), request.device_id.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(request.payload_commitment.begin(), request.payload_commitment.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;
    if (network_id.IsNull() || request.account_id.IsNull()) return std::nullopt;
    std::array<unsigned char, 8> nonce_le{}, activation_le{};
    for (size_t i{0}; i < nonce_le.size(); ++i) nonce_le[i] = static_cast<unsigned char>(request.nonce >> (8 * i));
    for (size_t i{0}; i < activation_le.size(); ++i) activation_le[i] = static_cast<unsigned char>(request.activation_nonce >> (8 * i));
    constexpr std::string_view domain{"CYBOU/DEVICE-OP/V2"};
    DigestCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    IdentityKeyId result{};
    unsigned int size{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), network_id.begin(), network_id.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), request.account_id.Value().begin(), AccountId::SIZE) != 1 ||
        EVP_DigestUpdate(ctx.get(), request.device_id.data(), request.device_id.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), nonce_le.data(), nonce_le.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), activation_le.data(), activation_le.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), &kind, sizeof(kind)) != 1 ||
        EVP_DigestUpdate(ctx.get(), request.payload_commitment.data(), request.payload_commitment.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), result.data(), &size) != 1 || size != result.size()) return std::nullopt;
    return result;
}

IdentityRegistryError IdentityRegistry::Register(const AccountCreateOp& create,
    const uint256& network_id, uint64_t block_height, const CybouProtocolParameters& params)
{
    if (ValidateAccountCreateOp(create, network_id, block_height, params) != AccountCreateError::NONE) return IdentityRegistryError::INVALID_CREATE;
    if (m_accounts.contains(create.account_id)) return IdentityRegistryError::ACCOUNT_EXISTS;
    const auto root_id = ComputeRecoveryKeyId(create.authorization.recovery_root);
    const auto device_id = ComputeDeviceKeyId(create.authorization.initial_device);
    if (!root_id || !device_id) return IdentityRegistryError::INVALID_KEY;
    if (m_recovery_index.contains(*root_id)) return IdentityRegistryError::RECOVERY_KEY_EXISTS;
    IdentityRecord record{};
    record.recovery_root = create.authorization.recovery_root;
    record.devices.emplace(*device_id, IdentityDevice{create.authorization.initial_device, 0, 0});
    m_accounts.emplace(create.account_id, std::move(record));
    m_recovery_index.emplace(*root_id, create.account_id);
    return IdentityRegistryError::NONE;
}

IdentityRegistryError IdentityRegistry::AddDevice(const DeviceAdd& request, const uint256& network_id)
{
    auto it = m_accounts.find(request.account_id);
    if (it == m_accounts.end()) return IdentityRegistryError::ACCOUNT_NOT_FOUND;
    auto& record = it->second;
    const auto id = ComputeDeviceKeyId(request.new_device);
    if (!id || request.new_device.ed25519 == record.recovery_root.ed25519) return IdentityRegistryError::INVALID_KEY;
    if (record.devices.contains(*id)) return IdentityRegistryError::DEVICE_EXISTS;
    if (record.devices.size() >= MAX_ACTIVE_DEVICES) return IdentityRegistryError::DEVICE_LIMIT;
    if (request.root_nonce != record.next_root_nonce) return IdentityRegistryError::BAD_NONCE;
    if (record.next_root_nonce == std::numeric_limits<uint64_t>::max()) return IdentityRegistryError::NONCE_EXHAUSTED;
    const auto digest = ComputeDeviceAddDigest(network_id, request);
    if (!digest || !VerifyIdentityMessage(record.recovery_root, request.root_signature, *digest) ||
        !VerifyIdentityMessage(request.new_device, request.device_pop, *digest)) return IdentityRegistryError::INVALID_SIGNATURE;
    record.devices.emplace(*id, IdentityDevice{request.new_device, 0, record.next_root_nonce + 1});
    ++record.next_root_nonce;
    return IdentityRegistryError::NONE;
}

IdentityRegistryError IdentityRegistry::RevokeDevice(const DeviceRevoke& request, const uint256& network_id)
{
    auto it = m_accounts.find(request.account_id);
    if (it == m_accounts.end()) return IdentityRegistryError::ACCOUNT_NOT_FOUND;
    auto& record = it->second;
    if (!record.devices.contains(request.device_id)) return IdentityRegistryError::DEVICE_NOT_FOUND;
    if (request.root_nonce != record.next_root_nonce) return IdentityRegistryError::BAD_NONCE;
    if (record.next_root_nonce == std::numeric_limits<uint64_t>::max()) return IdentityRegistryError::NONCE_EXHAUSTED;
    const auto digest = ComputeDeviceRevokeDigest(network_id, request);
    if (!digest || !VerifyIdentityMessage(record.recovery_root, request.root_signature, *digest)) return IdentityRegistryError::INVALID_SIGNATURE;
    record.devices.erase(request.device_id);
    ++record.next_root_nonce;
    return IdentityRegistryError::NONE;
}

IdentityRegistryError IdentityRegistry::RotateRecovery(const RecoveryRotate& request, const uint256& network_id)
{
    auto it = m_accounts.find(request.account_id);
    if (it == m_accounts.end()) return IdentityRegistryError::ACCOUNT_NOT_FOUND;
    auto& record = it->second;
    const auto old_id = ComputeRecoveryKeyId(record.recovery_root);
    const auto new_id = ComputeRecoveryKeyId(request.new_root);
    if (!old_id || !new_id) return IdentityRegistryError::INVALID_KEY;
    for (const auto& [id, device] : record.devices) {
        if (device.key.ed25519 == request.new_root.ed25519) return IdentityRegistryError::INVALID_KEY;
    }
    if (m_recovery_index.contains(*new_id)) return IdentityRegistryError::RECOVERY_KEY_EXISTS;
    if (request.root_nonce != record.next_root_nonce) return IdentityRegistryError::BAD_NONCE;
    if (record.next_root_nonce == std::numeric_limits<uint64_t>::max()) return IdentityRegistryError::NONCE_EXHAUSTED;
    const auto digest = ComputeRecoveryRotateDigest(network_id, request);
    if (!digest || !VerifyIdentityMessage(record.recovery_root, request.old_root_signature, *digest) ||
        !VerifyIdentityMessage(request.new_root, request.new_root_pop, *digest)) return IdentityRegistryError::INVALID_SIGNATURE;
    m_recovery_index.erase(*old_id);
    m_recovery_index.emplace(*new_id, request.account_id);
    record.recovery_root = request.new_root;
    ++record.next_root_nonce;
    return IdentityRegistryError::NONE;
}

IdentityRegistryError IdentityRegistry::AuthorizeDeviceOperation(const DeviceAuthorization& request, const uint256& network_id)
{
    auto account = m_accounts.find(request.account_id);
    if (account == m_accounts.end()) return IdentityRegistryError::ACCOUNT_NOT_FOUND;
    auto device = account->second.devices.find(request.device_id);
    if (device == account->second.devices.end()) return IdentityRegistryError::DEVICE_NOT_FOUND;
    if (request.activation_nonce != device->second.activation_nonce) return IdentityRegistryError::BAD_NONCE;
    if (request.nonce != device->second.next_nonce) return IdentityRegistryError::BAD_NONCE;
    if (device->second.next_nonce == std::numeric_limits<uint64_t>::max()) return IdentityRegistryError::NONCE_EXHAUSTED;
    const auto digest = ComputeDeviceOperationDigest(network_id, request);
    if (!digest) return IdentityRegistryError::INVALID_PAYLOAD;
    if (!VerifyIdentityMessage(device->second.key, request.signature, *digest)) return IdentityRegistryError::INVALID_SIGNATURE;
    ++device->second.next_nonce;
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
