// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// \brief Реализация volatile-пула проверенных Validation-attestations.

#include <cybou/validation_pool.h>

namespace cybou {

ValidationPoolAdd ValidationPool::Add(const ValidationAttestation& attestation)
{
    // Sidecar deliberately shares one finalized base for every operation: once
    // tip changes, mixed-base Validation would only mislead local UX/gossip.
    if (m_base.IsNull() || attestation.finalized_base_block_id != m_base) return ValidationPoolAdd::STALE_BASE;
    auto entry = m_entries.find(attestation.operation_id);
    if (entry == m_entries.end()) {
        if (m_entries.size() >= MAX_VALIDATED_OPERATIONS) return ValidationPoolAdd::FULL;
        entry = m_entries.emplace(attestation.operation_id, std::map<AccountId, ValidationAttestation>{}).first;
    }
    if (entry->second.contains(attestation.validator_account_id)) return ValidationPoolAdd::DUPLICATE;
    if (entry->second.size() >= MAX_ATTESTATIONS_PER_OPERATION) return ValidationPoolAdd::FULL;
    entry->second.emplace(attestation.validator_account_id, attestation);
    return ValidationPoolAdd::ADDED;
}

void ValidationPool::ResetBase(const cybou::Hash256& finalized_tip)
{
    if (finalized_tip == m_base) return;
    m_base = finalized_tip;
    // Validation never creates state, so advancing finality simply invalidates
    // the whole evidence cache instead of migrating entries across bases.
    m_entries.clear();
}

void ValidationPool::Drop(const cybou::Hash256& operation_id)
{
    m_entries.erase(operation_id);
}

size_t ValidationPool::Count(const cybou::Hash256& operation_id) const
{
    const auto entry = m_entries.find(operation_id);
    return entry == m_entries.end() ? 0 : entry->second.size();
}

std::vector<ValidationAttestation> ValidationPool::ForOperation(const cybou::Hash256& operation_id) const
{
    const auto entry = m_entries.find(operation_id);
    if (entry == m_entries.end()) return {};
    std::vector<ValidationAttestation> attestations;
    attestations.reserve(entry->second.size());
    for (const auto& [account, attestation] : entry->second) {
        (void)account;
        attestations.push_back(attestation);
    }
    return attestations;
}

std::optional<ValidationAttestation> ValidationPool::First(const std::function<bool(const Key&)>& skip) const
{
    for (const auto& [operation_id, by_account] : m_entries) {
        for (const auto& [account, attestation] : by_account) {
            if (!skip({operation_id, account})) return attestation;
        }
    }
    return std::nullopt;
}

} // namespace cybou
