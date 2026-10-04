// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Координация durable journaling и локальной PoA-подписи блоков/adjustments.

#ifndef CYBOU_POA_FINALIZER_H
#define CYBOU_POA_FINALIZER_H

#include <cybou/poa_finality.h>
#include <cybou/poa_signing_journal.h>
#include <cybou/recovery_phrase.h>
#include <cybou/block.h>
#include <cybou/poa_signer.h>

namespace cybou {

/// \brief Итог попытки выпустить PoA finality certificate.
enum class PoaSigningStatus : uint8_t {
    SIGNED,           ///< Durable intent записан и подпись выпущена.
    ALREADY_PREPARED, ///< Тот же `BlockID` уже journaled ранее; подпись перевыпущена детерминированно.
    JOURNAL_REJECTED, ///< Журнал отказал до подписи ради safety.
    SIGNING_FAILED,   ///< Signer недоступен или не смог выпустить/самопроверить подпись.
};

/// \brief Результат journaling+signing финализирующего сертификата.
struct PoaSigningResult {
    PoaSigningStatus status{PoaSigningStatus::SIGNING_FAILED}; ///< Итог попытки.
    PoaJournalStatus journal_status{PoaJournalStatus::NONE}; ///< Детализация ответа журнала.
    std::optional<PoaFinalityCertificate> certificate; ///< Сертификат при `SIGNED` или `ALREADY_PREPARED`.
};

/// \brief Обёртка над журналом и signer'ом, требующая durable intent до подписи.
class PoaFinalizer final {
public:
    PoaFinalizer(KVStore& db, const cybou::Hash256& network_binding,
        const cybou::Hash256& genesis_anchor, const IdentityHybridPublicKey& genesis_finalizer_key);
    PoaFinalizer(KVStore& db, const cybou::Hash256& network_binding,
        const cybou::Hash256& genesis_anchor, const RecoveryEntropy& operator_recovery_entropy,
        const IdentityHybridPublicKey& genesis_finalizer_key);
    ~PoaFinalizer() = default;
    PoaFinalizer(const PoaFinalizer&) = delete;
    PoaFinalizer& operator=(const PoaFinalizer&) = delete;
    PoaFinalizer(PoaFinalizer&&) = delete;
    PoaFinalizer& operator=(PoaFinalizer&&) = delete;

    /// \brief Проверяет совместимость текущего канонического tip с локальным журналом.
    /// \param finalized_height Локально признанная финализированная высота.
    /// \param finalized_tip Локально признанный `BlockID` на этой высоте.
    /// \return Статус журнала; при конфликте журнал может перейти в fail-closed halt.
    PoaJournalStatus CheckCanonicalTip(uint64_t finalized_height, const cybou::Hash256& finalized_tip);
    /// \brief Сообщает, остановлен ли финализатор локальным safety halt.
    bool SafetyHalted() const { return m_journal.SafetyHalted(); }
    /// \brief Подключает signer, если его публичный ключ совпадает с genesis-authorized PoA key.
    /// \return `true` только если signer существует и его публичный ключ в точности совпадает с ключом genesis.
    bool EnableSigner(PoaSignerRef signer);
    /// \brief Отключает текущий локальный signer.
    void DisableSigner();
    /// \brief Возвращает признак наличия активного signer'а.
    bool SignerEnabled() const { return static_cast<bool>(m_signer); }
    /// \brief Журналирует intent и, если это безопасно, подписывает блок PoA finality certificate.
    /// \param finalized_height Текущая локальная финализированная высота.
    /// \param finalized_tip Текущий локальный финализированный `BlockID`.
    /// \param block Кандидатный следующий блок.
    /// \return Структура с итогом durable intent + signing workflow.
    /// \pre `block` уже детерминированно исполнен поверх `finalized_tip`.
    /// \post Без успешного `PrepareToSign` подпись не выпускается.
    PoaSigningResult SignFinality(uint64_t finalized_height, const cybou::Hash256& finalized_tip,
        const CybouBlock& block);
    /// \brief Заполняет PoA-подпись StorageSettlement одного периода (DEC-282).
    /// \return `true`, если подпись выпущена и локально самопроверена.
    bool SignStorageSettlement(StorageSettlement& settlement) const;

private:
    const cybou::Hash256 m_network_binding;
    IdentityHybridPublicKey m_public_key;
    PoaSignerRef m_signer;
    PoaSigningJournal m_journal;
};

} // namespace cybou

#endif // CYBOU_POA_FINALIZER_H
