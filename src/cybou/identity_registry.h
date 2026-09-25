// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_REGISTRY_H
#define CYBOU_IDENTITY_REGISTRY_H

#include <cybou/account_creation.h>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

using IdentityKeyId = std::array<unsigned char, 32>;
inline constexpr size_t MAX_ACTIVE_DEVICES{8};
inline constexpr uint32_t MAX_IDENTITY_REGISTRY_ACCOUNTS{1'000'000};

struct IdentityDevice {
    IdentityHybridPublicKey key;
    uint64_t next_nonce{0};
    uint64_t activation_nonce{0};

    friend bool operator==(const IdentityDevice&, const IdentityDevice&) = default;
};

struct IdentityRecord {
    IdentityHybridPublicKey recovery_root;
    uint64_t next_root_nonce{0};
    std::map<IdentityKeyId, IdentityDevice> devices;

    friend bool operator==(const IdentityRecord&, const IdentityRecord&) = default;
};

struct DeviceAdd {
    AccountId account_id;
    IdentityHybridPublicKey new_device;
    uint64_t root_nonce{0};
    IdentityHybridSignature root_signature;
    IdentityHybridSignature device_pop;

    friend bool operator==(const DeviceAdd&, const DeviceAdd&) = default;
};

struct DeviceRevoke {
    AccountId account_id;
    IdentityKeyId device_id{};
    uint64_t root_nonce{0};
    IdentityHybridSignature root_signature;

    friend bool operator==(const DeviceRevoke&, const DeviceRevoke&) = default;
};

struct RecoveryRotate {
    AccountId account_id;
    IdentityHybridPublicKey new_root;
    uint64_t root_nonce{0};
    IdentityHybridSignature old_root_signature;
    IdentityHybridSignature new_root_pop;

    friend bool operator==(const RecoveryRotate&, const RecoveryRotate&) = default;
};

enum class DeviceOperationKind : uint8_t { PAYMENT = 1, MAIL = 2, SYSTEM_LOCK = 3, NAME_COMMIT = 4, NAME_REVEAL = 5 };

struct DeviceAuthorization {
    AccountId account_id;
    IdentityKeyId device_id{};
    uint64_t nonce{0};
    uint64_t activation_nonce{0};
    DeviceOperationKind kind{DeviceOperationKind::PAYMENT};
    IdentityKeyId payload_commitment{};
    IdentityHybridSignature signature;

    friend bool operator==(const DeviceAuthorization&, const DeviceAuthorization&) = default;
};

enum class IdentityRegistryError : uint8_t {
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

std::optional<std::array<unsigned char, 32>> ComputeDeviceAddDigest(
    const uint256& network_id, const DeviceAdd& request);
std::optional<std::array<unsigned char, 32>> ComputeDeviceRevokeDigest(
    const uint256& network_id, const DeviceRevoke& request);
std::optional<std::array<unsigned char, 32>> ComputeRecoveryRotateDigest(
    const uint256& network_id, const RecoveryRotate& request);
std::optional<std::array<unsigned char, 32>> ComputeDeviceOperationDigest(
    const uint256& network_id, const DeviceAuthorization& request);

class IdentityRegistry
{
public:
    IdentityRegistryError Register(const AccountCreateOp& create,
        const uint256& network_id, uint64_t block_height,
        const CybouProtocolParameters& params);
    IdentityRegistryError AddDevice(const DeviceAdd& request, const uint256& network_id);
    IdentityRegistryError RevokeDevice(const DeviceRevoke& request, const uint256& network_id);
    IdentityRegistryError RotateRecovery(const RecoveryRotate& request, const uint256& network_id);
    IdentityRegistryError AuthorizeDeviceOperation(const DeviceAuthorization& request, const uint256& network_id);

    std::optional<AccountId> FindByRecoveryKeyId(const IdentityKeyId& id) const;
    const IdentityRecord* Find(const AccountId& id) const;
    const std::map<AccountId, IdentityRecord>& Accounts() const { return m_accounts; }

    friend std::optional<std::vector<unsigned char>> SerializeIdentityRegistry(const IdentityRegistry& registry);
    friend std::optional<IdentityRegistry> DeserializeIdentityRegistry(std::span<const unsigned char> bytes);

private:
    std::map<AccountId, IdentityRecord> m_accounts;
    std::map<IdentityKeyId, AccountId> m_recovery_index;
};

std::optional<std::vector<unsigned char>> SerializeIdentityRegistry(const IdentityRegistry& registry);
std::optional<IdentityRegistry> DeserializeIdentityRegistry(std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_IDENTITY_REGISTRY_H
