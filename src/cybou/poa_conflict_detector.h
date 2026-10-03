// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Локальное обнаружение equivocation PoA finalizer'а и fail-closed halt.

#ifndef CYBOU_POA_CONFLICT_DETECTOR_H
#define CYBOU_POA_CONFLICT_DETECTOR_H

#include <cybou/kv_store.h>
#include <cybou/poa_finality.h>

#include <mutex>
#include <optional>
#include <string>

namespace cybou {

/// \brief Результат наблюдения PoA-сертификата на одной высоте/родителе.
enum class PoaConflictStatus : uint8_t {
    OBSERVED,
    ALREADY_OBSERVED,
    SAFETY_CONFLICT,
    COMPETING_NON_CANONICAL,
    CANONICAL_REORG_REQUIRED,
    INVALID_CERTIFICATE,
    ALREADY_HALTED,
    CORRUPT_STORAGE,
    STORAGE_ERROR,
};

/// \brief Статус чтения durable halt evidence для PoA safety.
enum class PoaEvidenceReadStatus : uint8_t {
    NOT_HALTED,
    EQUIVOCATION,
    HALTED_CORRUPT_STORAGE,
    UNAVAILABLE,
};

/// \brief Два конфликтующих PoA-сертификата для одной сети, высоты и родителя.
struct PoaEquivocationEvidence {
    PoaFinalityCertificate first;
    PoaFinalityCertificate second;

    friend bool operator==(const PoaEquivocationEvidence&, const PoaEquivocationEvidence&) = default;
};

/// \brief Результат чтения durable evidence локального safety halt.
struct PoaEvidenceReadResult {
    PoaEvidenceReadStatus status{PoaEvidenceReadStatus::UNAVAILABLE};
    std::optional<PoaEquivocationEvidence> equivocation;
};

/// \brief Хранит наблюдения финальности, фиксирует equivocation и применяет правило min(BlockID).
class PoaConflictDetector final {
public:
    PoaConflictDetector(KVStore& db, const cybou::Hash256& network_binding,
        const IdentityHybridPublicKey& genesis_finalizer_key);

    /// \brief Наблюдает PoA-сертификат и возвращает локальное решение safety guard.
    PoaConflictStatus Observe(const PoaFinalityCertificate& certificate, const CybouBlock& block);
    /// \brief Возвращает факт перехода локального guard в fail-closed состояние.
    bool SafetyHalted() const;
    /// \brief Перечитывает и повторно валидирует durable halt record для расследования.
    PoaEvidenceReadResult ReadSafetyEvidence() const;

private:
    bool PersistHalt(const std::vector<unsigned char>& record) noexcept;

    KVStore& m_db;
    const cybou::Hash256 m_network_binding;
    const IdentityHybridPublicKey m_genesis_finalizer_key;
    const std::string m_prefix;
    mutable std::mutex m_mutex;
    mutable bool m_halted{false};
};

} // namespace cybou

#endif // CYBOU_POA_CONFLICT_DETECTOR_H
