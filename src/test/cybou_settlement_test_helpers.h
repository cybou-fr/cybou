// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_SETTLEMENT_TEST_HELPERS_H
#define CYBOU_SETTLEMENT_TEST_HELPERS_H
#include <cybou/storage_lease.h>
#include <cybou/crypto/sha256.h>
#include <algorithm>
#include <limits>
#include <tuple>
namespace cybou::test {
inline void PayWindow(StorageSettlement& op, uint64_t seconds = 86400)
{
    op.period_end_utc = op.period_start_utc > std::numeric_limits<uint64_t>::max() - seconds
        ? std::numeric_limits<uint64_t>::max() : op.period_start_utc + seconds;
    const std::array<unsigned char,4> empty_count{};
    crypto::ComputeSha256({std::span<const unsigned char>{empty_count}}, op.evidence_root.begin());
}
// Component codec/economy fixture only. Real binding/activation provenance is
// separately exercised through finality on two Full Nodes.
inline void AcceptFixtureAllocation(CybouState& state, const Hash256& publication, const AccountId& provider)
{
    auto& lease = state.leases.at(publication);
    for (auto& term : lease.funded_terms) {
        if (!term.assignments.empty() || term.first_period > state.settlement.next_period) continue;
        std::array<unsigned char,32> storage{}; storage[0] = 1;
        auto prepare = term.funding_operation_id; prepare.begin()[0] ^= 0xA1;
        auto activate = term.funding_operation_id; activate.begin()[0] ^= 0xB2;
        term.declarations.push_back({prepare,1,1,{{storage,provider,state.identities.Find(provider)->key_epoch,1}}});
        term.next_assignment_epoch = 2;
        term.assignments.push_back({activate,prepare,Hash256{123},1,term.first_period,{{0,storage,provider,lease.units}}});
    }
}
inline void FixturePayEntries(StorageSettlement& op, const CybouState& state)
{
    for (auto& e : op.entries) {
        e.storage_id[0] = 1; e.verified_unit_seconds = 1;
        const auto lease = state.leases.find(e.funding_operation_id);
        if (lease == state.leases.end()) continue;
        for (const auto& term : lease->second.funded_terms) {
            if (op.period < term.first_period || op.period >= term.end_period) continue;
            e.funding_operation_id = term.funding_operation_id;
            e.verified_unit_seconds = (op.period + 1 - term.first_period) * lease->second.units * term.period_seconds;
            for (const auto& a : term.assignments) op.activation_witnesses.push_back(a.operation_id);
            break;
        }
    }
    std::sort(op.activation_witnesses.begin(),op.activation_witnesses.end());
    op.activation_witnesses.erase(std::unique(op.activation_witnesses.begin(),op.activation_witnesses.end()),op.activation_witnesses.end());
    std::sort(op.entries.begin(),op.entries.end(),[](const auto& a,const auto& b) {
        return std::tie(a.funding_operation_id,a.slot,a.storage_id,a.payout_account) <
            std::tie(b.funding_operation_id,b.slot,b.storage_id,b.payout_account); });
}
}
#endif
