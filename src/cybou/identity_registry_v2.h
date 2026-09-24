// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_REGISTRY_V2_H
#define CYBOU_IDENTITY_REGISTRY_V2_H

#include <cybou/account_creation_v2.h>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

using IdentityKeyIdV2 = std::array<unsigned char, 32>;
inline constexpr size_t MAX_ACTIVE_DEVICES_V2{8};
inline constexpr uint32_t MAX_IDENTITY_REGISTRY_ACCOUNTS_V2{1'000'000};

struct IdentityDeviceV2 {
    IdentityHybridPublicKey key;
    uint64_t next_nonce{0};
    uint64_t activation_nonce{0};
};

struct IdentityRecordV2 {
    IdentityHybridPublicKey recovery_root;
    uint64_t next_root_nonce{0};
    std::map<IdentityKeyIdV2, IdentityDeviceV2> devices;
};

struct DeviceAddV2 {
    AccountId account_id;
    IdentityHybridPublicKey new_device;
    uint64_t root_nonce{0};
    IdentityHybridSignature root_signature;
    IdentityHybridSignature device_pop;
};

struct DeviceRevokeV2 {
    AccountId account_id;
    IdentityKeyIdV2 device_id{};
    uint64_t root_nonce{0};
    IdentityHybridSignature root_signature;
};

struct RecoveryRotateV2 {
    AccountId account_id;
    IdentityHybridPublicKey new_root;
    uint64_t root_nonce{0};
    IdentityHybridSignature old_root_signature;
    IdentityHybridSignature new_root_pop;
};

enum class DeviceOperationKindV2 : uint8_t { PAYMENT = 1, MAIL = 2, SYSTEM_LOCK = 3 };

// The payload commitment must be computed from the canonical, complete V2
// payload encoding by its operation-specific validator.
struct DeviceAuthorizationV2 {
    AccountId account_id;
    IdentityKeyIdV2 device_id{};
    uint64_t nonce{0};
    uint64_t activation_nonce{0};
    DeviceOperationKindV2 kind{DeviceOperationKindV2::PAYMENT};
    IdentityKeyIdV2 payload_commitment{};
    IdentityHybridSignature signature;
};

enum class IdentityRegistryErrorV2 : uint8_t {
    NONE,
    INVALID_CREATE,
    ACCOUNT_EXISTS,
    RECOVERY_KEY_EXISTS,
    ACCOUNT_NOT_FOUND,
    INVALID_KEY,
    DEVICE_EXISTS,
    DEVICE_NOT_FOUND,
    DEVICE_LIMIT,
    BAD_NONCE,
    NONCE_EXHAUSTED,
    INVALID_SIGNATURE,
    INVALID_PAYLOAD,
};

std::optional<std::array<unsigned char, 32>> ComputeDeviceAddDigestV2(
    const uint256& network_id, const DeviceAddV2& request);
std::optional<std::array<unsigned char, 32>> ComputeDeviceRevokeDigestV2(
    const uint256& network_id, const DeviceRevokeV2& request);
std::optional<std::array<unsigned char, 32>> ComputeRecoveryRotateDigestV2(
    const uint256& network_id, const RecoveryRotateV2& request);
std::optional<std::array<unsigned char, 32>> ComputeDeviceOperationDigestV2(
    const uint256& network_id, const DeviceAuthorizationV2& request);

class IdentityRegistryV2
{
public:
    IdentityRegistryErrorV2 Register(const AccountCreateOpV2& create,
        const uint256& network_id, uint64_t block_height,
        const CybouProtocolParameters& params);
    IdentityRegistryErrorV2 AddDevice(const DeviceAddV2& request, const uint256& network_id);
    IdentityRegistryErrorV2 RevokeDevice(const DeviceRevokeV2& request, const uint256& network_id);
    IdentityRegistryErrorV2 RotateRecovery(const RecoveryRotateV2& request, const uint256& network_id);
    // Apply only to a throwaway candidate registry. Commit it together with the
    // validated payment/Mail/system state transition, never before it.
    IdentityRegistryErrorV2 AuthorizeDeviceOperation(const DeviceAuthorizationV2& request, const uint256& network_id);

    std::optional<AccountId> FindByRecoveryKeyId(const IdentityKeyIdV2& id) const;
    const IdentityRecordV2* Find(const AccountId& id) const;

    friend std::optional<std::vector<unsigned char>> SerializeIdentityRegistryV2(const IdentityRegistryV2& registry);
    friend std::optional<IdentityRegistryV2> DeserializeIdentityRegistryV2(std::span<const unsigned char> bytes);

private:
    std::map<AccountId, IdentityRecordV2> m_accounts;
    std::map<IdentityKeyIdV2, AccountId> m_recovery_index;
};

// Standalone canonical snapshot component. The enclosing V2 state format and
// state-root domain will be defined when monetary and name state are integrated.
std::optional<std::vector<unsigned char>> SerializeIdentityRegistryV2(const IdentityRegistryV2& registry);
std::optional<IdentityRegistryV2> DeserializeIdentityRegistryV2(std::span<const unsigned char> bytes);

} // namespace cybou
#endif
