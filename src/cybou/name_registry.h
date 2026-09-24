// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_NAME_REGISTRY_H
#define CYBOU_NAME_REGISTRY_H

#include <crypto/sha256.h>
#include <cybou/account_id.h>
#include <cybou/identity_registry_v2.h>
#include <cybou/protocol_params.h>
#include <uint256.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cybou {

inline constexpr uint8_t NAME_REGISTRY_VERSION{2};
inline constexpr size_t NAME_MIN_LABEL_LENGTH{5};
inline constexpr size_t NAME_MAX_LABEL_LENGTH{32};
inline constexpr size_t NAME_COMMIT_PAYLOAD_SIZE{33};
inline constexpr size_t NAME_CLAIM_WORK_SIZE{113};
inline constexpr size_t NAME_REVEAL_PAYLOAD_SIZE{1 + 1 + 32 + 32 + NAME_CLAIM_WORK_SIZE}; // 179 bytes
inline constexpr size_t AUTHORIZED_NAME_COMMIT_SIZE{2597 + NAME_COMMIT_PAYLOAD_SIZE};    // 2630 bytes
inline constexpr size_t AUTHORIZED_NAME_REVEAL_SIZE{2597 + NAME_REVEAL_PAYLOAD_SIZE};    // 2776 bytes

enum class NameValidationError : uint8_t {
    NONE,
    EMPTY,
    TOO_SHORT,
    TOO_LONG,
    INVALID_CHARACTER,
    INVALID_START_END,
    CONSECUTIVE_HYPHENS,
    IDN_PREFIX,
    ALL_DIGITS,
    RESERVED_NAME,
};

inline NameValidationError ValidateNameLabel(std::string_view label)
{
    if (label.empty()) return NameValidationError::EMPTY;
    if (label.size() < NAME_MIN_LABEL_LENGTH) return NameValidationError::TOO_SHORT;
    if (label.size() > NAME_MAX_LABEL_LENGTH) return NameValidationError::TOO_LONG;

    if (label.starts_with("xn--")) {
        return NameValidationError::IDN_PREFIX;
    }

    bool has_alpha = false;
    for (size_t i = 0; i < label.size(); ++i) {
        const char c = label[i];
        if (c >= 'a' && c <= 'z') {
            has_alpha = true;
        } else if (c >= '0' && c <= '9') {
            // digit allowed
        } else if (c == '-') {
            if (i + 1 < label.size() && label[i + 1] == '-') {
                return NameValidationError::CONSECUTIVE_HYPHENS;
            }
        } else {
            return NameValidationError::INVALID_CHARACTER;
        }
    }

    if (label.front() == '-' || label.back() == '-') {
        return NameValidationError::INVALID_START_END;
    }

    if (!has_alpha) {
        return NameValidationError::ALL_DIGITS;
    }

    static constexpr std::string_view RESERVED[] = {
        "cybou", "admin", "root", "system", "support",
        "security", "operator", "validator", "wallet", "mail"
    };
    for (const auto& reserved : RESERVED) {
        if (label == reserved) {
            return NameValidationError::RESERVED_NAME;
        }
    }

    return NameValidationError::NONE;
}

inline uint256 ComputeNameCommitment(
    const uint256& network_id,
    const AccountId& account_id,
    std::string_view label,
    std::span<const unsigned char, 32> salt)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NAME-COMMIT/V2"};
    static constexpr uint8_t VERSION{NAME_REGISTRY_VERSION};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(&VERSION, 1);
    hasher.Write(network_id.begin(), 32);
    hasher.Write(account_id.Value().begin(), 32);
    const uint8_t len = static_cast<uint8_t>(label.size());
    hasher.Write(&len, 1);
    hasher.Write(reinterpret_cast<const unsigned char*>(label.data()), label.size());
    hasher.Write(salt.data(), 32);
    uint256 commitment;
    hasher.Finalize(commitment.begin());
    return commitment;
}

struct NameCommitPayload {
    uint8_t version{NAME_REGISTRY_VERSION};
    uint256 commitment;

    friend bool operator==(const NameCommitPayload&, const NameCommitPayload&) = default;
};

inline std::optional<std::array<unsigned char, NAME_COMMIT_PAYLOAD_SIZE>> SerializeNameCommitPayload(
    const NameCommitPayload& payload)
{
    if (payload.version != NAME_REGISTRY_VERSION || payload.commitment.IsNull()) return std::nullopt;
    std::array<unsigned char, NAME_COMMIT_PAYLOAD_SIZE> out{};
    out[0] = payload.version;
    std::copy_n(payload.commitment.begin(), 32, out.begin() + 1);
    return out;
}

