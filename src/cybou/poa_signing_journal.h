// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Durable journal намерений PoA finalizer'а перед подписью блоков.

#ifndef CYBOU_POA_SIGNING_JOURNAL_H
#define CYBOU_POA_SIGNING_JOURNAL_H

#include <cybou/identity_crypto.h>
#include <cybou/kv_store.h>
#include <cybou/hash256.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <string>

namespace cybou {

/// \brief Последнее зафиксированное намерение PoA finalizer'а.
struct PoaJournalHead {
    uint64_t height{0}; ///< Высота последнего durably-prepared блока.
    cybou::Hash256 parent_block_id; ///< Родитель durably-prepared блока; для genesis-head остаётся нулевым.
    cybou::Hash256 block_id; ///< `BlockID` последнего durably-prepared блока либо genesis anchor на высоте 0.
    friend bool operator==(const PoaJournalHead&, const PoaJournalHead&) = default;
};

/// \brief Результаты проверки и продвижения durable PoA signing journal.
enum class PoaJournalStatus : uint8_t {
    NONE,                 ///< Операция журнала завершена без замечаний.
    ALREADY_PREPARED,     ///< Тот же `height/parent/block` уже durably записан.
    INVALID_REQUEST,      ///< Некорректные входные параметры (нулевая высота, null hash и т.п.).
    HISTORY_NOT_VERIFIED, ///< Канонический tip ещё не проверен против головы журнала.
    HISTORY_MISMATCH,     ///< Наблюдаемая каноническая история расходится с journal head; журнал halt'ится.
    HEIGHT_MISMATCH,      ///< Следующая высота не равна `head.height + 1`.
    PARENT_MISMATCH,      ///< Родитель кандидата не совпадает с journal head.
    EQUIVOCATION,         ///< Для одной высоты уже подготовлен другой `BlockID`; журнал halt'ится.
    JOURNAL_HALTED,       ///< Журнал уже переведён в fail-closed состояние.
    STORAGE_ERROR,        ///< Не удалось надёжно прочитать/записать durable state журнала.
};

/// \brief Fail-closed журнал, фиксирующий intent до выпуска PoA-подписи.
class PoaSigningJournal final {
public:
    PoaSigningJournal(KVStore& db, const cybou::Hash256& network_binding,
        const cybou::Hash256& genesis_anchor, const IdentityHybridPublicKey& finalizer_key);

    /// \brief Проверяет, что локально видимый канонический tip совместим с журналом.
    /// \return `HISTORY_MISMATCH` и fail-closed halt при расхождении с уже journaled историей.
    PoaJournalStatus CheckCanonicalTip(uint64_t finalized_height, const cybou::Hash256& finalized_tip);

    /// \brief Синхронно фиксирует intent следующего блока до генерации подписи.
    /// \pre Перед вызовом должен успешно пройти `CheckCanonicalTip`.
    /// \post При успехе durable head продвигается ровно на один блок; при неоднозначности журнал halt'ится.
    PoaJournalStatus PrepareToSign(uint64_t height,
        const cybou::Hash256& parent_block_id, const cybou::Hash256& block_id);

    /// \brief Возвращает последнюю зафиксированную journal head.
    /// \note Потокобезопасно.
    PoaJournalHead Head() const;
    /// \brief Сообщает, переведён ли журнал в fail-closed состояние.
    /// \note Потокобезопасно.
    bool SafetyHalted() const;

private:
    bool PersistHalt(PoaJournalStatus reason) noexcept;

    KVStore& m_db;
    const cybou::Hash256 m_network_binding;
    const cybou::Hash256 m_genesis_anchor;
    const std::array<unsigned char, 32> m_finalizer_key_id;
    const std::string m_prefix;
    mutable std::mutex m_mutex;
    PoaJournalHead m_head;
    uint64_t m_verified_height{0};
    cybou::Hash256 m_verified_tip;
    bool m_history_verified{false};
    bool m_halted{false};
};

} // namespace cybou

#endif // CYBOU_POA_SIGNING_JOURNAL_H
