// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_VALIDATION_ATTESTATION_H
#define CYBOU_VALIDATION_ATTESTATION_H

/// \file
/// \brief Canonical Validation-attestation: сериализация, проверка и локальное подписание.

#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief Минимальный finalized AUTH для права подписывать Validation-attestation.
inline constexpr uint64_t VALIDATION_AUTHORITY_THRESHOLD{1'000'000};
/// \brief Точный canonical byte size: NetworkBinding || OperationID || base BlockID || AccountID || подписи.
inline constexpr size_t VALIDATION_ATTESTATION_SIZE{ 32 + 32 + 32 + 32 + 64 + 2420};

/**
 * \brief Свидетельство того, что узел этой Identity независимо исполнил OperationID
 *        на finalized state после base BlockID и признал операцию валидной.
 *
 * Хранится рядом с операцией, не меняет OperationID и не влияет на state.
 */
struct ValidationAttestation {
    cybou::Hash256 network_binding;
    cybou::Hash256 operation_id;
    cybou::Hash256 finalized_base_block_id;
    AccountId validator_account_id;
    IdentityHybridSignature signature;

    friend bool operator==(const ValidationAttestation&, const ValidationAttestation&) = default;
};

/// \brief Причина отказа при canonical разборе или криптографической проверке attestation.
enum class ValidationAttestationError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    WRONG_NETWORK,
    STALE_BASE,
    NOT_ELIGIBLE,
    INVALID_SIGNATURE,
};

/// \brief Вычисляет canonical digest attestation для подписи и проверки.
std::optional<std::array<unsigned char, 32>> ComputeValidationAttestationDigest(
    const ValidationAttestation& attestation);
/// \brief Полностью сериализует attestation в canonical wire bytes.
std::optional<std::vector<unsigned char>> SerializeValidationAttestation(const ValidationAttestation& attestation);
/// \brief Разбирает canonical wire bytes attestation.
std::optional<ValidationAttestation> DeserializeValidationAttestation(std::span<const unsigned char> bytes);

/// \brief Проверяет право AccountID подписывать Validation по локальному finalized state.
bool IsValidationEligible(const CybouState& finalized_state, const AccountId& account);

/**
 * \brief Проверяет сеть, finalized base, finalized AUTH валидатора и его текущую Authorization-подпись.
 *
 * Саму операцию функция не проверяет: вызывающая сторона обязана сначала
 * независимо исполнить её на своём finalized state.
 */
ValidationAttestationError VerifyValidationAttestation(const ValidationAttestation& attestation,
    const cybou::Hash256& network_binding, const cybou::Hash256& finalized_tip, const CybouState& finalized_state);

/// \brief Минимальная граница подписания: приватный материал Identity не покидает реализацию.
class ValidationSigner {
public:
    virtual ~ValidationSigner() = default;
    /// \brief Возвращает локальный AccountID валидатора, если signer активен.
    virtual std::optional<AccountId> Account() const = 0;
    /// \brief Подписывает digest текущим Authorization-ключом.
    virtual std::optional<IdentityHybridSignature> SignAuthorization(std::span<const unsigned char> digest) const = 0;
};

using ValidationSignerRef = std::shared_ptr<ValidationSigner>;

/// \brief Подписывает attestation только если локальная Identity имеет право в данном finalized state.
std::optional<ValidationAttestation> SignValidationAttestation(const ValidationSigner& signer,
    const cybou::Hash256& network_binding, const cybou::Hash256& operation_id, const cybou::Hash256& finalized_tip,
    const CybouState& finalized_state);

} // namespace cybou
#endif // CYBOU_VALIDATION_ATTESTATION_H
