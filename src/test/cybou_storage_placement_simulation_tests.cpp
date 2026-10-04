// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief M6 release gates: provider concentration and Sybil simulations of placement policy (DEC-280/283).
///
/// The model mirrors StorageService::Place: for every chunk the eligible providers are taken in
/// uniformly random order and the first ones with free provider budget (`2V/3`) receive distinct
/// replicas. Units are 1 GiB placements so a thousand-node network runs in milliseconds.

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

enum class Policy { UNIFORM, CAPACITY_WEIGHTED };

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
                if (std::find(chosen.begin(), chosen.end(), i) == chosen.end()) candidates.push_back(i);
            }
            if (candidates.empty()) return false;
            size_t pick{0};
            if (policy == Policy::UNIFORM) {
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
        BOOST_REQUIRE(Place(uniform, chunks, 2, Policy::UNIFORM, rng_a));
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
    // One actor offers the same 15 TiB either as one node or split into 100 Identities of
    // 150 GiB. Under uniform selection splitting multiplies the actor's share; the protocol
    // charges AccountCreate PoW per Identity and pays only distinct payout accounts per chunk,
    // so the measured gain is the price that PoW has to cover (DEC-280/283).
    const auto run = [](bool split) {
        std::vector<Node> nodes;
        for (int i{0}; i < 900; ++i) nodes.push_back({150 * 2 / 3, 0, i});
        if (split) {
            for (int i{0}; i < 100; ++i) nodes.push_back({150 * 2 / 3, 0, -1});
        } else {
            nodes.push_back({15 * 1024 * 2 / 3, 0, -1});
        }
        std::mt19937_64 rng{7};
        const auto chunks = static_cast<uint64_t>(TotalBudget(nodes) * 0.1 / 2);
        BOOST_REQUIRE(Place(nodes, chunks, 2, Policy::UNIFORM, rng));
        uint64_t actor{0}, total{0};
        for (const auto& node : nodes) {
            total += node.used;
            if (node.owner == -1) actor += node.used;
        }
        return static_cast<double>(actor) / total;
    };
    const double single = run(false);
    const double split = run(true);
    BOOST_TEST_MESSAGE("actor share: one node " << single << ", 100 Sybil identities " << split
        << ", gain x" << split / single);
    BOOST_CHECK_GT(split, single);
    // The split actor behaves like 100 ordinary honest nodes: never more than its node count share.
    BOOST_CHECK_LE(split, 100.0 / 1000.0 + 0.01);
}

BOOST_AUTO_TEST_SUITE_END()
