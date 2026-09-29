// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_POA_FINALIZER_H
#define CYBOU_POA_FINALIZER_H

#include <cybou/poa_finality.h>
#include <cybou/poa_signing_journal.h>
#include <cybou/recovery_phrase.h>
#include <cybou/block.h>

namespace cybou {

enum class PoaSigningStatus : uint8_t {
    SIGNED,
    ALREADY_PREPARED,
    JOURNAL_REJECTED,
    SIGNING_FAILED,
};

struct PoaSigningResult {
    PoaSigningStatus status{PoaSigningStatus::SIGNING_FAILED};
    PoaJournalStatus journal_status{PoaJournalStatus::NONE};
    std::optional<PoaFinalityCertificate> certificate;
};

/** Keeps operator recovery entropy in RAM and journals every intent before signing. */
class PoaFinalizer final {
public:
    PoaFinalizer(KVStore& db, const uint256& network_id,
        const uint256& genesis_block_id, const RecoveryEntropy& operator_recovery_entropy,
        const IdentityHybridPublicKey& genesis_finalizer_key);
    ~PoaFinalizer();
    PoaFinalizer(const PoaFinalizer&) = delete;
    PoaFinalizer& operator=(const PoaFinalizer&) = delete;
    PoaFinalizer(PoaFinalizer&&) = delete;
    PoaFinalizer& operator=(PoaFinalizer&&) = delete;

    PoaJournalStatus CheckCanonicalTip(uint64_t finalized_height, const uint256& finalized_tip);
    bool SafetyHalted() const { return m_journal.SafetyHalted(); }
    PoaSigningResult SignFinality(uint64_t finalized_height, const uint256& finalized_tip,
        const CybouBlock& block);

private:
    const uint256 m_network_id;
    RecoveryEntropy m_operator_recovery_entropy;
    IdentityHybridPublicKey m_public_key;
    PoaSigningJournal m_journal;
};

} // namespace cybou

#endif // CYBOU_POA_FINALIZER_H
