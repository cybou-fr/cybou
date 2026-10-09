// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment.h>
#include <cybou/hash256.h>
#include <test/cybou_service_test_fixture.h>
#include <boost/test/unit_test.hpp>
#include <algorithm>

namespace {
cybou::StorageAssignmentContext Context()
{
    cybou::StorageAssignmentContext c;
    c.network_binding.fill(1); c.publication.fill(2); c.chunk.fill(3);
    c.finalized_seed.fill(4); c.payer.fill(5);
    c.epoch = 7; c.term_start = 10; c.term_end = 40;
    return c;
}
std::vector<cybou::StorageAssignmentProvider> Providers()
{
    std::vector<cybou::StorageAssignmentProvider> result;
    for (unsigned char i{10}; i < 18; ++i) {
        cybou::StorageAssignmentProvider p;
        p.storage_id.fill(i); p.payout_account.fill(20 + i % 3);
        result.push_back(p);
    }
    return result;
}
}
BOOST_AUTO_TEST_SUITE(cybou_storage_assignment_tests)

BOOST_AUTO_TEST_CASE(assignment_is_reproducible_and_uses_distinct_economic_identities)
{
    const auto context = Context();
    auto providers = Providers();
    const auto plan = cybou::PrepareStorageAssignment(context, providers);
    BOOST_REQUIRE(plan);
    BOOST_REQUIRE_EQUAL(plan->selected.size(), 2U);
    // Independently reproduced with Python hashlib/struct; fixes byte order,
    // draw counter scope, candidate ordering and replica-slot order.
    BOOST_CHECK_EQUAL(plan->selected[0].storage_id[0], 15U);
    BOOST_CHECK_EQUAL(plan->selected[1].storage_id[0], 10U);
    const auto commitment_hex = cybou::Hash256{std::span<const unsigned char, 32>{plan->commitment}}.GetHex();
    BOOST_CHECK_EQUAL(commitment_hex,
        "7bb3ccdf9dc723d8294ab4ede6c7849cac989f4a44cc9d7098df33db66828a72");
    BOOST_CHECK(plan->selected[0].storage_id != plan->selected[1].storage_id);
    BOOST_CHECK(plan->selected[0].payout_account != plan->selected[1].payout_account);
    std::reverse(providers.begin(), providers.end());
    providers.push_back(providers.front()); // Alias endpoints collapse to the same key pair.
    BOOST_CHECK(cybou::PrepareStorageAssignment(context, providers) == plan);
    for (unsigned i{0}; i < 100; ++i) {
        auto changed = context;
        changed.finalized_seed[0] = static_cast<unsigned char>(i);
        const auto replay = cybou::PrepareStorageAssignment(changed, providers);
        BOOST_REQUIRE(replay);
        BOOST_CHECK(replay->selected[0].payout_account != replay->selected[1].payout_account);
        BOOST_CHECK(cybou::PrepareStorageAssignment(changed, providers) == replay);
    }
}

BOOST_AUTO_TEST_CASE(assignment_rejects_conflicting_missing_self_and_insufficient_candidates)
{
    auto context = Context();
    auto providers = Providers();
    auto conflict = providers.front(); conflict.payout_account.fill(99);
    providers.push_back(conflict);
    BOOST_CHECK(!cybou::PrepareStorageAssignment(context, providers));
    providers = Providers(); providers[0].payout_account = context.payer;
    BOOST_CHECK(!cybou::PrepareStorageAssignment(context, providers));
    providers = Providers(); providers[0].storage_id.fill(0);
    BOOST_CHECK(!cybou::PrepareStorageAssignment(context, providers));
    providers = Providers(); for (auto& p : providers) p.payout_account.fill(8);
    BOOST_CHECK(!cybou::PrepareStorageAssignment(context, providers));
    context.replicas = 1;
    BOOST_REQUIRE(cybou::PrepareStorageAssignment(context, providers));
    context.replicas = 3;
    BOOST_CHECK(!cybou::PrepareStorageAssignment(context, Providers()));
    context = Context(); context.term_end = context.term_start;
    BOOST_CHECK(!cybou::PrepareStorageAssignment(context, Providers()));
    context = Context(); context.finalized_seed.fill(0);
    BOOST_CHECK(!cybou::PrepareStorageAssignment(context, Providers()));
    providers.resize(cybou::MAX_STORAGE_ASSIGNMENT_CANDIDATES + 1);
    BOOST_CHECK(!cybou::PrepareStorageAssignment(Context(), providers));
}

BOOST_AUTO_TEST_CASE(frozen_assignment_reopens_and_cannot_be_replaced_or_cross_network_reused)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("assignment-owner.cybou");
    const auto plan = cybou::PrepareStorageAssignment(Context(), Providers());
    BOOST_REQUIRE(plan);
    const auto path = fixture.directory / "assignments";
    {
        cybou::PrivateApplicationStore db{identity->GetKeyStore(), path};
        BOOST_REQUIRE(cybou::FreezeStorageAssignment(db, *plan));
        BOOST_CHECK(cybou::FreezeStorageAssignment(db, *plan));
        auto changed = plan->context; changed.finalized_seed[0] ^= 1;
        const auto replacement = cybou::PrepareStorageAssignment(changed, Providers());
        BOOST_REQUIRE(replacement);
        BOOST_CHECK(!cybou::FreezeStorageAssignment(db, *replacement));
        auto forged = *plan; std::swap(forged.selected[0], forged.selected[1]);
        BOOST_CHECK(!cybou::FreezeStorageAssignment(db, forged));
        BOOST_CHECK(cybou::LoadStorageAssignment(db, plan->context) == plan);
    }
    {
        cybou::PrivateApplicationStore db{identity->GetKeyStore(), path};
        BOOST_CHECK(cybou::LoadStorageAssignment(db, plan->context) == plan);
        auto other = plan->context; other.network_binding[0] ^= 1;
        BOOST_CHECK(!cybou::LoadStorageAssignment(db, other));
        other = plan->context; ++other.epoch;
        BOOST_CHECK(!cybou::LoadStorageAssignment(db, other));
        other = plan->context; ++other.term_end;
        BOOST_CHECK(!cybou::LoadStorageAssignment(db, other));
        identity->GetKeyStore().Clear();
        BOOST_CHECK(!cybou::FreezeStorageAssignment(db, *plan));
        BOOST_CHECK(!cybou::LoadStorageAssignment(db, plan->context));
    }
}
BOOST_AUTO_TEST_SUITE_END()
