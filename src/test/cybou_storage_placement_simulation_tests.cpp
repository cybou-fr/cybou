// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief M6 release gates: provider concentration and Sybil simulations of placement policy (DEC-280/283).
///
/// The model mirrors StorageService::Place: for every chunk a uniformly random economic identity
/// (payout account) with free provider budget (`2V/3`) is chosen, then one of its nodes, and the
/// replicas of a chunk go to distinct accounts. Units are 1 GiB placements so a thousand-node
/// network runs in milliseconds.

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace {

struct Node {
    uint64_t budget{0}; ///< Provider budget, GiB: floor(2V/3).
    uint64_t used{0};
    int owner{0};       ///< Economic identity (payout account) for Sybil accounting.
};

/// BY_ACCOUNT is the implemented rule (DEC-280); the others are rejected alternatives for comparison.
enum class Policy { BY_ACCOUNT, BY_STORAGE_ID, CAPACITY_WEIGHTED };

/// Places `chunks` logical GiB with `replicas` distinct providers each; returns false if the network is full.
bool Place(std::vector<Node>& nodes, uint64_t chunks, int replicas, Policy policy, std::mt19937_64& rng)
{
    std::vector<size_t> eligible(nodes.size());
    std::iota(eligible.begin(), eligible.end(), size_t{0});
    for (uint64_t chunk{0}; chunk < chunks; ++chunk) {
        std::vector<size_t> chosen;
        while (static_cast<int>(chosen.size()) < replicas) {
            std::erase_if(eligible, [&](size_t i) { return nodes[i].used >= nodes[i].budget; });
            std::vector<size_t> candidates;
            for (const auto i : eligible) {
                const bool same_account = std::any_of(chosen.begin(), chosen.end(),
                    [&](size_t c) { return nodes[c].owner == nodes[i].owner; });
                if (std::find(chosen.begin(), chosen.end(), i) == chosen.end() &&
                    (policy != Policy::BY_ACCOUNT || !same_account)) candidates.push_back(i);
            }
            if (candidates.empty()) return false;
            size_t pick{0};
            if (policy == Policy::BY_ACCOUNT) {
                std::vector<int> owners;
                for (const auto i : candidates) owners.push_back(nodes[i].owner);
                std::sort(owners.begin(), owners.end());
                owners.erase(std::unique(owners.begin(), owners.end()), owners.end());
                const int owner = owners[rng() % owners.size()];
                std::vector<size_t> members;
                for (const auto i : candidates) if (nodes[i].owner == owner) members.push_back(i);
                pick = members[rng() % members.size()];
            } else if (policy == Policy::BY_STORAGE_ID) {
                pick = candidates[rng() % candidates.size()];
            } else {
                // The rejected alternative: chance proportional to free capacity.
                uint64_t free{0};
                for (const auto i : candidates) free += nodes[i].budget - nodes[i].used;
                uint64_t draw = rng() % free;
                for (const auto i : candidates) {
                    const auto weight = nodes[i].budget - nodes[i].used;
                    if (draw < weight) { pick = i; break; }
                    draw -= weight;
                }
            }
            ++nodes[pick].used;
            chosen.push_back(pick);
        }
    }
    return true;
}

struct Concentration {
    double top_1_percent{0}, top_5_nodes{0}, top_10_percent{0}, effective_providers{0}, small_nodes_working{0};
};

Concentration Measure(const std::vector<Node>& nodes, uint64_t small_budget)
{
    std::vector<uint64_t> used;
    uint64_t total{0}, small{0}, small_working{0};
    for (const auto& node : nodes) {
        used.push_back(node.used);
        total += node.used;
        if (node.budget == small_budget) {
            ++small;
            small_working += node.used > 0;
        }
    }
    std::sort(used.rbegin(), used.rend());
    const auto share = [&](size_t count) {
        return static_cast<double>(std::accumulate(used.begin(), used.begin() + count, uint64_t{0})) / total;
    };
    double sum_squares{0};
    for (const auto value : used) sum_squares += std::pow(static_cast<double>(value) / total, 2);
    return {share(nodes.size() / 100), share(5), share(nodes.size() / 10), 1.0 / sum_squares,
        small == 0 ? 0 : static_cast<double>(small_working) / small};
}

/// 1000 nodes: 700 x 15 GiB, 200 x 150 GiB, 80 x 1 TiB, 20 x 20 TiB (DEC-283 simulation profile).
std::vector<Node> MixedNetwork()
{
    std::vector<Node> nodes;
    const auto add = [&](int count, uint64_t capacity_gib) {
        for (int i{0}; i < count; ++i) nodes.push_back({capacity_gib * 2 / 3, 0, static_cast<int>(nodes.size())});
    };
    add(700, 15);
    add(200, 150);
    add(80, 1024);
    add(20, 20 * 1024);
    return nodes;
}

uint64_t TotalBudget(const std::vector<Node>& nodes)
{
    return std::accumulate(nodes.begin(), nodes.end(), uint64_t{0}, [](uint64_t sum, const Node& n) { return sum + n.budget; });
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_storage_placement_simulation_tests)

BOOST_AUTO_TEST_CASE(uniform_placement_keeps_home_nodes_working_and_bounds_concentration)
{
    for (const double demand : {0.05, 0.3, 0.7}) {
        auto uniform = MixedNetwork();
        auto weighted = MixedNetwork();
        const auto chunks = static_cast<uint64_t>(TotalBudget(uniform) * demand / 2);
        std::mt19937_64 rng_a{42}, rng_b{42};
        BOOST_REQUIRE(Place(uniform, chunks, 2, Policy::BY_ACCOUNT, rng_a));
        BOOST_REQUIRE(Place(weighted, chunks, 2, Policy::CAPACITY_WEIGHTED, rng_b));
        const auto u = Measure(uniform, 10);
        const auto w = Measure(weighted, 10);
        BOOST_TEST_MESSAGE("demand " << demand << " uniform: top1% " << u.top_1_percent << " top5 " << u.top_5_nodes
            << " top10% " << u.top_10_percent << " effective " << u.effective_providers << " home nodes working "
            << u.small_nodes_working << " | weighted: top1% " << w.top_1_percent << " effective " << w.effective_providers
            << " home nodes working " << w.small_nodes_working);
        // Release gate: the five largest operators never hold 70% (DEC-283 example threshold).
        BOOST_CHECK_LT(u.top_5_nodes, 0.70);
        // Every ordinary 15 GiB home node receives work under uniform selection.
        BOOST_CHECK_EQUAL(u.small_nodes_working, 1.0);
        // Uniform selection is never more concentrated than capacity weighting.
        BOOST_CHECK_LE(u.top_1_percent, w.top_1_percent + 1e-9);
        BOOST_CHECK_GE(u.effective_providers, w.effective_providers - 1e-9);
    }
}

BOOST_AUTO_TEST_CASE(sybil_splitting_gains_share_only_by_paying_per_identity)
{
    // One actor offers the same 15 TiB as one node, as 100 nodes under one payout account
    // (free: StorageIds cost nothing), or as 100 Identities with their own accounts (each
    // priced by AccountCreate PoW). Selection by payout account removes the free gain (DEC-280).
    enum class Shape { ONE_NODE, NODES_ONE_ACCOUNT, IDENTITIES };
    const auto run = [](Shape shape, Policy policy) {
        std::vector<Node> nodes;
        for (int i{0}; i < 900; ++i) nodes.push_back({150 * 2 / 3, 0, i});
        if (shape == Shape::ONE_NODE) {
            nodes.push_back({15 * 1024 * 2 / 3, 0, -1});
        } else {
            for (int i{0}; i < 100; ++i) nodes.push_back({150 * 2 / 3, 0, shape == Shape::IDENTITIES ? -1 - i : -1});
        }
        std::mt19937_64 rng{7};
        const auto chunks = static_cast<uint64_t>(TotalBudget(nodes) * 0.1 / 2);
        BOOST_REQUIRE(Place(nodes, chunks, 2, policy, rng));
        uint64_t actor{0}, total{0};
        for (const auto& node : nodes) {
            total += node.used;
            if (node.owner < 0) actor += node.used;
        }
        return static_cast<double>(actor) / total;
    };
    const double single = run(Shape::ONE_NODE, Policy::BY_ACCOUNT);
    const double farm = run(Shape::NODES_ONE_ACCOUNT, Policy::BY_ACCOUNT);
    const double farm_by_storage_id = run(Shape::NODES_ONE_ACCOUNT, Policy::BY_STORAGE_ID);
    const double identities = run(Shape::IDENTITIES, Policy::BY_ACCOUNT);
    BOOST_TEST_MESSAGE("actor share: one node " << single << " | 100 nodes one account " << farm
        << " (by StorageId would be " << farm_by_storage_id << ") | 100 Identities " << identities);
    // Free splitting into StorageIds no longer pays: the farm counts as one account.
    BOOST_CHECK_LE(farm, single * 1.5 + 1e-9);
    BOOST_CHECK_GT(farm_by_storage_id, 20 * single);
    // Splitting into real Identities still gains share; AccountCreate PoW is its price (open gate).
    BOOST_CHECK_GT(identities, 20 * single);
    BOOST_CHECK_LE(identities, 100.0 / 1000.0 + 0.01);
}

BOOST_AUTO_TEST_SUITE_END()
