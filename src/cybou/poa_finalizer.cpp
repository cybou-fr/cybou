// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Реализация локального PoA finalizer'а поверх durable journal и signer boundary.

#include <cybou/poa_finalizer.h>

#include <cybou/crypto/cleanse.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

namespace cybou {
namespace {

bool Nonzero(const std::span<const unsigned char> bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](const unsigned char value) { return value != 0; });
}

const IdentityHybridPublicKey& ValidateRecoveryPoaKey(
    const RecoveryEntropy& entropy, const IdentityHybridPublicKey& expected)
{
    // Производим локальную сверку recovery phrase -> genesis PoA key до любого
    // подписывания: PoA authority следует только из genesis-authorized key, а не из среды исполнения.
    if (!Nonzero(entropy)) throw std::invalid_argument{"empty operator recovery entropy"};
    const auto derived = DeriveIdentityPublicKey(entropy, IdentityKeyPurpose::POA_FINALIZER);
    if (!derived || *derived != expected) {
        throw std::invalid_argument{"operator recovery phrase does not match genesis PoA key"};
    }
    return expected;
}

class RecoveryEntropyPoaSigner final : public PoaSigner {
public:
    explicit RecoveryEntropyPoaSigner(const RecoveryEntropy& entropy) : m_entropy{entropy}
    {
        auto key = DeriveIdentityPublicKey(m_entropy, IdentityKeyPurpose::POA_FINALIZER);
        if (!key) throw std::invalid_argument{"cannot derive operator PoA key"};
        m_public_key = std::move(*key);
    }
    ~RecoveryEntropyPoaSigner() override { crypto::CleanseMemory(m_entropy.data(), m_entropy.size()); }
    std::optional<IdentityHybridPublicKey> PublicKey() const override { return m_public_key; }
    std::optional<IdentityHybridSignature> Sign(const std::span<const unsigned char> message) const override
    {
        if (message.empty()) return std::nullopt;
        return SignIdentityMessage(m_entropy, IdentityKeyPurpose::POA_FINALIZER, message);
    }

private:
    RecoveryEntropy m_entropy;
    IdentityHybridPublicKey m_public_key;
};

std::optional<PoaFinalityCertificate> CreateCertificate(
    const IdentityHybridSignature& signature, const cybou::Hash256& network_binding,
    const cybou::Hash256& block_id, const uint64_t height, const cybou::Hash256& parent_block_id)
{
    if (network_binding.IsNull() || block_id.IsNull() || height == 0 || parent_block_id.IsNull()) return std::nullopt;
    PoaFinalityCertificate certificate{
        .network_binding = network_binding,
        .block_id = block_id,
        .height = height,
        .parent_block_id = parent_block_id,
        .signature = signature,
    };
    if (!SerializePoaFinalityCertificate(certificate)) return std::nullopt;
    return certificate;
}

} // namespace

PoaFinalizer::PoaFinalizer(KVStore& db, const cybou::Hash256& network_binding,
    const cybou::Hash256& genesis_anchor, const IdentityHybridPublicKey& genesis_finalizer_key)
    : m_network_binding{network_binding}, m_public_key{genesis_finalizer_key},
      m_journal{db, network_binding, genesis_anchor, m_public_key}
{
    if (genesis_finalizer_key.purpose != IdentityKeyPurpose::POA_FINALIZER ||
        genesis_finalizer_key.ml_dsa.size() != 1952) {
        throw std::invalid_argument{"invalid genesis PoA public key"};
    }
}

PoaFinalizer::PoaFinalizer(KVStore& db, const cybou::Hash256& network_binding,
    const cybou::Hash256& genesis_anchor, const RecoveryEntropy& operator_recovery_entropy,
    const IdentityHybridPublicKey& genesis_finalizer_key)
    : PoaFinalizer{db, network_binding, genesis_anchor,
          ValidateRecoveryPoaKey(operator_recovery_entropy, genesis_finalizer_key)}
{
    auto signer = std::make_shared<RecoveryEntropyPoaSigner>(operator_recovery_entropy);
    if (!EnableSigner(std::move(signer)) && !m_journal.SafetyHalted()) {
        throw std::invalid_argument{"operator recovery phrase does not match genesis PoA key"};
    }
}

bool PoaFinalizer::EnableSigner(PoaSignerRef signer)
{
    if (!signer) return false;
    const auto key = signer->PublicKey();
    if (!key || *key != m_public_key) return false;
    m_signer = std::move(signer);
    return true;
}

void PoaFinalizer::DisableSigner()
{
    m_signer.reset();
}

PoaJournalStatus PoaFinalizer::CheckCanonicalTip(
    const uint64_t finalized_height, const cybou::Hash256& finalized_tip)
{
    return m_journal.CheckCanonicalTip(finalized_height, finalized_tip);
}

PoaSigningResult PoaFinalizer::SignFinality(const uint64_t finalized_height,
    const cybou::Hash256& finalized_tip, const CybouBlock& block)
{
    if (!SerializeBlock(block)) return {.status = PoaSigningStatus::SIGNING_FAILED};
    const auto block_id = ComputeBlockId(block);
    if (block_id.IsNull()) return {.status = PoaSigningStatus::SIGNING_FAILED};
    if (!m_signer || m_journal.SafetyHalted()) {
        return {.status = PoaSigningStatus::SIGNING_FAILED};
    }
    const auto history_status = m_journal.CheckCanonicalTip(finalized_height, finalized_tip);
    if (history_status != PoaJournalStatus::NONE) {
        return {.status = PoaSigningStatus::JOURNAL_REJECTED, .journal_status = history_status};
    }
    const auto journal_status = m_journal.PrepareToSign(block.height, block.parent_block_id, block_id);
    if (journal_status != PoaJournalStatus::NONE && journal_status != PoaJournalStatus::ALREADY_PREPARED) {
        return {.status = PoaSigningStatus::JOURNAL_REJECTED, .journal_status = journal_status};
    }
    if (!m_signer) return {.status = PoaSigningStatus::SIGNING_FAILED, .journal_status = journal_status};
    const auto digest = ComputePoaFinalityDigest(m_network_binding, block_id, block.height, block.parent_block_id);
    const auto signature = m_signer->Sign(digest);
    // Самопроверка подписи удерживает boundary signer'а fail-closed: наружный signer
    // не может тихо вернуть некорректный сертификат.
    if (!signature || !VerifyIdentityMessage(m_public_key, *signature, digest)) {
        return {.status = PoaSigningStatus::SIGNING_FAILED, .journal_status = journal_status};
    }
    const auto certificate = CreateCertificate(*signature, m_network_binding, block_id, block.height, block.parent_block_id);
    if (!certificate) return {.status = PoaSigningStatus::SIGNING_FAILED, .journal_status = journal_status};
    return {
        .status = journal_status == PoaJournalStatus::ALREADY_PREPARED ?
            PoaSigningStatus::ALREADY_PREPARED : PoaSigningStatus::SIGNED,
        .journal_status = journal_status,
        .certificate = *certificate,
    };
}

bool PoaFinalizer::SignStorageSettlement(StorageSettlement& settlement) const
{
    const auto digest = ComputeStorageSettlementDigest(m_network_binding, settlement);
    if (!digest || m_journal.SafetyHalted() || !m_signer) return false;
    const auto signature = m_signer->Sign(*digest);
    if (!signature || !VerifyIdentityMessage(m_public_key, *signature, *digest)) return false;
    settlement.poa_signature = *signature;
    return true;
}

} // namespace cybou
