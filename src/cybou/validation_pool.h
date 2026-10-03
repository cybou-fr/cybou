// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_VALIDATION_POOL_H
#define CYBOU_VALIDATION_POOL_H

/// \file
/// \brief RAM-sidecar для проверенных Validation-attestations на текущем finalized base.

#include <cybou/validation_attestation.h>

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace cybou {

/// \brief Верхняя граница числа candidate operations, для которых одновременно держатся Validation-attestations.
inline constexpr size_t MAX_VALIDATED_OPERATIONS{256};
/// \brief Верхняя граница числа unique validator AccountID на одну candidate operation.
inline constexpr size_t MAX_ATTESTATIONS_PER_OPERATION{16};

/// \brief Результат добавления attestation в volatile ValidationPool.
enum class ValidationPoolAdd : uint8_t { ADDED, DUPLICATE, STALE_BASE, FULL };
/// \var ValidationPoolAdd::ADDED
/// \brief Attestation сохранена в sidecar.
/// \var ValidationPoolAdd::DUPLICATE
/// \brief Эта пара (OperationID, validator AccountID) уже известна.
/// \var ValidationPoolAdd::STALE_BASE
/// \brief Attestation относится к другому finalized base и потому устарела для текущего sidecar.
/// \var ValidationPoolAdd::FULL
/// \brief Достигнут лимит числа операций или attestations на операцию.

/**
 * Volatile RAM sidecar of verified attestations, keyed by OperationID with one
 * attestation per validator AccountID. Every entry shares the current
 * finalized base; advancing the base discards them all. Not state, not a DB.
 */
class ValidationPool
{
public:
    using Key = std::pair<cybou::Hash256, AccountId>;

    /// \brief Добавляет уже проверенную attestation для локально исполненного кандидата.
    /// \param attestation Already verified Validation-attestation.
    /// \return ADDED при успехе; DUPLICATE для того же validator AccountID; STALE_BASE при иной finalized base; FULL при исчерпании лимитов.
    /// \pre Вызывающая сторона уже выполнила VerifyValidationAttestation() и убедилась, что операция удерживается локально.
    /// \post При ADDED пул содержит не более одной attestation на пару (OperationID, AccountID).
    ValidationPoolAdd Add(const ValidationAttestation& attestation);
    /// \brief Смена finalized tip делает все удерживаемые attestations устаревшими.
    /// \param finalized_tip Новый локальный finalized BlockID.
    /// \post Если finalized_tip отличается от текущего base, пул полностью очищен.
    void ResetBase(const cybou::Hash256& finalized_tip);
    /// \brief Удаляет все attestations для одной candidate operation.
    /// \param operation_id OperationID, который больше не нужно сопровождать Validation sidecar'ом.
    void Drop(const cybou::Hash256& operation_id);
    /// \brief Возвращает число удерживаемых attestations для одной operation.
    /// \param operation_id Искомый OperationID.
    /// \return Количество известных validator signatures; 0, если записи нет.
    size_t Count(const cybou::Hash256& operation_id) const;
    /// \brief Возвращает attestations для одной операции в детерминированном порядке по validator AccountID.
    /// \param operation_id OperationID искомой candidate operation.
    /// \return Копия attestation-ов; пустой вектор, если операция не отслеживается.
    std::vector<ValidationAttestation> ForOperation(const cybou::Hash256& operation_id) const;
    /// \brief Возвращает первую attestation, которую предикат skip ещё не считает известной.
    /// \param skip Предикат фильтрации по паре (OperationID, validator AccountID).
    /// \return Первая подходящая attestation в детерминированном порядке или std::nullopt.
    std::optional<ValidationAttestation> First(const std::function<bool(const Key&)>& skip) const;
    /// \brief Число candidate operations, для которых sidecar сейчас удерживает attestations.
    size_t Operations() const { return m_entries.size(); }

private:
    cybou::Hash256 m_base;
    std::map<cybou::Hash256, std::map<AccountId, ValidationAttestation>> m_entries;
};

} // namespace cybou

#endif // CYBOU_VALIDATION_POOL_H
