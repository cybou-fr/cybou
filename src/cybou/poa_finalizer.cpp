// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/poa_finalizer.h>

#include <cybou/crypto/cleanse.h>

#include <algorithm>
#include <stdexcept>

namespace cybou {
namespace {

bool Nonzero(const std::span<const unsigned char> bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](const unsigned char value) { return value != 0; });
}

std::optional<PoaFinalityCertificate> SignFinalityCertificate(
    const std::span<const unsigned char, 32> operator_recovery_entropy,
    const uint256& network_id, const uint256& block_id, const uint64_t height,
    const uint256& parent_block_id)
{
    if (!Nonzero(operator_recovery_entropy) || network_id.IsNull() || block_id.IsNull() ||
        height == 0 || parent_block_id.IsNull()) return std::nullopt;
    const auto digest = ComputePoaFinalityDigest(network_id, block_id, height, parent_block_id);
    const auto signature = SignIdentityMessage(operator_recovery_entropy,
        IdentityKeyPurpose::POA_FINALIZER, digest);
    if (!signature) return std::nullopt;
    PoaFinalityCertificate certificate{
        .version = POA_FINALITY_CERTIFICATE_VERSION,
        .network_id = network_id,
        .block_id = block_id,
        .height = height,
        .parent_block_id = parent_block_id,
        .signature = *signature,
    };
    if (!SerializePoaFinalityCertificate(certificate)) return std::nullopt;
    return certificate;
}

} // namespace

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
    const auto certificate = SignFinalityCertificate(m_operator_recovery_entropy,
        m_network_id, block_id, block.height, block.parent_block_id);
    if (!certificate) return {.status = PoaSigningStatus::SIGNING_FAILED, .journal_status = journal_status};
    return {
        .status = journal_status == PoaJournalStatus::ALREADY_PREPARED ?
            PoaSigningStatus::ALREADY_PREPARED : PoaSigningStatus::SIGNED,
        .journal_status = journal_status,
        .certificate = *certificate,
    };
}

std::optional<IdentityHybridSignature> PoaFinalizer::SignTransportProof(
    const std::span<const unsigned char> message) const
{
    if (message.empty() || m_journal.SafetyHalted()) return std::nullopt;
    return SignIdentityMessage(m_operator_recovery_entropy, IdentityKeyPurpose::POA_FINALIZER, message);
}

} // namespace cybou
