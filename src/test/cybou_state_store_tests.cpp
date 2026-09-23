// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_store.h>

#include <dbwrapper.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <span>

BOOST_FIXTURE_TEST_SUITE(cybou_state_store_tests, BasicTestingSetup)

namespace {

const cybou::AccountId ACCOUNT_ID{uint256::FromUserHex("0a").value()};
const uint256 VOUCHER_ID{uint256::FromUserHex("02").value()};
const std::string STATE_KEY{"cybou/invite-redemption/state/v1"};
const std::string HASH_KEY{"cybou/invite-redemption/hash/v1"};

class AcceptVerifier final : public cybou::OperatorAuthoritySignatureVerifier
{
    bool Verify(
        const cybou::OperatorAuthorityKeySet&,
        const cybou::SignatureBundleV1&,
        std::span<const unsigned char>) const override
    {
        return true;
    }
};

const AcceptVerifier VERIFIER;
const cybou::OperatorAuthorityKeySet KEYSET{
    .keyset_id = uint256::FromUserHex("04").value(),
    .active_from_epoch = 1,
    .retired_from_epoch = 20,
};

cybou::InviteRedemptionState State()
{
    cybou::InviteRedemptionState state{
        .onboarding_pool = 10000,
        .accounts{},
        .consumed_voucher_ids{},
    };
    state.accounts.emplace(ACCOUNT_ID, cybou::AccountBalanceState{50, 6000});
    state.consumed_voucher_ids.insert(VOUCHER_ID);
    return state;
}

CDBWrapper MemoryDb()
{
    return CDBWrapper{{
        .path = "cybou-state-store-test",
        .cache_bytes = 1 << 20,
        .memory_only = true,
        .wipe_data = true,
        .obfuscate = false,
    }};
}

cybou::InviteVoucher NewVoucher(const uint256& voucher_id = uint256::FromUserHex("03").value())
{
    cybou::InviteVoucher voucher{
        .payload{
            .network_id = uint256::ONE,
            .voucher_id = voucher_id,
            .beneficiary_account_id = ACCOUNT_ID,
            .expiry_epoch = 10,
            .organization_id = std::nullopt,
        },
        .signature{},
    };
    voucher.signature.authority_keyset_id = KEYSET.keyset_id;
    voucher.signature.classical_signature[0] = 1;
    voucher.signature.pq_signature[0] = 1;
    return voucher;
}

cybou::InviteVoucherValidationContext Context()
{
    return {
        .expected_network_id = uint256::ONE,
        .redeemer_account_id = ACCOUNT_ID,
        .current_epoch = 10,
        .authority_keyset = &KEYSET,
    };
}

} // namespace