inline std::optional<NameCommitPayload> DeserializeNameCommitPayload(std::span<const unsigned char> bytes)
{
    if (bytes.size() != NAME_COMMIT_PAYLOAD_SIZE || bytes[0] != NAME_REGISTRY_VERSION) return std::nullopt;
    NameCommitPayload payload;
    payload.version = bytes[0];
    std::copy_n(bytes.begin() + 1, 32, payload.commitment.begin());
    if (payload.commitment.IsNull()) return std::nullopt;
    return payload;
}

inline std::optional<IdentityKeyIdV2> ComputeNameCommitPayloadCommitment(const NameCommitPayload& payload)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NAME-COMMIT-PAYLOAD/V2"};
    const auto bytes = SerializeNameCommitPayload(payload);
    if (!bytes) return std::nullopt;
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes->data(), bytes->size());
    IdentityKeyIdV2 res{};
    hasher.Finalize(res.data());
    return res;
}

struct AuthorizedNameCommit {
    DeviceAuthorizationV2 authorization;
    NameCommitPayload commit;

    friend bool operator==(const AuthorizedNameCommit&, const AuthorizedNameCommit&) = default;
};

struct NameClaimWork {
    uint8_t version{NAME_REGISTRY_VERSION};
    uint256 network_id;
    AccountId account_id;
    uint256 commitment;
    uint64_t work_epoch{0};
    uint64_t nonce{0};

    friend bool operator==(const NameClaimWork&, const NameClaimWork&) = default;
};

inline std::optional<std::array<unsigned char, NAME_CLAIM_WORK_SIZE>> SerializeNameClaimWork(
    const NameClaimWork& work)
{
    if (work.version != NAME_REGISTRY_VERSION || work.network_id.IsNull() || work.account_id.IsNull() || work.commitment.IsNull()) {
        return std::nullopt;
    }
    std::array<unsigned char, NAME_CLAIM_WORK_SIZE> out{};
    out[0] = work.version;
    std::copy_n(work.network_id.begin(), 32, out.begin() + 1);
    std::copy_n(work.account_id.Value().begin(), 32, out.begin() + 33);
    std::copy_n(work.commitment.begin(), 32, out.begin() + 65);
    for (int i = 0; i < 8; ++i) out[97 + i] = static_cast<unsigned char>(work.work_epoch >> (8 * i));
    for (int i = 0; i < 8; ++i) out[105 + i] = static_cast<unsigned char>(work.nonce >> (8 * i));
    return out;
}

inline std::optional<NameClaimWork> DeserializeNameClaimWork(std::span<const unsigned char> bytes)
{
    if (bytes.size() != NAME_CLAIM_WORK_SIZE || bytes[0] != NAME_REGISTRY_VERSION) return std::nullopt;
    NameClaimWork work;
    work.version = bytes[0];
    std::copy_n(bytes.begin() + 1, 32, work.network_id.begin());
    const auto acc = AccountId::FromBytes(bytes.subspan(33, 32));
    if (!acc) return std::nullopt;
    work.account_id = *acc;
    std::copy_n(bytes.begin() + 65, 32, work.commitment.begin());
    uint64_t epoch{0};
    for (int i = 0; i < 8; ++i) epoch |= uint64_t{bytes[97 + i]} << (8 * i);
    work.work_epoch = epoch;
    uint64_t nonce{0};
    for (int i = 0; i < 8; ++i) nonce |= uint64_t{bytes[105 + i]} << (8 * i);
    work.nonce = nonce;
    if (work.network_id.IsNull() || work.commitment.IsNull()) return std::nullopt;
    return work;
}

inline uint256 ComputeNameClaimWorkHash(const NameClaimWork& work)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NAME-WORK/V2"};
    const auto bytes = SerializeNameClaimWork(work);
    if (!bytes) return uint256{};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes->data(), bytes->size());
    uint256 hash;
    hasher.Finalize(hash.begin());
    return hash;
}

