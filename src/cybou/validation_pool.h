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

inline constexpr size_t MAX_VALIDATED_OPERATIONS{256};
inline constexpr size_t MAX_ATTESTATIONS_PER_OPERATION{16};

/// \brief Результат добавления attestation в volatile ValidationPool.
enum class ValidationPoolAdd : uint8_t { ADDED, DUPLICATE, STALE_BASE, FULL };

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
    ValidationPoolAdd Add(const ValidationAttestation& attestation);
    /// \brief Смена finalized tip делает все удерживаемые attestations устаревшими.
    void ResetBase(const cybou::Hash256& finalized_tip);
    void Drop(const cybou::Hash256& operation_id);
    size_t Count(const cybou::Hash256& operation_id) const;
    /// \brief Возвращает attestations для одной операции в детерминированном порядке по validator AccountID.
    std::vector<ValidationAttestation> ForOperation(const cybou::Hash256& operation_id) const;
    /// \brief Возвращает первую attestation, которую предикат skip ещё не считает известной.
    std::optional<ValidationAttestation> First(const std::function<bool(const Key&)>& skip) const;
    size_t Operations() const { return m_entries.size(); }

private:
    cybou::Hash256 m_base;
    std::map<cybou::Hash256, std::map<AccountId, ValidationAttestation>> m_entries;
};

} // namespace cybou

#endif // CYBOU_VALIDATION_POOL_H
