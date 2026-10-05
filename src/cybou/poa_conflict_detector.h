// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
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
    OBSERVED,                 ///< Сертификат впервые увиден и сохранён как наблюдение.
    ALREADY_OBSERVED,         ///< Тот же сертификат уже был сохранён на этой высоте/родителе.
    SAFETY_CONFLICT,          ///< Зарезервировано для локального safety-конфликта; вызывающая сторона должна halt'иться fail-closed.
    COMPETING_NON_CANONICAL,  ///< Подтверждён equivocating сертификат с `BlockID` больше канонического `min(BlockID)`.
    CANONICAL_REORG_REQUIRED, ///< Подтверждён equivocating сертификат с меньшим `BlockID`; канон должен переключиться.
    INVALID_CERTIFICATE,      ///< Сертификат/блок не проходят PoA-проверку.
    ALREADY_HALTED,           ///< Детектор уже остановлен fail-closed.
    CORRUPT_STORAGE,          ///< Durable observations/halt record повреждены.
    STORAGE_ERROR,            ///< Ошибка записи/чтения не позволяет продолжать безопасно.
};

/// \brief Статус чтения durable halt evidence для PoA safety.
enum class PoaEvidenceReadStatus : uint8_t {
    NOT_HALTED,             ///< Durable halt record отсутствует.
    EQUIVOCATION,           ///< Найдены и заново проверены два конфликтующих сертификата.
    HALTED_CORRUPT_STORAGE, ///< Узел остановлен из-за повреждения durable records.
    UNAVAILABLE,            ///< Доказательство не удалось безопасно перечитать/повторно проверить.
};

/// \brief Два конфликтующих PoA-сертификата для одной сети, высоты и родителя.
struct PoaEquivocationEvidence {
    PoaFinalityCertificate first;  ///< Первое наблюдение для `(network,height,parent)`.
    PoaFinalityCertificate second; ///< Конфликтующее наблюдение с другим `BlockID`.

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
    /// \return Статус, включая детерминированное правило `min(BlockID)` при подтверждённом equivocation.
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
