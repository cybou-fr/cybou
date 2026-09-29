// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/poa_finalizer.h>

#include <cybou/crypto/cleanse.h>

#include <stdexcept>

namespace cybou {

PoaFinalizer::PoaFinalizer(KVStore& db, const uint256& network_id,
    const uint256& genesis_block_id, const RecoveryEntropy& operator_recovery_entropy,
    const IdentityHybridPublicKey& genesis_finalizer_key)
    : m_network_id{network_id},
      m_operator_recovery_entropy{operator_recovery_entropy},
      m_public_key{[&operator_recovery_entropy, &genesis_finalizer_key] {
          const auto derived = DeriveIdentityPublicKey(operator_recovery_entropy, IdentityKeyPurpose::POA_FINALIZER);
          if (!derived || *derived != genesis_finalizer_key) {
              throw std::invalid_argument{"operator recovery phrase does not match genesis PoA key"};
          }
          return *derived;
      }()},
      m_journal{db, network_id, genesis_block_id, m_public_key}
{
}

PoaFinalizer::~PoaFinalizer()
{
    crypto::CleanseMemory(m_operator_recovery_entropy.data(), m_operator_recovery_entropy.size());
}

PoaJournalStatus PoaFinalizer::CheckCanonicalTip(
    const uint64_t finalized_height, const uint256& finalized_tip)
{
    return m_journal.CheckCanonicalTip(finalized_height, finalized_tip);
}

PoaSigningResult PoaFinalizer::SignFinality(const uint64_t finalized_height,
    const uint256& finalized_tip, const CybouBlock& block)
{
    if (!SerializeBlock(block)) return {.status = PoaSigningStatus::SIGNING_FAILED};
    const auto block_id = ComputeBlockId(block);
    if (block_id.IsNull()) return {.status = PoaSigningStatus::SIGNING_FAILED};
    const auto history_status = m_journal.CheckCanonicalTip(finalized_height, finalized_tip);
    if (history_status != PoaJournalStatus::NONE) {
        return {.status = PoaSigningStatus::JOURNAL_REJECTED, .journal_status = history_status};
    }
    const auto journal_status = m_journal.PrepareToSign(block.height, block.parent_block_id, block_id);
    if (journal_status != PoaJournalStatus::NONE && journal_status != PoaJournalStatus::ALREADY_PREPARED) {
        return {.status = PoaSigningStatus::JOURNAL_REJECTED, .journal_status = journal_status};
    }
    const auto certificate = SignPoaFinalityCertificate(m_operator_recovery_entropy,
        m_network_id, block_id, block.height, block.parent_block_id);
    if (!certificate) return {.status = PoaSigningStatus::SIGNING_FAILED, .journal_status = journal_status};
    return {
        .status = journal_status == PoaJournalStatus::ALREADY_PREPARED ?
            PoaSigningStatus::ALREADY_PREPARED : PoaSigningStatus::SIGNED,
        .journal_status = journal_status,
        .certificate = *certificate,
    };
}

} // namespace cybou
