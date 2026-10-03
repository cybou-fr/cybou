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
inline constexpr size_t IDENTITY_OPERATION_AUTH_SIZE{2565};
inline constexpr uint32_t MAX_IDENTITY_REGISTRY_ACCOUNTS{1'000'000};

/** Protocol identity: mnemonic-derived key roles, never a machine or device. */
struct IdentityRecord {
    IdentityHybridPublicKey recovery_key;
    IdentityHybridPublicKey authorization_key;
    std::array<unsigned char, 32> kem_package_id{};
    uint64_t nonce{0};
    uint64_t key_epoch{0};

    friend bool operator==(const IdentityRecord&, const IdentityRecord&) = default;
};

/** Atomically rotate every public capability derived from a new recovery phrase. */
struct IdentityRotate {
    AccountId account_id;
    IdentityHybridPublicKey new_recovery_key;
    IdentityHybridPublicKey new_authorization_key;
    IdentityKemPackage new_kem_package{};
    uint64_t nonce{0};
    uint64_t key_epoch{0};
    IdentityHybridSignature old_recovery_signature;
    IdentityHybridSignature new_recovery_pop;
    IdentityHybridSignature new_authorization_pop;

    friend bool operator==(const IdentityRotate&, const IdentityRotate&) = default;
};

enum class IdentityOperationKind : uint8_t {
    PAYMENT = 1, SYSTEM_LOCK = 2, NAME_COMMIT = 3, NAME_REVEAL = 4,
    ROOT_PUBLICATION = 5,
};

struct IdentityOperationAuthorization {
    AccountId account_id;
    uint64_t nonce{0};
    uint64_t key_epoch{0};
    IdentityOperationKind kind{IdentityOperationKind::PAYMENT};
    IdentityKeyId payload_commitment{};
    IdentityHybridSignature signature;

    friend bool operator==(const IdentityOperationAuthorization&, const IdentityOperationAuthorization&) = default;
};

enum class IdentityRegistryError : uint8_t {
    NONE,
    INVALID_CREATE,
    ACCOUNT_EXISTS,
    RECOVERY_KEY_EXISTS,
    ACCOUNT_NOT_FOUND,
    INVALID_KEY,
    BAD_NONCE,
    NONCE_EXHAUSTED,
    INVALID_SIGNATURE,
    INVALID_PAYLOAD,
};

std::optional<IdentityKeyId> ComputeIdentityRotateDigest(
    const cybou::Hash256& network_binding, const IdentityRotate& request);
std::optional<IdentityKeyId> ComputeIdentityOperationDigest(
    const cybou::Hash256& network_binding, const IdentityOperationAuthorization& request);

class IdentityRegistry
{
public:
    IdentityRegistryError Register(const AccountCreateOp& create,
        const cybou::Hash256& network_binding, uint64_t block_height,
        const CybouProtocolParameters& params);
    IdentityRegistryError RotateIdentity(const IdentityRotate& request, const cybou::Hash256& network_binding);
    IdentityRegistryError AuthorizeOperation(const IdentityOperationAuthorization& request, const cybou::Hash256& network_binding);

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
