// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Канонические правила имён .cybou, commit/reveal payload и NameRegistry.

#ifndef CYBOU_NAME_REGISTRY_H
#define CYBOU_NAME_REGISTRY_H

#include <cybou/economics.h>
#include <cybou/crypto/sha256.h>
#include <cybou/account_id.h>
#include <cybou/identity_registry.h>
#include <cybou/protocol_params.h>
#include <cybou/hash256.h>

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

/// \brief Границы размеров канонической label и related payloads name subsystem'а.
inline constexpr size_t NAME_MIN_LABEL_LENGTH{5};
inline constexpr size_t NAME_MAX_LABEL_LENGTH{32};
inline constexpr size_t NAME_COMMIT_PAYLOAD_SIZE{32};
inline constexpr size_t NAME_CLAIM_WORK_SIZE{112};
inline constexpr size_t NAME_REVEAL_PAYLOAD_SIZE{1 + 32 + 32 + NAME_CLAIM_WORK_SIZE}; // 177 bytes
inline constexpr size_t AUTHORIZED_NAME_COMMIT_SIZE{2565 + NAME_COMMIT_PAYLOAD_SIZE};    // 2597 bytes
inline constexpr size_t AUTHORIZED_NAME_REVEAL_SIZE{2565 + NAME_REVEAL_PAYLOAD_SIZE};    // 2742 bytes

/// \brief Ошибки синтаксической проверки публичной label .cybou.
enum class NameValidationError : uint8_t {
    NONE,                ///< Label допустима для `.cybou`.
    EMPTY,               ///< Пустая label.
    TOO_SHORT,           ///< Длина меньше `NAME_MIN_LABEL_LENGTH`.
    TOO_LONG,            ///< Длина больше `NAME_MAX_LABEL_LENGTH`.
    INVALID_CHARACTER,   ///< Есть символ вне `[a-z0-9-]`.
    INVALID_START_END,   ///< Label начинается или заканчивается `-`.
    CONSECUTIVE_HYPHENS, ///< Запрещена последовательность `--`.
    IDN_PREFIX,          ///< Запрещён зарезервированный префикс `xn--`.
    ALL_DIGITS,          ///< Нужна хотя бы одна латинская буква.
    PROTECTED_NAME,      ///< Label зарезервирована протоколом/genesis.
};

/// \brief Проверяет публичную label .cybou по каноническим правилам.
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
        CENTRAL_AUTHORITY_NAME, "admin", "root", "system", "support",
        "security", "operator", "validator", "wallet", "mail"
    };
    for (const auto& reserved : RESERVED) {
        if (label == reserved) {
            return NameValidationError::PROTECTED_NAME;
        }
    }

    return NameValidationError::NONE;
}

/// \brief Вычисляет commitment NameCommit из сети, аккаунта, label и salt.
inline cybou::Hash256 ComputeNameCommitment(
    const cybou::Hash256& network_binding,
    const AccountId& account_id,
    std::string_view label,
    std::span<const unsigned char, 32> salt)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NAME-COMMIT"};
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_binding.begin(), 32);
    hasher.Write(account_id.Value().begin(), 32);
    const uint8_t len = static_cast<uint8_t>(label.size());
    hasher.Write(&len, 1);
    hasher.Write(reinterpret_cast<const unsigned char*>(label.data()), label.size());
    hasher.Write(salt.data(), 32);
    cybou::Hash256 commitment;
    hasher.Finalize(commitment.begin());
    return commitment;
}

/// \brief Payload операции NameCommit.
struct NameCommitPayload {
    cybou::Hash256 commitment;

    friend bool operator==(const NameCommitPayload&, const NameCommitPayload&) = default;
};

/// \brief Сериализует payload NameCommit в канонический бинарный формат.
inline std::optional<std::array<unsigned char, NAME_COMMIT_PAYLOAD_SIZE>> SerializeNameCommitPayload(
    const NameCommitPayload& payload)
{
    if (payload.commitment.IsNull()) return std::nullopt;
    std::array<unsigned char, NAME_COMMIT_PAYLOAD_SIZE> out{};
    std::copy_n(payload.commitment.begin(), 32, out.begin() + 0);
    return out;
}

