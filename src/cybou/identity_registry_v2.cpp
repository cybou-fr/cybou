// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_registry_v2.h>

#include <openssl/evp.h>

#include <array>
#include <algorithm>
#include <limits>
#include <memory>
#include <string_view>

namespace cybou {
namespace {
using DigestCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

std::optional<IdentityKeyIdV2> Digest(std::string_view domain, const uint256& network_id,
    const AccountId& account_id, uint64_t nonce, const IdentityKeyIdV2& key_id)
{
    if (network_id.IsNull() || account_id.IsNull()) return std::nullopt;
    std::array<unsigned char, 8> nonce_le{};
    for (size_t i{0}; i < nonce_le.size(); ++i) nonce_le[i] = static_cast<unsigned char>(nonce >> (8 * i));
    DigestCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    IdentityKeyIdV2 result{};
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

std::optional<IdentityKeyIdV2> ComputeDeviceAddDigestV2(const uint256& network_id, const DeviceAddV2& request)
{
    const auto id = ComputeDeviceKeyId(request.new_device);
    if (!id) return std::nullopt;
    return Digest("CYBOU/DEVICE-ADD/V2", network_id, request.account_id, request.root_nonce, *id);
}

std::optional<IdentityKeyIdV2> ComputeDeviceRevokeDigestV2(const uint256& network_id, const DeviceRevokeV2& request)
{
    return Digest("CYBOU/DEVICE-REVOKE/V2", network_id, request.account_id, request.root_nonce, request.device_id);
}

std::optional<IdentityKeyIdV2> ComputeRecoveryRotateDigestV2(const uint256& network_id, const RecoveryRotateV2& request)
{
    const auto id = ComputeRecoveryKeyId(request.new_root);
    if (!id) return std::nullopt;
    return Digest("CYBOU/RECOVERY-ROTATE/V2", network_id, request.account_id, request.root_nonce, *id);
}

std::optional<IdentityKeyIdV2> ComputeDeviceOperationDigestV2(const uint256& network_id, const DeviceAuthorizationV2& request)
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
    IdentityKeyIdV2 result{};
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

IdentityRegistryErrorV2 IdentityRegistryV2::Register(const AccountCreateOpV2& create,
    const uint256& network_id, uint64_t block_height, const CybouProtocolParameters& params)
{
    if (ValidateAccountCreateOpV2(create, network_id, block_height, params) != AccountCreateV2Error::NONE) return IdentityRegistryErrorV2::INVALID_CREATE;
    if (m_accounts.contains(create.account_id)) return IdentityRegistryErrorV2::ACCOUNT_EXISTS;
    const auto root_id = ComputeRecoveryKeyId(create.authorization.recovery_root);
    const auto device_id = ComputeDeviceKeyId(create.authorization.initial_device);
    if (!root_id || !device_id) return IdentityRegistryErrorV2::INVALID_KEY;
    if (m_recovery_index.contains(*root_id)) return IdentityRegistryErrorV2::RECOVERY_KEY_EXISTS;
    IdentityRecordV2 record{};
    record.recovery_root = create.authorization.recovery_root;
    record.devices.emplace(*device_id, IdentityDeviceV2{create.authorization.initial_device, 0, 0});
    m_accounts.emplace(create.account_id, std::move(record));
    m_recovery_index.emplace(*root_id, create.account_id);
    return IdentityRegistryErrorV2::NONE;
}

IdentityRegistryErrorV2 IdentityRegistryV2::AddDevice(const DeviceAddV2& request, const uint256& network_id)
{
    auto it = m_accounts.find(request.account_id);
    if (it == m_accounts.end()) return IdentityRegistryErrorV2::ACCOUNT_NOT_FOUND;
    auto& record = it->second;
    const auto id = ComputeDeviceKeyId(request.new_device);
    if (!id || request.new_device.ed25519 == record.recovery_root.ed25519) return IdentityRegistryErrorV2::INVALID_KEY;
    if (record.devices.contains(*id)) return IdentityRegistryErrorV2::DEVICE_EXISTS;
    if (record.devices.size() >= MAX_ACTIVE_DEVICES_V2) return IdentityRegistryErrorV2::DEVICE_LIMIT;
    if (request.root_nonce != record.next_root_nonce) return IdentityRegistryErrorV2::BAD_NONCE;
    if (record.next_root_nonce == std::numeric_limits<uint64_t>::max()) return IdentityRegistryErrorV2::NONCE_EXHAUSTED;
    const auto digest = ComputeDeviceAddDigestV2(network_id, request);
    if (!digest || !VerifyIdentityMessage(record.recovery_root, request.root_signature, *digest) ||
        !VerifyIdentityMessage(request.new_device, request.device_pop, *digest)) return IdentityRegistryErrorV2::INVALID_SIGNATURE;
    record.devices.emplace(*id, IdentityDeviceV2{request.new_device, 0, record.next_root_nonce + 1});
    ++record.next_root_nonce;
    return IdentityRegistryErrorV2::NONE;
}

IdentityRegistryErrorV2 IdentityRegistryV2::RevokeDevice(const DeviceRevokeV2& request, const uint256& network_id)
{
    auto it = m_accounts.find(request.account_id);
    if (it == m_accounts.end()) return IdentityRegistryErrorV2::ACCOUNT_NOT_FOUND;
    auto& record = it->second;
    if (!record.devices.contains(request.device_id)) return IdentityRegistryErrorV2::DEVICE_NOT_FOUND;
    if (request.root_nonce != record.next_root_nonce) return IdentityRegistryErrorV2::BAD_NONCE;
    if (record.next_root_nonce == std::numeric_limits<uint64_t>::max()) return IdentityRegistryErrorV2::NONCE_EXHAUSTED;
    const auto digest = ComputeDeviceRevokeDigestV2(network_id, request);
    if (!digest || !VerifyIdentityMessage(record.recovery_root, request.root_signature, *digest)) return IdentityRegistryErrorV2::INVALID_SIGNATURE;
    record.devices.erase(request.device_id);
    ++record.next_root_nonce;
    return IdentityRegistryErrorV2::NONE;
}

IdentityRegistryErrorV2 IdentityRegistryV2::RotateRecovery(const RecoveryRotateV2& request, const uint256& network_id)
{
    auto it = m_accounts.find(request.account_id);
    if (it == m_accounts.end()) return IdentityRegistryErrorV2::ACCOUNT_NOT_FOUND;
    auto& record = it->second;
    const auto old_id = ComputeRecoveryKeyId(record.recovery_root);
    const auto new_id = ComputeRecoveryKeyId(request.new_root);
    if (!old_id || !new_id) return IdentityRegistryErrorV2::INVALID_KEY;
    for (const auto& [id, device] : record.devices) {
        if (device.key.ed25519 == request.new_root.ed25519) return IdentityRegistryErrorV2::INVALID_KEY;
    }
    if (m_recovery_index.contains(*new_id)) return IdentityRegistryErrorV2::RECOVERY_KEY_EXISTS;
    if (request.root_nonce != record.next_root_nonce) return IdentityRegistryErrorV2::BAD_NONCE;
    if (record.next_root_nonce == std::numeric_limits<uint64_t>::max()) return IdentityRegistryErrorV2::NONCE_EXHAUSTED;
    const auto digest = ComputeRecoveryRotateDigestV2(network_id, request);
    if (!digest || !VerifyIdentityMessage(record.recovery_root, request.old_root_signature, *digest) ||
        !VerifyIdentityMessage(request.new_root, request.new_root_pop, *digest)) return IdentityRegistryErrorV2::INVALID_SIGNATURE;
    m_recovery_index.erase(*old_id);
    m_recovery_index.emplace(*new_id, request.account_id);
    record.recovery_root = request.new_root;
    ++record.next_root_nonce;
    return IdentityRegistryErrorV2::NONE;
}

IdentityRegistryErrorV2 IdentityRegistryV2::AuthorizeDeviceOperation(const DeviceAuthorizationV2& request, const uint256& network_id)
{
    auto account = m_accounts.find(request.account_id);
    if (account == m_accounts.end()) return IdentityRegistryErrorV2::ACCOUNT_NOT_FOUND;
    auto device = account->second.devices.find(request.device_id);
    if (device == account->second.devices.end()) return IdentityRegistryErrorV2::DEVICE_NOT_FOUND;
    if (request.activation_nonce != device->second.activation_nonce) return IdentityRegistryErrorV2::BAD_NONCE;
    if (request.nonce != device->second.next_nonce) return IdentityRegistryErrorV2::BAD_NONCE;
    if (device->second.next_nonce == std::numeric_limits<uint64_t>::max()) return IdentityRegistryErrorV2::NONCE_EXHAUSTED;
    const auto digest = ComputeDeviceOperationDigestV2(network_id, request);
    if (!digest) return IdentityRegistryErrorV2::INVALID_PAYLOAD;
    if (!VerifyIdentityMessage(device->second.key, request.signature, *digest)) return IdentityRegistryErrorV2::INVALID_SIGNATURE;
    ++device->second.next_nonce;
    return IdentityRegistryErrorV2::NONE;
}

std::optional<AccountId> IdentityRegistryV2::FindByRecoveryKeyId(const IdentityKeyIdV2& id) const
{
    const auto it = m_recovery_index.find(id);
    if (it == m_recovery_index.end()) return std::nullopt;
    return it->second;
}

const IdentityRecordV2* IdentityRegistryV2::Find(const AccountId& id) const
{
    const auto it = m_accounts.find(id);
    return it == m_accounts.end() ? nullptr : &it->second;
}
} // namespace cybou