BOOST_AUTO_TEST_CASE(state_store_round_trips_and_overwrites_snapshot)
{
    auto db{MemoryDb()};
    cybou::InviteRedemptionStateStore store{db};
    BOOST_CHECK(store.Load().error == cybou::StateLoadError::NOT_FOUND);

    auto state{State()};
    store.Write(state);
    auto loaded{store.Load()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(*loaded.state == state);

    state.onboarding_pool = 4000;
    state.accounts.at(ACCOUNT_ID).system_balance = 12000;
    store.Write(state);
    loaded = store.Load();
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(*loaded.state == state);
}

BOOST_AUTO_TEST_CASE(state_store_rejects_partial_or_hash_mismatched_snapshots)
{
    auto db{MemoryDb()};
    cybou::InviteRedemptionStateStore store{db};
    const auto state{State()};

    db.Write(STATE_KEY, cybou::SerializeInviteRedemptionState(state));
    BOOST_CHECK(store.Load().error == cybou::StateLoadError::CORRUPT);

    db.Write(HASH_KEY, uint256::ONE);
    BOOST_CHECK(store.Load().error == cybou::StateLoadError::CORRUPT);

    store.Write(state);
    BOOST_REQUIRE(store.Load());
    auto corrupt_bytes{cybou::SerializeInviteRedemptionState(state)};
    corrupt_bytes.back() ^= 1;
    db.Write(STATE_KEY, corrupt_bytes);
    BOOST_CHECK(store.Load().error == cybou::StateLoadError::CORRUPT);
}

BOOST_AUTO_TEST_CASE(redeem_and_write_keeps_memory_and_database_in_sync)
{
    auto db{MemoryDb()};
    cybou::InviteRedemptionStateStore store{db};
    auto state{State()};

    const auto result{store.RedeemAndWrite(NewVoucher(), Context(), VERIFIER, state)};
    BOOST_REQUIRE(result);
    const auto loaded{store.Load()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(*loaded.state == state);

    const auto snapshot{state};
    const auto replay{store.RedeemAndWrite(NewVoucher(), Context(), VERIFIER, state)};
    BOOST_CHECK(replay.voucher_error == cybou::InviteVoucherError::ALREADY_CONSUMED);
    BOOST_CHECK(state == snapshot);
    BOOST_REQUIRE(store.Load());
    BOOST_CHECK(*store.Load().state == snapshot);
}

BOOST_AUTO_TEST_CASE(finalized_redemption_apply_and_rollback_are_tip_ordered_and_atomic)
{
    auto db{MemoryDb()};
    cybou::InviteRedemptionStateStore store{db};
    auto state{State()};
    state.onboarding_pool = 22000;
    store.Write(state);
    const auto initial{state};
    const uint256 block1{uint256::FromUserHex("11").value()};
    const uint256 block2{uint256::FromUserHex("12").value()};

    BOOST_REQUIRE(store.ApplyFinalizedRedemption(block1, uint256{}, NewVoucher(), Context(), VERIFIER, state));
    const auto after_block1{state};
    BOOST_CHECK(store.ApplyFinalizedRedemption(
        block2, uint256::ONE, NewVoucher(uint256::FromUserHex("05").value()), Context(), VERIFIER, state).error ==
        cybou::BlockTransitionError::PARENT_MISMATCH);
    BOOST_REQUIRE(store.ApplyFinalizedRedemption(
        block2, block1, NewVoucher(uint256::FromUserHex("05").value()), Context(), VERIFIER, state));
    BOOST_CHECK_EQUAL(state.onboarding_pool, 10000);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).system_balance, 18000);

    BOOST_CHECK(store.RollbackFinalizedRedemption(block1, state).error == cybou::BlockTransitionError::NOT_CURRENT_TIP);
    BOOST_REQUIRE(store.RollbackFinalizedRedemption(block2, state));
    BOOST_CHECK(state == after_block1);
    BOOST_REQUIRE(store.RollbackFinalizedRedemption(block1, state));
    BOOST_CHECK(state == initial);
    BOOST_REQUIRE(store.Load());
    BOOST_CHECK(*store.Load().state == initial);
}

BOOST_AUTO_TEST_CASE(finalized_redemption_rejects_state_mismatch_and_corrupt_undo)
{
    auto db{MemoryDb()};
    cybou::InviteRedemptionStateStore store{db};
    auto state{State()};
    store.Write(state);
    const uint256 block{uint256::FromUserHex("21").value()};

    auto stale{state};
    --stale.onboarding_pool;
    BOOST_CHECK(store.ApplyFinalizedRedemption(block, uint256{}, NewVoucher(), Context(), VERIFIER, stale).error ==
        cybou::BlockTransitionError::STATE_MISMATCH);
    BOOST_REQUIRE(store.ApplyFinalizedRedemption(block, uint256{}, NewVoucher(), Context(), VERIFIER, state));

    const std::string undo_key{"cybou/invite-redemption/undo/v1/" + block.GetHex()};
    db.Write(undo_key, std::vector<unsigned char>{1, 0});
    const auto snapshot{state};
    BOOST_CHECK(store.RollbackFinalizedRedemption(block, state).error ==
        cybou::BlockTransitionError::MISSING_OR_CORRUPT_UNDO);
    BOOST_CHECK(state == snapshot);
}

BOOST_AUTO_TEST_SUITE_END()
