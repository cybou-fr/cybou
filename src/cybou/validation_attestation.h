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

/// \brief Минимальный finalized AUTH для права подписывать Validation-attestation (текущее кодовое значение: 1M AUTH).
/// \details Строгое условие — authority > VALIDATION_AUTHORITY_THRESHOLD; точное значение берётся из этой константы.
inline constexpr uint64_t VALIDATION_AUTHORITY_THRESHOLD{1'000'000};
/// \brief Точный canonical byte size: NetworkBinding || OperationID || base BlockID || AccountID || подписи.
/// \details 32 + 32 + 32 + 32 + 64 + 2420 байт = тело attestation плюс Ed25519 и ML-DSA-44 signature.
inline constexpr size_t VALIDATION_ATTESTATION_SIZE{ 32 + 32 + 32 + 32 + 64 + 2420};

/**
 * \brief Свидетельство того, что узел этой Identity независимо исполнил OperationID
 *        на finalized state после base BlockID и признал операцию валидной.
 *
 * Хранится рядом с операцией, не меняет OperationID и не влияет на state.
 */
struct ValidationAttestation {
    /// \brief NetworkBinding сети, для которой подписана attestation.
    cybou::Hash256 network_binding;
    /// \brief OperationID локально исполненной кандидат-операции.
    cybou::Hash256 operation_id;
    /// \brief Finalized BlockID, относительно которого кандидат независимо исполнялся.
    cybou::Hash256 finalized_base_block_id;
    /// \brief AccountID Identity, чьим Authorization key подписана attestation.
    AccountId validator_account_id;
    /// \brief Гибридная подпись Authorization key по canonical digest attestation.
    IdentityHybridSignature signature;

    friend bool operator==(const ValidationAttestation&, const ValidationAttestation&) = default;
};

/// \brief Причина отказа при canonical разборе или криптографической проверке attestation.
enum class ValidationAttestationError : uint8_t {
    /// \brief Проверка пройдена.
    NONE,
    /// \brief Нарушен canonical размер/структура payload или формат signature.
    INVALID_PAYLOAD,
    /// \brief Attestation подписана для другой official network.
    WRONG_NETWORK,
    /// \brief Attestation опирается на иной finalized base, чем текущий локальный tip.
    STALE_BASE,
    /// \brief Указанный AccountID не имеет достаточного finalized AUTH.
    NOT_ELIGIBLE,
    /// \brief Authorization signature не сошлась с текущим finalized key record.
    INVALID_SIGNATURE,
};

/// \brief Вычисляет canonical digest attestation для подписи и проверки.
/// \param attestation Attestation без изменения её полей.
/// \return SHA-256 digest или std::nullopt, если attestation содержит нулевые обязательные поля.
std::optional<std::array<unsigned char, 32>> ComputeValidationAttestationDigest(
    const ValidationAttestation& attestation);
/// \brief Полностью сериализует attestation в canonical wire bytes.
/// \param attestation Полностью заполненная attestation.
/// \return Canonical bytes или std::nullopt, если обязательные поля/размер ML-DSA подписи недопустимы.
std::optional<std::vector<unsigned char>> SerializeValidationAttestation(const ValidationAttestation& attestation);
/// \brief Разбирает canonical wire bytes attestation.
/// \param bytes Exact wire bytes.
/// \return Разобранная attestation или std::nullopt при неверной длине/структуре.
std::optional<ValidationAttestation> DeserializeValidationAttestation(std::span<const unsigned char> bytes);

/// \brief Проверяет право AccountID подписывать Validation по локальному finalized state.
/// \param finalized_state Локально verified finalized state.
/// \param account Проверяемый AccountID.
/// \return true только если account существует и его authority строго больше VALIDATION_AUTHORITY_THRESHOLD.
bool IsValidationEligible(const CybouState& finalized_state, const AccountId& account);

/**
 * \brief Проверяет сеть, finalized base, finalized AUTH валидатора и его текущую Authorization-подпись.
 *
 * Саму операцию функция не проверяет: вызывающая сторона обязана сначала
 * независимо исполнить её на своём finalized state.
 */
/// \param attestation Проверяемая Validation-attestation.
/// \param network_binding Ожидаемый NetworkBinding локальной official network.
/// \param finalized_tip Текущий локальный finalized BlockID.
/// \param finalized_state Текущий локальный finalized state.
/// \return Причина отказа либо NONE при полном успехе.
/// \pre Candidate operation уже независимо исполнена вызывающей стороной; эта функция не заменяет исполнение.
ValidationAttestationError VerifyValidationAttestation(const ValidationAttestation& attestation,
    const cybou::Hash256& network_binding, const cybou::Hash256& finalized_tip, const CybouState& finalized_state);

/// \brief Минимальная граница подписания: приватный материал Identity не покидает реализацию.
class ValidationSigner {
public:
    virtual ~ValidationSigner() = default;
    /// \brief Возвращает локальный AccountID валидатора, если signer активен.
    /// \return AccountID активной Identity или std::nullopt, если локальная Identity недоступна для Validation.
    virtual std::optional<AccountId> Account() const = 0;
    /// \brief Подписывает digest текущим Authorization-ключом.
    /// \param digest Canonical digest длиной 32 байта.
    /// \return Hybrid signature или std::nullopt, если подпись сейчас невозможна.
    virtual std::optional<IdentityHybridSignature> SignAuthorization(std::span<const unsigned char> digest) const = 0;
};

using ValidationSignerRef = std::shared_ptr<ValidationSigner>;

/// \brief Подписывает attestation только если локальная Identity имеет право в данном finalized state.
/// \param signer Абстракция над локальным Authorization signer.
/// \param network_binding NetworkBinding текущей сети.
/// \param operation_id OperationID уже локально принятой кандидат-операции.
/// \param finalized_tip Finalized BlockID, относительно которого операция исполнялась.
/// \param finalized_state Локальный finalized state для проверки eligibility.
/// \return Полностью проверенная attestation или std::nullopt, если нет права подписи либо подпись/самопроверка не удались.
/// \pre Вызывающая сторона уже независимо исполнила операцию на finalized_tip.
std::optional<ValidationAttestation> SignValidationAttestation(const ValidationSigner& signer,
    const cybou::Hash256& network_binding, const cybou::Hash256& operation_id, const cybou::Hash256& finalized_tip,
    const CybouState& finalized_state);

} // namespace cybou
#endif // CYBOU_VALIDATION_ATTESTATION_H
