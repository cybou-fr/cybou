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
    uint64_t height{0};
    cybou::Hash256 parent_block_id;
    cybou::Hash256 block_id;
    friend bool operator==(const PoaJournalHead&, const PoaJournalHead&) = default;
};

/// \brief Результаты проверки и продвижения durable PoA signing journal.
enum class PoaJournalStatus : uint8_t {
    NONE,
    ALREADY_PREPARED,
    INVALID_REQUEST,
    HISTORY_NOT_VERIFIED,
    HISTORY_MISMATCH,
    HEIGHT_MISMATCH,
    PARENT_MISMATCH,
    EQUIVOCATION,
    JOURNAL_HALTED,
    STORAGE_ERROR,
};

/// \brief Fail-closed журнал, фиксирующий intent до выпуска PoA-подписи.
class PoaSigningJournal final {
public:
    PoaSigningJournal(KVStore& db, const cybou::Hash256& network_binding,
        const cybou::Hash256& genesis_anchor, const IdentityHybridPublicKey& finalizer_key);

    /// \brief Проверяет, что локально видимый канонический tip совместим с журналом.
    PoaJournalStatus CheckCanonicalTip(uint64_t finalized_height, const cybou::Hash256& finalized_tip);

    /// \brief Синхронно фиксирует intent следующего блока до генерации подписи.
    PoaJournalStatus PrepareToSign(uint64_t height,
        const cybou::Hash256& parent_block_id, const cybou::Hash256& block_id);

    /// \brief Возвращает последнюю зафиксированную journal head.
    PoaJournalHead Head() const;
    /// \brief Сообщает, переведён ли журнал в fail-closed состояние.
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