/// \brief Десериализует payload NameCommit.
inline std::optional<NameCommitPayload> DeserializeNameCommitPayload(std::span<const unsigned char> bytes)
{
    if (bytes.size() != NAME_COMMIT_PAYLOAD_SIZE) return std::nullopt;
    NameCommitPayload payload;
    std::copy_n(bytes.begin() + 0, 32, payload.commitment.begin());
    if (payload.commitment.IsNull()) return std::nullopt;
    return payload;
}

/// \brief Вычисляет payload commitment NameCommit.
inline std::optional<IdentityKeyId> ComputeNameCommitPayloadCommitment(const NameCommitPayload& payload)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NAME-COMMIT-PAYLOAD"};
    const auto bytes = SerializeNameCommitPayload(payload);
    if (!bytes) return std::nullopt;
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes->data(), bytes->size());
    IdentityKeyId res{};
    hasher.Finalize(res.data());
    return res;
}

/// \brief Identity-authorized NameCommit для включения в блок.
struct AuthorizedNameCommit {
    IdentityOperationAuthorization authorization;
    NameCommitPayload commit;

    friend bool operator==(const AuthorizedNameCommit&, const AuthorizedNameCommit&) = default;
};

/// \brief Proof-of-work структура для NameReveal.
struct NameClaimWork {
    cybou::Hash256 network_binding;
    AccountId account_id;
    cybou::Hash256 commitment;
    uint64_t work_epoch{0};
    uint64_t nonce{0};

    friend bool operator==(const NameClaimWork&, const NameClaimWork&) = default;
};

/// \brief Сериализует NameClaimWork в канонический бинарный формат.
inline std::optional<std::array<unsigned char, NAME_CLAIM_WORK_SIZE>> SerializeNameClaimWork(
    const NameClaimWork& work)
{
    if (work.network_binding.IsNull() || work.account_id.IsNull() || work.commitment.IsNull()) {
        return std::nullopt;
    }
    std::array<unsigned char, NAME_CLAIM_WORK_SIZE> out{};
    std::copy_n(work.network_binding.begin(), 32, out.begin() + 0);
    std::copy_n(work.account_id.Value().begin(), 32, out.begin() + 32);
    std::copy_n(work.commitment.begin(), 32, out.begin() + 64);
    for (int i = 0; i < 8; ++i) out[96 + i] = static_cast<unsigned char>(work.work_epoch >> (8 * i));
    for (int i = 0; i < 8; ++i) out[104 + i] = static_cast<unsigned char>(work.nonce >> (8 * i));
    return out;
}

/// \brief Десериализует NameClaimWork.
inline std::optional<NameClaimWork> DeserializeNameClaimWork(std::span<const unsigned char> bytes)
{
    if (bytes.size() != NAME_CLAIM_WORK_SIZE) return std::nullopt;
    NameClaimWork work;
    std::copy_n(bytes.begin() + 0, 32, work.network_binding.begin());
    const auto acc = AccountId::FromBytes(bytes.subspan(32, 32));
    if (!acc) return std::nullopt;
    work.account_id = *acc;
    std::copy_n(bytes.begin() + 64, 32, work.commitment.begin());
    uint64_t epoch{0};
    for (int i = 0; i < 8; ++i) epoch |= uint64_t{bytes[96 + i]} << (8 * i);
    work.work_epoch = epoch;
    uint64_t nonce{0};
    for (int i = 0; i < 8; ++i) nonce |= uint64_t{bytes[104 + i]} << (8 * i);
    work.nonce = nonce;
    if (work.network_binding.IsNull() || work.commitment.IsNull()) return std::nullopt;
    return work;
}

/// \brief Вычисляет domain-separated hash структуры NameClaimWork.
inline cybou::Hash256 ComputeNameClaimWorkHash(const NameClaimWork& work)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NAME-WORK"};
    const auto bytes = SerializeNameClaimWork(work);
    if (!bytes) return cybou::Hash256{};
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes->data(), bytes->size());
    cybou::Hash256 hash;
    hasher.Finalize(hash.begin());
    return hash;
}

