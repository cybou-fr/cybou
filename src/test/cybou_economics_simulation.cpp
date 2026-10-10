// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
// BUILD_TESTS-only offline model. No node, signer, publication or network I/O.
#include <test/cybou_economics_scenarios.h>
#include <cybou/economics_quote.h>
#include <cybou/official_networks.h>
#include <cybou/protocol_operation.h>
#include <cybou/storage_economy.h>
#include <cybou/storage_lease.h>
#include <iostream>
#include <stdexcept>

int main()
{
    try {
        const auto& network = cybou::RequireOfficialNetwork("devnet");
        const auto& p = network.genesis.GetProtocolParameters();
        if (p.storage_rate_per_gib_day_replica != cybou::STORAGE_RATE_CYBOU_PER_GIB_DAY_REPLICA)
            throw std::runtime_error{"shadow accrual helper rate differs from compiled parameters"};
        struct Scenario { const char* name; std::uint64_t count, bytes; std::size_t capsules; };
        const Scenario scenarios[]{{"short_mail", 10000, 0, 2}, {"mail_attachment_256_kib", 1000, 256 * 1024, 2},
            {"document_1_mib", 100, 1 << 20, 1}, {"file_1_gib", 10, 1ULL << 30, 1}, {"metadata_changes", 1000, 0, 1}};
        std::cout << "{\"rate\":" << p.storage_rate_per_gib_day_replica << ",\"replicas\":" << unsigned(p.storage_replica_target)
            << ",\"period_seconds\":" << p.storage_settlement_period_seconds << ",\"workloads\":[";
        bool first{true};
        std::uint64_t earned_low{0}, earned_high{0};
        for (const auto days : {30U, 90U, 365U}) for (const auto& scenario : scenarios) {
            const auto [low, high] = cybou::test::PublicationChunkBounds(scenario.bytes);
            const auto a = cybou::QuotePublicationCost(p, cybou::test::SamplePublication(low, days, scenario.capsules));
            const auto b = cybou::QuotePublicationCost(p, cybou::test::SamplePublication(high, days, scenario.capsules));
            if (!a || !b) throw std::runtime_error{"workload quote failed"};
            cybou::StorageRentAccumulator rent_a, rent_b;
            if (!cybou::AccrueStorageRent(rent_a, low, days * p.storage_settlement_period_seconds, p.storage_replica_target) ||
                !cybou::AccrueStorageRent(rent_b, high, days * p.storage_settlement_period_seconds, p.storage_replica_target))
                throw std::runtime_error{"workload accrual overflow"};
            const auto encoded = cybou::SerializeProtocolOperation(cybou::ProtocolOperation{cybou::test::SamplePublication(low, days, scenario.capsules)});
            if (!encoded) throw std::runtime_error{"sample operation cannot be serialized"};
            auto strategy_a = a->total_system_debit, strategy_b = b->total_system_debit;
            if (days > 30) {
                const auto initial_a = cybou::QuotePublicationCost(p, cybou::test::SamplePublication(low, 30, scenario.capsules));
                const auto initial_b = cybou::QuotePublicationCost(p, cybou::test::SamplePublication(high, 30, scenario.capsules));
                const auto renewal_a = cybou::QuoteStorageLeaseCost(p, low, p.storage_replica_target, days - 30);
                const auto renewal_b = cybou::QuoteStorageLeaseCost(p, high, p.storage_replica_target, days - 30);
                if (!initial_a || !initial_b || !renewal_a || !renewal_b) throw std::runtime_error{"renewal quote failed"};
                strategy_a = initial_a->total_system_debit + renewal_a->total_system_debit;
                strategy_b = initial_b->total_system_debit + renewal_b->total_system_debit;
            }
            if (!first) std::cout << ',';
            first = false;
            std::cout << "{\"scenario\":\"" << scenario.name << "\",\"objects\":" << scenario.count << ",\"days\":" << days
                << ",\"bytes_per_object\":" << scenario.bytes << ",\"chunks_min\":" << low << ",\"chunks_max\":" << high
                << ",\"operation_bytes\":" << encoded->size()
                << ",\"treasury_min\":" << a->publication_fee * scenario.count << ",\"treasury_max\":" << b->publication_fee * scenario.count
                << ",\"escrow_min\":" << a->storage_escrow * scenario.count << ",\"escrow_max\":" << b->storage_escrow * scenario.count
                << ",\"debit_min\":" << a->total_system_debit * scenario.count << ",\"debit_max\":" << b->total_system_debit * scenario.count
                << ",\"initial_30_then_renew_debit_min\":" << strategy_a * scenario.count
                << ",\"initial_30_then_renew_debit_max\":" << strategy_b * scenario.count
                << ",\"full_service_floor_min\":" << rent_a.cybou * scenario.count << ",\"full_service_floor_max\":" << rent_b.cybou * scenario.count
                << ",\"refund_model_at_min_chunks\":" << (a->storage_escrow - rent_a.cybou) * scenario.count
                << ",\"refund_model_at_max_chunks\":" << (b->storage_escrow - rent_b.cybou) * scenario.count << '}';
            if (days == 30) { earned_low += rent_a.cybou * scenario.count; earned_high += rent_b.cybou * scenario.count; }
        }
        std::cout << "],\"provider_models\":[";
        first = true;
        for (const auto providers : {10U, 100U, 1000U}) {
            if (!first) std::cout << ',';
            first = false;
            std::cout << "{\"providers\":" << providers << ",\"equal_share_floor_min\":" << earned_low / providers
                << ",\"equal_share_floor_max\":" << earned_high / providers << '}';
        }
        const auto tiny_escrow = cybou::ComputeStorageLeaseEscrow(p, 1, p.storage_replica_target, 30);
        const auto tiny_budget = cybou::ComputeAssignedStorageBudget(1, p.storage_replica_target, 30, p.storage_settlement_period_seconds, p.storage_rate_per_gib_day_replica);
        const auto tiny_entitlement = cybou::ComputeAssignedStoragePayout(*tiny_budget, p.storage_settlement_period_seconds, 0);
        std::cout << "],\"onboarding_count\":10000,\"onboarding_treasury_debit\":" << 10000ULL * p.onboarding_bonus
            << ",\"tiny_30_day_escrow\":" << *tiny_escrow << ",\"tiny_first_period_entitlement_per_replica\":" << *tiny_entitlement << "}\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
