// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#include <cybou/validation_service.h>
#include <cybou/block_executor.h>
#include <cybou/crypto/sha256.h>
#include <cybou/protocol_limits.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace cybou {
namespace {
constexpr size_t MAX_RESERVATIONS{4096};
constexpr size_t ATTESTATION_BYTES{1 + 32 + 8 + 32 * 4 + 64 + 2420};
void U64(std::vector<unsigned char>& out, uint64_t value) { for (unsigned i{0}; i < 8; ++i) out.push_back(value >> (8 * i)); }
void Hash(std::vector<unsigned char>& out, const uint256& value) { out.insert(out.end(), value.begin(), value.end()); }
struct Reader {
    std::span<const unsigned char> bytes; size_t offset{0};
    bool Take(std::span<unsigned char> out) {
        if (out.size() > bytes.size() - offset) return false;
        std::copy_n(bytes.begin() + offset, out.size(), out.begin()); offset += out.size(); return true;
    }
    bool H(uint256& value) { return Take({value.begin(), 32}); }
    bool N(uint64_t& value) {
        std::array<unsigned char, 8> raw{}; if (!Take(raw)) return false;
        value = 0; for (unsigned i{0}; i < 8; ++i) value |= uint64_t{raw[i]} << (8 * i); return true;
    }
};
void Base(std::vector<unsigned char>& out, const ValidationBase& base) {
    Hash(out, base.network_id); U64(out, base.height); Hash(out, base.block_id); Hash(out, base.state_root);
}
bool ReadBase(Reader& in, ValidationBase& base) { return in.H(base.network_id) && in.N(base.height) && in.H(base.block_id) && in.H(base.state_root); }
std::optional<std::array<unsigned char, 32>> Digest(std::string_view domain, std::span<const unsigned char> bytes) {
    std::array<unsigned char, 32> out{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), bytes}, out.data())) return std::nullopt;
    return out;
}
std::vector<unsigned char> AttestationBody(const ValidationAttestation& value) {
    std::vector<unsigned char> out{1}; Base(out, value.base); Hash(out, value.operation_id); Hash(out, value.node_id); return out;
}
std::optional<std::pair<AccountId, uint64_t>> Nonce(const ProtocolOperation& operation) {
    return std::visit([](const auto& op) -> std::optional<std::pair<AccountId, uint64_t>> {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, AccountCreateOp> || std::is_same_v<T, ServiceEvidence>) return std::nullopt;
        else if constexpr (std::is_same_v<T, IdentityRotate>) return std::pair{op.account_id, op.nonce};
        else return std::pair{op.authorization.account_id, op.authorization.nonce};
    }, operation);
}
} // namespace