/// \brief Проверяет PoW для NameClaimWork против заданной сложности.
inline bool CheckNameClaimWork(const NameClaimWork& work, uint32_t required_bits)
{
    if (required_bits == 0) return true;
    if (required_bits > 256) return false;
    const cybou::Hash256 hash = ComputeNameClaimWorkHash(work);
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

/// \brief Payload операции NameReveal.
struct NameRevealPayload {
    std::string label;
    std::array<unsigned char, 32> salt{};
    NameClaimWork work;

    friend bool operator==(const NameRevealPayload&, const NameRevealPayload&) = default;
};

/// \brief Сериализует payload NameReveal в канонический бинарный формат.
inline std::optional<std::array<unsigned char, NAME_REVEAL_PAYLOAD_SIZE>> SerializeNameRevealPayload(
    const NameRevealPayload& payload)
{
    if (payload.label.size() < NAME_MIN_LABEL_LENGTH || payload.label.size() > NAME_MAX_LABEL_LENGTH) {
        return std::nullopt;
    }
    const auto work_bytes = SerializeNameClaimWork(payload.work);
    if (!work_bytes) return std::nullopt;
    if (std::all_of(payload.salt.begin(), payload.salt.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;

    std::array<unsigned char, NAME_REVEAL_PAYLOAD_SIZE> out{};
    out[0] = static_cast<unsigned char>(payload.label.size());
    std::copy(payload.label.begin(), payload.label.end(), out.begin() + 1);
    std::copy(payload.salt.begin(), payload.salt.end(), out.begin() + 33);
    std::copy(work_bytes->begin(), work_bytes->end(), out.begin() + 65);
    return out;
}

/// \brief Десериализует payload NameReveal.
inline std::optional<NameRevealPayload> DeserializeNameRevealPayload(std::span<const unsigned char> bytes)
{
    if (bytes.size() != NAME_REVEAL_PAYLOAD_SIZE) return std::nullopt;
    const uint8_t label_len = bytes[0];
    if (label_len < NAME_MIN_LABEL_LENGTH || label_len > NAME_MAX_LABEL_LENGTH) return std::nullopt;

    NameRevealPayload payload;
    payload.label.assign(reinterpret_cast<const char*>(bytes.data() + 1), label_len);
    for (size_t i = 1 + label_len; i < 33; ++i) {
        if (bytes[i] != 0) return std::nullopt;
    }
    std::copy_n(bytes.begin() + 33, 32, payload.salt.begin());
    if (std::all_of(payload.salt.begin(), payload.salt.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;

    const auto work = DeserializeNameClaimWork(bytes.subspan(65, NAME_CLAIM_WORK_SIZE));
    if (!work) return std::nullopt;
    payload.work = *work;
    return payload;
}

/// \brief Вычисляет payload commitment NameReveal.
inline std::optional<IdentityKeyId> ComputeNameRevealPayloadCommitment(const NameRevealPayload& payload)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NAME-REVEAL-PAYLOAD"};
    const auto bytes = SerializeNameRevealPayload(payload);
    if (!bytes) return std::nullopt;
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes->data(), bytes->size());
    IdentityKeyId res{};
    hasher.Finalize(res.data());
    return res;
}

/// \brief Identity-authorized NameReveal для включения в блок.
struct AuthorizedNameReveal {
    IdentityOperationAuthorization authorization;
    NameRevealPayload reveal;

    friend bool operator==(const AuthorizedNameReveal&, const AuthorizedNameReveal&) = default;
};

/// \brief Запись о pending NameCommit, ожидающем достаточной глубины.
struct NameCommitRecord {
    AccountId account_id;
    uint64_t commit_height{0};

    friend bool operator==(const NameCommitRecord&, const NameCommitRecord&) = default;
};

/// \brief Каноническое отображение имён, обратных ссылок и pending commit'ов.
struct NameRegistry {
    std::map<std::string, AccountId> names;
    std::map<AccountId, std::string> account_names;
    std::map<cybou::Hash256, NameCommitRecord> pending_commits;

    friend bool operator==(const NameRegistry&, const NameRegistry&) = default;

    /// \brief Разрешает публичную label в AccountId.
    const AccountId* Resolve(std::string_view label) const
    {
        auto it = names.find(std::string(label));
        return it != names.end() ? &it->second : nullptr;
    }

    /// \brief Возвращает primary name аккаунта, если оно уже финализировано.
    const std::string* PrimaryName(const AccountId& account_id) const
    {
        auto it = account_names.find(account_id);
        return it != account_names.end() ? &it->second : nullptr;
    }
};

/// \brief Сериализует канонический реестр имён.
inline std::vector<unsigned char> SerializeNameRegistry(const NameRegistry& reg)
{
    std::vector<unsigned char> out;
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

/// \brief Десериализует и валидирует канонический реестр имён.
inline std::optional<NameRegistry> DeserializeNameRegistry(std::span<const unsigned char> bytes)
{
    if (bytes.size() < 4 + 4) return std::nullopt;

    NameRegistry reg;
    size_t offset{0};

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
        // Reserved labels pass syntax here; ValidateCybouState admits them
        // only with a matching claimed genesis allocation.
        if (const auto validity = ValidateNameLabel(label);
            validity != NameValidationError::NONE && validity != NameValidationError::PROTECTED_NAME) return std::nullopt;

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
        cybou::Hash256 commit;
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

/// \brief Ошибки применения NameCommit к кандидатному состоянию.
enum class NameCommitError : uint8_t {
    NONE,                      ///< Commit принят.
    INVALID_PAYLOAD,           ///< Payload неканоничен.
    INVALID_AUTHORIZATION,     ///< Identity authorization невалидна.
    ACCOUNT_NOT_FOUND,         ///< Авторизующий аккаунт отсутствует.
    ACCOUNT_ALREADY_HAS_NAME,  ///< У аккаунта уже есть финализированное имя.
    ACCOUNT_HAS_PENDING_COMMIT, ///< У аккаунта уже есть другой pending commit.
    COMMITMENT_EXISTS,         ///< Такой commit уже зарегистрирован.
    COMMITMENT_LIMIT_EXCEEDED, ///< Превышен лимит pending commit-ов.
};

/// \brief Ошибки применения NameReveal к кандидатному состоянию.
enum class NameRevealError : uint8_t {
    NONE,                      ///< Reveal принят и имя закреплено.
    INVALID_PAYLOAD,           ///< Payload неканоничен.
    INVALID_AUTHORIZATION,     ///< Identity authorization невалидна.
    INVALID_LABEL_SYNTAX,      ///< Label нарушает правила `.cybou`.
    ACCOUNT_NOT_FOUND,         ///< Авторизующий аккаунт отсутствует.
    ACCOUNT_ALREADY_HAS_NAME,  ///< У аккаунта уже есть имя.
    NAME_ALREADY_TAKEN,        ///< Имя уже занято либо зарезервировано genesis.
    COMMITMENT_NOT_FOUND,      ///< Соответствующий pending commit отсутствует.
    COMMITMENT_ACCOUNT_MISMATCH, ///< Commit принадлежит другому аккаунту.
    INSUFFICIENT_COMMIT_DEPTH, ///< Не выдержана минимальная глубина между commit и reveal.
    COMMIT_EXPIRED,            ///< Pending commit истёк.
    INVALID_WORK_TARGET,       ///< Зарезервировано для ошибок цели сложности reveal PoW.
    INVALID_WORK_PROOF,        ///< Reveal PoW или его binding некорректны.
};

/// \brief Псевдоним публичного типа для NameCommit operation payload.
using NameCommitOp = AuthorizedNameCommit;
/// \brief Псевдоним публичного типа для NameReveal operation payload.
using NameRevealOp = AuthorizedNameReveal;

} // namespace cybou

#endif // CYBOU_NAME_REGISTRY_H
