// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment.h>
#include <cybou/storage_assignment_evidence.h>
#include <cybou/storage_economy.h>
#include <cybou/binary_codec.h>
#include <cybou/hash256.h>
#include <test/cybou_service_test_fixture.h>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <limits>
#include <thread>

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
BOOST_AUTO_TEST_CASE(assignment_evidence_rejects_replay_overlap_future_and_wrong_period)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("evidence-owner.cybou");
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "intervals"};
    const auto plan = cybou::PrepareStorageAssignment(Context(), Providers());
    BOOST_REQUIRE(plan);
    cybou::StorageAssignmentEvidenceScope scope{*plan, 0, 1000, 100};
    cybou::StorageAssignmentInterval first{10, 1000, 1020}; first.proof_commitment.fill(1);
    using Result = cybou::StorageEvidenceAppendResult;
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, first, 1020) == Result::REJECTED);
    BOOST_REQUIRE(cybou::FreezeStorageAssignment(db, *plan));
    BOOST_REQUIRE(cybou::StorageAssignmentVerifiedSeconds(db, scope, 10));
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(db, scope, 10), 0U);
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, first, 1019) == Result::REJECTED);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(db, scope, first, 1020) == Result::ADDED);
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, first, 1020) == Result::DUPLICATE);
    auto conflicting = first; conflicting.start_utc = 1020; conflicting.end_utc = 1040;
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, conflicting, 1040) == Result::REJECTED);
    conflicting = first; conflicting.start_utc = 1010; conflicting.proof_commitment.fill(2);
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, conflicting, 1040) == Result::REJECTED);
    conflicting = first; conflicting.end_utc = 1101;
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, conflicting, 1200) == Result::REJECTED);
    conflicting = first; conflicting.period = 11;
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, conflicting, 1200) == Result::REJECTED);
    cybou::StorageAssignmentInterval later{11, 1100, 1150}; later.proof_commitment.fill(2);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(db, scope, later, 1150) == Result::ADDED);
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(db, scope, 10), 20U);
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(db, scope, 11), 70U);
    // The other provider has its own service counter; no replication multiplier.
    auto other = scope; other.replica_slot = 1;
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(db, other, 11), 0U);
    other = scope; ++other.term_start_utc;
    BOOST_CHECK(!cybou::StorageAssignmentVerifiedSeconds(db, other, 11));
    other = scope; other.period_seconds = 101;
    BOOST_CHECK(!cybou::StorageAssignmentVerifiedSeconds(db, other, 11));
    other = scope; other.replica_slot = 2;
    BOOST_CHECK(!cybou::StorageAssignmentVerifiedSeconds(db, other, 11));
    BOOST_CHECK(!cybou::StorageAssignmentVerifiedSeconds(db, scope, 9));
    BOOST_CHECK(!cybou::StorageAssignmentVerifiedSeconds(db, scope, 40));
}

BOOST_AUTO_TEST_CASE(assignment_evidence_survives_reopen_and_feeds_cumulative_payout)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("evidence-owner.cybou");
    const auto plan = cybou::PrepareStorageAssignment(Context(), Providers());
    BOOST_REQUIRE(plan);
    cybou::StorageAssignmentEvidenceScope scope{*plan, 0, 1000, 100};
    const auto path = fixture.directory / "durable-intervals";
    {
        cybou::PrivateApplicationStore db{identity->GetKeyStore(), path};
        BOOST_REQUIRE(cybou::FreezeStorageAssignment(db, *plan));
        // Later-period evidence may arrive before an earlier completed interval.
        for (const auto period : {11U, 10U}) {
            const auto start = 1000 + (period - 10) * 100;
            cybou::StorageAssignmentInterval interval{period, start, start + 100};
            interval.proof_commitment.fill(static_cast<unsigned char>(period));
            BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(db, scope, interval, 1200) ==
                cybou::StorageEvidenceAppendResult::ADDED);
        }
    }
    {
        cybou::PrivateApplicationStore db{identity->GetKeyStore(), path};
        const auto seconds = cybou::StorageAssignmentVerifiedSeconds(db, scope, 11);
        BOOST_REQUIRE(seconds);
        BOOST_CHECK_EQUAL(*seconds, 200U);
        const auto budget = cybou::ComputeAssignedStorageBudget(1, 2, 30, 100, 5);
        BOOST_REQUIRE(budget);
        BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(*budget, *seconds, 0), 0U);
        cybou::StorageAssignmentInterval duplicate{10, 1000, 1100}; duplicate.proof_commitment.fill(10);
        BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, duplicate, 1200) ==
            cybou::StorageEvidenceAppendResult::DUPLICATE);
        identity->GetKeyStore().Clear();
        BOOST_CHECK(!cybou::StorageAssignmentVerifiedSeconds(db, scope, 11));
        BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, duplicate, 1200) ==
            cybou::StorageEvidenceAppendResult::REJECTED);
    }
}