std::optional<uint256> ValidationNodeId(const IdentityHybridPublicKey& key) {
    if (key.purpose != IdentityKeyPurpose::VALIDATION_NODE || key.ml_dsa.size() != 1312 ||
        std::all_of(key.ed25519.begin(), key.ed25519.end(), [](auto b) { return b == 0; }) ||
        std::all_of(key.ml_dsa.begin(), key.ml_dsa.end(), [](auto b) { return b == 0; })) return std::nullopt;
    uint256 out;
    if (!crypto::ComputeSha256({crypto::Sha256Bytes("CYBOU/VALIDATION-NODE-ID/V1"), key.ed25519, key.ml_dsa}, out.begin())) return std::nullopt;
    return out;
}
std::optional<IdentityKeyId> ValidationAttestationDigest(const ValidationAttestation& value) {
    return Digest("CYBOU/VALIDATION-ATTESTATION/V1", AttestationBody(value));
}
std::optional<std::vector<unsigned char>> SerializeValidationAttestation(const ValidationAttestation& value) {
    if (value.base.network_id.IsNull() || value.base.block_id.IsNull() || value.base.state_root.IsNull() ||
        value.operation_id.IsNull() || value.node_id.IsNull() || value.signature.ml_dsa.size() != 2420) return std::nullopt;
    auto out = AttestationBody(value);
    out.insert(out.end(), value.signature.ed25519.begin(), value.signature.ed25519.end());
    out.insert(out.end(), value.signature.ml_dsa.begin(), value.signature.ml_dsa.end()); return out;
}
std::optional<ValidationAttestation> DeserializeValidationAttestation(std::span<const unsigned char> bytes) {
    if (bytes.size() != ATTESTATION_BYTES || bytes[0] != 1) return std::nullopt;
    Reader in{bytes, 1}; ValidationAttestation value;
    if (!ReadBase(in, value.base) || !in.H(value.operation_id) || !in.H(value.node_id) || !in.Take(value.signature.ed25519)) return std::nullopt;
    value.signature.ml_dsa.assign(bytes.begin() + in.offset, bytes.end());
    return SerializeValidationAttestation(value) ? std::optional{value} : std::nullopt;
}
ValidationCheck ValidateAgainstFinalizedBase(const ProtocolOperation& operation, const FinalizedValidationSnapshot& snapshot) {
    if (snapshot.base.network_id.IsNull() || snapshot.base.block_id.IsNull() ||
        snapshot.base.height == UINT64_MAX || CybouStateHash(snapshot.state) != snapshot.base.state_root) return ValidationCheck::UNAVAILABLE;
    if (!Nonce(operation)) return ValidationCheck::UNSUPPORTED;
    const auto bytes = SerializeProtocolOperation(operation);
    if (!bytes || bytes->size() > MAX_OPERATION_PAYLOAD_BYTES) return ValidationCheck::INVALID;
    const auto decoded = DeserializeProtocolOperation(*bytes);
    if (!decoded || *decoded != operation) return ValidationCheck::INVALID;
    // IdentityRecord.nonce is the NEXT admissible nonce. No pending overlay.
    return ExecuteBlockOperations(snapshot.state, {operation}, snapshot.base.network_id,
        snapshot.base.height + 1, snapshot.parameters) ? ValidationCheck::VALID : ValidationCheck::INVALID;
}
bool VerifyValidationAttestation(const ValidationAttestation& value, const ProtocolOperation& operation,
    const FinalizedValidationSnapshot& snapshot, const IdentityHybridPublicKey& node_key) {
    const auto bound = snapshot.state.bound_nodes.find(value.node_id);
    if (bound == snapshot.state.bound_nodes.end() || bound->second.key != node_key) return false;
    if (value.base != snapshot.base || ComputeOperationId(operation) != value.operation_id ||
        ValidationNodeId(node_key) != value.node_id || !SerializeValidationAttestation(value) ||
        ValidateAgainstFinalizedBase(operation, snapshot) != ValidationCheck::VALID) return false;
    const auto digest = ValidationAttestationDigest(value);
    return digest && VerifyIdentityMessage(node_key, value.signature, *digest);
}