inline bool CheckNameClaimWork(const NameClaimWork& work, uint32_t required_bits)
{
    if (required_bits == 0) return true;
    if (required_bits > 256) return false;
    const uint256 hash = ComputeNameClaimWorkHash(work);
    unsigned count{0};
    for (size_t i = 0; i < 32; ++i) {
        const unsigned char b = hash.begin()[i];
        if (b == 0) {
            count += 8;
        } else {
            count += std::countl_zero(b);
            break;
        }
    }
    return count >= required_bits;
}

struct NameRevealPayload {
    uint8_t version{NAME_REGISTRY_VERSION};
    std::string label;
    std::array<unsigned char, 32> salt{};
    NameClaimWork work;

    friend bool operator==(const NameRevealPayload&, const NameRevealPayload&) = default;
};

inline std::optional<std::array<unsigned char, NAME_REVEAL_PAYLOAD_SIZE>> SerializeNameRevealPayload(
    const NameRevealPayload& payload)
{
    if (payload.version != NAME_REGISTRY_VERSION || payload.label.size() < NAME_MIN_LABEL_LENGTH || payload.label.size() > NAME_MAX_LABEL_LENGTH) {
        return std::nullopt;
    }
    const auto work_bytes = SerializeNameClaimWork(payload.work);
    if (!work_bytes) return std::nullopt;
    if (std::all_of(payload.salt.begin(), payload.salt.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;

    std::array<unsigned char, NAME_REVEAL_PAYLOAD_SIZE> out{};
    out[0] = payload.version;
    out[1] = static_cast<unsigned char>(payload.label.size());
    std::copy(payload.label.begin(), payload.label.end(), out.begin() + 2);
    std::copy(payload.salt.begin(), payload.salt.end(), out.begin() + 34);
    std::copy(work_bytes->begin(), work_bytes->end(), out.begin() + 66);
    return out;
}

inline std::optional<NameRevealPayload> DeserializeNameRevealPayload(std::span<const unsigned char> bytes)
{
    if (bytes.size() != NAME_REVEAL_PAYLOAD_SIZE || bytes[0] != NAME_REGISTRY_VERSION) return std::nullopt;
    const uint8_t label_len = bytes[1];
    if (label_len < NAME_MIN_LABEL_LENGTH || label_len > NAME_MAX_LABEL_LENGTH) return std::nullopt;

    NameRevealPayload payload;
    payload.version = bytes[0];
    payload.label.assign(reinterpret_cast<const char*>(bytes.data() + 2), label_len);
    for (size_t i = 2 + label_len; i < 34; ++i) {
        if (bytes[i] != 0) return std::nullopt;
    }
    std::copy_n(bytes.begin() + 34, 32, payload.salt.begin());
    if (std::all_of(payload.salt.begin(), payload.salt.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;

    const auto work = DeserializeNameClaimWork(bytes.subspan(66, NAME_CLAIM_WORK_SIZE));
    if (!work) return std::nullopt;
    payload.work = *work;
    return payload;
}

inline std::optional<IdentityKeyIdV2> ComputeNameRevealPayloadCommitment(const NameRevealPayload& payload)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NAME-REVEAL-PAYLOAD/V2"};
    const auto bytes = SerializeNameRevealPayload(payload);
    if (!bytes) return std::nullopt;
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes->data(), bytes->size());
    IdentityKeyIdV2 res{};
    hasher.Finalize(res.data());
    return res;
}

struct AuthorizedNameReveal {
    DeviceAuthorizationV2 authorization;
    NameRevealPayload reveal;

    friend bool operator==(const AuthorizedNameReveal&, const AuthorizedNameReveal&) = default;
};

struct NameCommitRecord {
    AccountId account_id;
    uint64_t commit_height{0};

    friend bool operator==(const NameCommitRecord&, const NameCommitRecord&) = default;
};

struct NameRegistry {
    uint8_t version{NAME_REGISTRY_VERSION};
    std::map<std::string, AccountId> names;
    std::map<AccountId, std::string> account_names;
    std::map<uint256, NameCommitRecord> pending_commits;

    friend bool operator==(const NameRegistry&, const NameRegistry&) = default;

    const AccountId* Resolve(std::string_view label) const
    {
        auto it = names.find(std::string(label));
        return it != names.end() ? &it->second : nullptr;
    }

    const std::string* PrimaryName(const AccountId& account_id) const
    {
        auto it = account_names.find(account_id);
        return it != account_names.end() ? &it->second : nullptr;
    }
};

inline std::vector<unsigned char> SerializeNameRegistry(const NameRegistry& reg)
{
    std::vector<unsigned char> out;
    out.push_back(reg.version);
    const uint32_t names_count = static_cast<uint32_t>(reg.names.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(names_count >> (8 * i)));
    for (const auto& [label, acc] : reg.names) {
        out.push_back(static_cast<unsigned char>(label.size()));
        out.insert(out.end(), label.begin(), label.end());
        out.insert(out.end(), acc.Value().begin(), acc.Value().end());
    }
    const uint32_t commit_count = static_cast<uint32_t>(reg.pending_commits.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(commit_count >> (8 * i)));
    for (const auto& [commit, record] : reg.pending_commits) {
        out.insert(out.end(), commit.begin(), commit.end());
        out.insert(out.end(), record.account_id.Value().begin(), record.account_id.Value().end());
        for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(record.commit_height >> (8 * i)));
    }
    return out;
}

inline std::optional<NameRegistry> DeserializeNameRegistry(std::span<const unsigned char> bytes)
{
    if (bytes.size() < 1 + 4 + 4) return std::nullopt;
    if (bytes[0] != NAME_REGISTRY_VERSION) return std::nullopt;

    NameRegistry reg;
    reg.version = bytes[0];
    size_t offset{1};

    uint32_t names_count{0};
    for (int i = 0; i < 4; ++i) names_count |= uint32_t{bytes[offset + i]} << (8 * i);
    offset += 4;

    for (uint32_t i = 0; i < names_count; ++i) {
        if (offset >= bytes.size()) return std::nullopt;
        const uint8_t label_len = bytes[offset++];
        if (label_len < NAME_MIN_LABEL_LENGTH || label_len > NAME_MAX_LABEL_LENGTH || offset + label_len + 32 > bytes.size()) {
            return std::nullopt;
        }
        std::string label(reinterpret_cast<const char*>(bytes.data() + offset), label_len);
        offset += label_len;
        if (ValidateNameLabel(label) != NameValidationError::NONE) return std::nullopt;

        const auto acc = AccountId::FromBytes(bytes.subspan(offset, 32));
        offset += 32;
        if (!acc) return std::nullopt;

        if (reg.names.contains(label) || reg.account_names.contains(*acc)) return std::nullopt;
        reg.names.emplace(label, *acc);
        reg.account_names.emplace(*acc, label);
    }

    if (offset + 4 > bytes.size()) return std::nullopt;
    uint32_t commit_count{0};
    for (int i = 0; i < 4; ++i) commit_count |= uint32_t{bytes[offset + i]} << (8 * i);
    offset += 4;

    if (bytes.size() != offset + static_cast<size_t>(commit_count) * (32 + 32 + 8)) return std::nullopt;

    for (uint32_t i = 0; i < commit_count; ++i) {
        uint256 commit;
        std::copy_n(bytes.begin() + offset, 32, commit.begin());
        offset += 32;
        const auto acc = AccountId::FromBytes(bytes.subspan(offset, 32));
        offset += 32;
        uint64_t height{0};
        for (int j = 0; j < 8; ++j) height |= uint64_t{bytes[offset + j]} << (8 * j);
        offset += 8;

        if (commit.IsNull() || !acc || reg.pending_commits.contains(commit)) return std::nullopt;
        reg.pending_commits.emplace(commit, NameCommitRecord{*acc, height});
    }

    return reg;
}

enum class NameCommitError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_AUTHORIZATION,
    ACCOUNT_NOT_FOUND,
    ACCOUNT_ALREADY_HAS_NAME,
    ACCOUNT_HAS_PENDING_COMMIT,
    COMMITMENT_EXISTS,
    COMMITMENT_LIMIT_EXCEEDED,
};

enum class NameRevealError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_AUTHORIZATION,
    INVALID_LABEL_SYNTAX,
    ACCOUNT_NOT_FOUND,
    ACCOUNT_ALREADY_HAS_NAME,
    NAME_ALREADY_TAKEN,
    COMMITMENT_NOT_FOUND,
    COMMITMENT_ACCOUNT_MISMATCH,
    INSUFFICIENT_COMMIT_DEPTH,
    COMMIT_EXPIRED,
    INVALID_WORK_TARGET,
    INVALID_WORK_PROOF,
};

// Aliases
using NameCommitOp = AuthorizedNameCommit;
using NameRevealOp = AuthorizedNameReveal;

} // namespace cybou

#endif // CYBOU_NAME_REGISTRY_H
