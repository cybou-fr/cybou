// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_VALIDATION_SERVICE_H
#define CYBOU_VALIDATION_SERVICE_H

#include <cybou/kv_store.h>
#include <cybou/network_definition.h>
#include <cybou/protocol_operation.h>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>

namespace cybou {

struct ValidationBase {
    uint256 network_id;
    uint64_t height{0};
    uint256 block_id;
    uint256 state_root;
    friend bool operator==(const ValidationBase&, const ValidationBase&) = default;
};
struct FinalizedValidationSnapshot {
    ValidationBase base;
    CybouState state;
    CybouProtocolParameters parameters;
};
enum class ValidationCheck : uint8_t {
    VALID, INVALID, CONFLICT, BASE_CHANGED, UNAVAILABLE, UNSUPPORTED,
};
struct ValidationAttestation {
    ValidationBase base;
    uint256 operation_id;
    uint256 node_id;
    IdentityHybridSignature signature;
    friend bool operator==(const ValidationAttestation&, const ValidationAttestation&) = default;
};
struct ValidationResult {
    ValidationCheck check{ValidationCheck::UNAVAILABLE};
    std::optional<ValidationAttestation> attestation;
};

std::optional<uint256> ValidationNodeId(const IdentityHybridPublicKey& key);
std::optional<IdentityKeyId> ValidationAttestationDigest(const ValidationAttestation& value);
std::optional<std::vector<unsigned char>> SerializeValidationAttestation(const ValidationAttestation& value);
std::optional<ValidationAttestation> DeserializeValidationAttestation(std::span<const unsigned char> bytes);
ValidationCheck ValidateAgainstFinalizedBase(const ProtocolOperation& operation, const FinalizedValidationSnapshot& snapshot);
bool VerifyValidationAttestation(const ValidationAttestation& value, const ProtocolOperation& operation,
    const FinalizedValidationSnapshot& snapshot, const IdentityHybridPublicKey& node_key);

/** Dedicated durable database. One base, bounded reservations, synchronous writes before signing.
 * A failed/corrupt write permanently disables this instance. Never use the canonical database. */
class ValidationState final {
public:
    ValidationState(KVStore& db, uint256 network_id, uint256 node_id);
    ValidationCheck Reserve(const ValidationBase& base, const AccountId& account, uint64_t nonce, const uint256& operation);
private:
    bool Load();
    bool Save();
    KVStore& m_db;
    uint256 m_network, m_node;
    std::optional<ValidationBase> m_base;
    std::map<std::pair<AccountId, uint64_t>, uint256> m_reservations;
    bool m_available{false};
    std::mutex m_mutex;
};

/** Runtime calls the callback while holding its canonical-state lock through signing. */
using ValidationSnapshotReader = std::function<bool(const std::function<void(const FinalizedValidationSnapshot&)>&)>;
class ValidationService final {
public:
    using Signer = std::function<std::optional<IdentityHybridSignature>(std::span<const unsigned char>)>;
    ValidationService(ValidationState& state, IdentityHybridPublicKey key, Signer signer, ValidationSnapshotReader reader);
    ValidationResult CheckReserveAndAttest(const ProtocolOperation& operation);
private:
    ValidationState& m_state;
    IdentityHybridPublicKey m_key;
    Signer m_signer;
    ValidationSnapshotReader m_reader;
};

/** Local policy defaults OFF. Counts distinct trusted Accounts, never raw NodeIDs. */
struct ValidationTrustPolicy {
    size_t required_accounts{0};
    struct TrustedNode { AccountId account; IdentityHybridPublicKey key; };
    std::map<uint256, TrustedNode> nodes;
};
bool HasLocalValidation(const ProtocolOperation& operation, const FinalizedValidationSnapshot& snapshot,
    std::span<const ValidationAttestation> attestations, const ValidationTrustPolicy& policy);
} // namespace cybou
#endif