BOOST_AUTO_TEST_CASE(assignment_evidence_corruption_and_time_overflow_fail_closed)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("evidence-owner.cybou");
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "corrupt-intervals"};
    const auto plan = cybou::PrepareStorageAssignment(Context(), Providers());
    BOOST_REQUIRE(plan);
    BOOST_REQUIRE(cybou::FreezeStorageAssignment(db, *plan));
    cybou::StorageAssignmentEvidenceScope scope{*plan, 0, 1000, 100};
    cybou::StorageAssignmentInterval interval{10, 1000, 1100}; interval.proof_commitment.fill(10);
    auto invalid = scope; invalid.period_seconds = std::numeric_limits<std::uint64_t>::max();
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, invalid, interval, 1100) ==
        cybou::StorageEvidenceAppendResult::REJECTED);
    const auto key = "storage/assignment-evidence/" +
        cybou::Hash256{std::span<const unsigned char, 32>{plan->commitment}}.GetHex() + "/0";
    BOOST_REQUIRE(db.Put(key, std::vector<unsigned char>{1, 2, 3}));
    BOOST_CHECK(!cybou::StorageAssignmentVerifiedSeconds(db, scope, 10));
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, interval, 1100) ==
        cybou::StorageEvidenceAppendResult::REJECTED);
    BOOST_CHECK(db.Get(key) == std::optional<std::vector<unsigned char>>({{1, 2, 3}}));
}

BOOST_AUTO_TEST_CASE(assignment_evidence_bound_and_concurrent_duplicate_are_enforced)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("evidence-owner.cybou");
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "bounded-intervals"};
    const auto plan = cybou::PrepareStorageAssignment(Context(), Providers());
    BOOST_REQUIRE(plan);
    BOOST_REQUIRE(cybou::FreezeStorageAssignment(db, *plan));
    cybou::StorageAssignmentEvidenceScope scope{*plan, 0, 1000, 200};
    cybou::StorageAssignmentInterval interval{10, 1000, 1001}; interval.proof_commitment.fill(1);
    std::array<cybou::StorageEvidenceAppendResult, 2> result;
    {
        std::jthread first{[&] { result[0] = cybou::AppendStorageAssignmentEvidence(db, scope, interval, 1001); }};
        std::jthread second{[&] { result[1] = cybou::AppendStorageAssignmentEvidence(db, scope, interval, 1001); }};
    }
    using Result = cybou::StorageEvidenceAppendResult;
    BOOST_CHECK((result[0] == Result::ADDED && result[1] == Result::DUPLICATE) ||
        (result[1] == Result::ADDED && result[0] == Result::DUPLICATE));
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(db, scope, 10), 1U);

    // Construct a full valid bounded journal without thousands of synchronous
    // writes. Loading still validates every interval and proof uniqueness.
    cybou::BinaryWriter out;
    out.Fixed(plan->commitment); out.U8(0); out.U64(1000); out.U64(200);
    out.U32(cybou::MAX_STORAGE_ASSIGNMENT_INTERVALS);
    for (std::uint32_t i{0}; i < cybou::MAX_STORAGE_ASSIGNMENT_INTERVALS; ++i) {
        out.U64(10 + i / 200); out.U64(1000 + i); out.U64(1001 + i);
        cybou::StorageAssignmentId proof{};
        for (unsigned byte{0}; byte < 4; ++byte) proof[byte] = static_cast<unsigned char>((i + 1) >> (byte * 8));
        out.Fixed(proof);
    }
    const auto key = "storage/assignment-evidence/" +
        cybou::Hash256{std::span<const unsigned char, 32>{plan->commitment}}.GetHex() + "/0";
    const auto full = out.Take();
    BOOST_REQUIRE(db.Put(key, full));
    BOOST_REQUIRE(cybou::StorageAssignmentVerifiedSeconds(db, scope, 39));
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(db, scope, 39), 4096U);
    interval = {30, 5096, 5097}; interval.proof_commitment.fill(99);
    BOOST_CHECK(cybou::AppendStorageAssignmentEvidence(db, scope, interval, 5097) == Result::REJECTED);
    BOOST_CHECK(db.Get(key) == full);
}
BOOST_AUTO_TEST_SUITE_END()
