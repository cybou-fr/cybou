// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_POA_SIGNING_JOURNAL_H
#define CYBOU_POA_SIGNING_JOURNAL_H

#include <cybou/identity_crypto.h>
#include <cybou/kv_store.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <string>

namespace cybou {

struct PoaJournalHead {
    uint64_t height{0};
    uint256 parent_block_id;
    uint256 block_id;
    friend bool operator==(const PoaJournalHead&, const PoaJournalHead&) = default;
};

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

/** Durable pre-sign intent journal. It never stores private signing material. */
class PoaSigningJournal final {
public:
    PoaSigningJournal(KVStore& db, const uint256& network_binding,
        const uint256& genesis_block_id, const IdentityHybridPublicKey& finalizer_key);

    /** Check the canonical head before the finalizer session may sign. */
    PoaJournalStatus CheckCanonicalTip(uint64_t finalized_height, const uint256& finalized_tip);

    /** Persist the next block intent synchronously before producing its signature. */
    PoaJournalStatus PrepareToSign(uint64_t height,
        const uint256& parent_block_id, const uint256& block_id);

    PoaJournalHead Head() const;
    bool SafetyHalted() const;

private:
    bool PersistHalt(PoaJournalStatus reason) noexcept;

    KVStore& m_db;
    const uint256 m_network_binding;
    const uint256 m_genesis_block_id;
    const std::array<unsigned char, 32> m_finalizer_key_id;
    const std::string m_prefix;
    mutable std::mutex m_mutex;
    PoaJournalHead m_head;
    uint64_t m_verified_height{0};
    uint256 m_verified_tip;
    bool m_history_verified{false};
    bool m_halted{false};
};

} // namespace cybou

#endif // CYBOU_POA_SIGNING_JOURNAL_H