ValidationState::ValidationState(KVStore& db, uint256 network, uint256 node)
    : m_db{db}, m_network{network}, m_node{node} {
    if (network.IsNull() || node.IsNull()) throw std::invalid_argument{"null validation journal identity"};
    try { m_available = Load(); } catch (...) { m_available = false; }
}
bool ValidationState::Load() {
    std::vector<unsigned char> bytes;
    if (!m_db.Read(std::string{"validation-state"}, bytes)) {
        if (m_db.Exists(std::string{"validation-state"})) return false;
        return Save();
    }
    if (bytes.size() < 1 + 64 + 8 + 32 || bytes.size() > 1 + 64 + 8 + 104 + MAX_RESERVATIONS * 72 + 32) return false;
    const auto content = std::span<const unsigned char>{bytes}.first(bytes.size() - 32);
    const auto digest = Digest("CYBOU/VALIDATION-STATE/V1", content);
    if (!digest || !std::equal(digest->begin(), digest->end(), bytes.end() - 32) || bytes[0] != 1) return false;
    Reader in{content, 1}; uint256 network, node; uint64_t count;
    if (!in.H(network) || !in.H(node) || network != m_network || node != m_node || !in.N(count) || count > MAX_RESERVATIONS + 1) return false;
    if (count != 0) {
        ValidationBase base; if (!ReadBase(in, base) || base.network_id != network || base.block_id.IsNull() || base.state_root.IsNull()) return false;
        m_base = base;
        std::optional<std::pair<AccountId, uint64_t>> previous;
        for (uint64_t i{0}; i < count - 1; ++i) {
            uint256 account, operation; uint64_t nonce;
            if (!in.H(account) || !in.N(nonce) || !in.H(operation) || account.IsNull() || operation.IsNull()) return false;
            const auto key = std::pair{AccountId{account}, nonce};
            if (previous && !(key > *previous)) return false;
            m_reservations.emplace(key, operation); previous = key;
        }
    }
    return in.offset == content.size();
}
bool ValidationState::Save() {
    std::vector<unsigned char> out{1}; Hash(out, m_network); Hash(out, m_node);
    U64(out, m_base ? m_reservations.size() + 1 : 0);
    if (m_base) { Base(out, *m_base); for (const auto& [key, operation] : m_reservations) { Hash(out, key.first.Value()); U64(out, key.second); Hash(out, operation); } }
    const auto digest = Digest("CYBOU/VALIDATION-STATE/V1", out); if (!digest) return false;
    out.insert(out.end(), digest->begin(), digest->end());
    m_db.Write(std::string{"validation-state"}, out, true); return true;
}
ValidationCheck ValidationState::Reserve(const ValidationBase& base, const AccountId& account, uint64_t nonce, const uint256& operation) {
    std::lock_guard lock{m_mutex};
    if (!m_available) return ValidationCheck::UNAVAILABLE;
    if (base.network_id != m_network || base.block_id.IsNull() || base.state_root.IsNull() || account.IsNull() || operation.IsNull()) return ValidationCheck::INVALID;
    if (m_base && base != *m_base && base.height <= m_base->height) return ValidationCheck::BASE_CHANGED;
    if (!m_base || base != *m_base) { m_base = base; m_reservations.clear(); }
    const auto key = std::pair{account, nonce};
    if (const auto found = m_reservations.find(key); found != m_reservations.end()) return found->second == operation ? ValidationCheck::VALID : ValidationCheck::CONFLICT;
    if (m_reservations.size() >= MAX_RESERVATIONS) return ValidationCheck::UNAVAILABLE;
    m_reservations.emplace(key, operation);
    try { if (Save()) return ValidationCheck::VALID; } catch (...) {}
    m_available = false; return ValidationCheck::UNAVAILABLE;
}
ValidationService::ValidationService(ValidationState& state, IdentityHybridPublicKey key, Signer signer, ValidationSnapshotReader reader)
    : m_state{state}, m_key{std::move(key)}, m_signer{std::move(signer)}, m_reader{std::move(reader)} {
    if (!ValidationNodeId(m_key) || !m_signer || !m_reader) throw std::invalid_argument{"invalid validation service"};
}
ValidationResult ValidationService::CheckReserveAndAttest(const ProtocolOperation& operation) {
    ValidationResult result;
    try {
        const bool available = m_reader([&](const FinalizedValidationSnapshot& snapshot) {
            const auto bound = snapshot.state.bound_nodes.find(*ValidationNodeId(m_key));
            if (bound == snapshot.state.bound_nodes.end() || bound->second.key != m_key) return;
            result.check = ValidateAgainstFinalizedBase(operation, snapshot);
            if (result.check != ValidationCheck::VALID) return;
            const auto nonce = *Nonce(operation); const auto id = *ComputeOperationId(operation);
            result.check = m_state.Reserve(snapshot.base, nonce.first, nonce.second, id);
            if (result.check != ValidationCheck::VALID) return;
            ValidationAttestation value{snapshot.base, id, *ValidationNodeId(m_key), {}};
            const auto digest = ValidationAttestationDigest(value);
            const auto signature = digest ? m_signer(*digest) : std::nullopt;
            if (!signature) { result.check = ValidationCheck::UNAVAILABLE; return; }
            value.signature = *signature;
            if (!VerifyValidationAttestation(value, operation, snapshot, m_key)) { result.check = ValidationCheck::UNAVAILABLE; return; }
            result.attestation = std::move(value);
        });
        if (!available) return {};
    } catch (...) { return {}; }
    return result;
}
bool HasLocalValidation(const ProtocolOperation& operation, const FinalizedValidationSnapshot& snapshot,
    std::span<const ValidationAttestation> values, const ValidationTrustPolicy& policy) {
    if (policy.required_accounts == 0 || policy.required_accounts > policy.nodes.size() || values.size() > MAX_RESERVATIONS || policy.nodes.size() > MAX_RESERVATIONS) return false;
    std::set<AccountId> accounts;
    for (const auto& value : values) {
        const auto trusted = policy.nodes.find(value.node_id);
        if (trusted == policy.nodes.end() || trusted->second.account.IsNull() || accounts.contains(trusted->second.account)) continue;
        const auto bound = snapshot.state.bound_nodes.find(value.node_id);
        if (bound == snapshot.state.bound_nodes.end() || bound->second.account != trusted->second.account) continue;
        if (VerifyValidationAttestation(value, operation, snapshot, trusted->second.key)) accounts.insert(trusted->second.account);
        if (accounts.size() >= policy.required_accounts) return true;
    }
    return false;
}
} // namespace cybou
