// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state.h>

#include <boost/test/unit_test.hpp>
#include <util/strencodings.h>

#include <limits>
#include <span>

BOOST_AUTO_TEST_SUITE(cybou_state_tests)

namespace {

const uint256 NETWORK_ID{uint256::ONE};
const uint256 BENEFICIARY{uint256::FromUserHex("0a").value()};
const uint256 VOUCHER_ID{uint256::FromUserHex("02").value()};

class FixedVerifier final : public cybou::OperatorAuthoritySignatureVerifier
{
public:
    explicit FixedVerifier(bool result) : m_result{result} {}
    bool Verify(
        const cybou::OperatorAuthorityKeySet&,
        const cybou::SignatureBundleV1&,
        std::span<const unsigned char>) const override
    {
        return m_result;
    }

private:
    bool m_result;
};

const FixedVerifier ACCEPT{true};
const FixedVerifier REJECT{false};

const cybou::OperatorAuthorityKeySet& KeySet()
{
    static const cybou::OperatorAuthorityKeySet keyset{
        .keyset_id = uint256::FromUserHex("04").value(),
        .active_from_epoch = 1,
        .retired_from_epoch = 20,
    };
    return keyset;
}

cybou::InviteVoucher Voucher()
{
    cybou::InviteVoucher voucher{
        .payload{
            .network_id = NETWORK_ID,
            .voucher_id = VOUCHER_ID,
            .beneficiary_account_id = BENEFICIARY,
            .expiry_epoch = 10,
            .organization_id = std::nullopt,
        },
        .signature{},
    };
    voucher.signature.authority_keyset_id = KeySet().keyset_id;
    voucher.signature.classical_signature[0] = 1;
    voucher.signature.pq_signature[0] = 1;
    return voucher;
}

cybou::InviteVoucherValidationContext Context()
{
    return {
        .expected_network_id = NETWORK_ID,
        .redeemer_account_id = BENEFICIARY,
        .current_epoch = 10,
        .authority_keyset = &KeySet(),
    };
}

cybou::InviteRedemptionState State()
{
    cybou::InviteRedemptionState state{
        .onboarding_pool = 10000,
        .accounts{},
        .consumed_voucher_ids{},
    };
    state.accounts.emplace(BENEFICIARY, cybou::AccountBalanceState{.balance = 50, .system_balance = 25});
    return state;
}

} // namespace

BOOST_AUTO_TEST_CASE(redemption_moves_exact_grant_and_consumes_voucher_atomically)
{
    auto state{State()};
    const auto result{cybou::RedeemInviteVoucher(Voucher(), Context(), ACCEPT, state)};

    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL(state.onboarding_pool, 4000);
    BOOST_CHECK_EQUAL(state.accounts.at(BENEFICIARY).balance, 50);
    BOOST_CHECK_EQUAL(state.accounts.at(BENEFICIARY).system_balance, 6025);
    BOOST_CHECK(state.consumed_voucher_ids.contains(VOUCHER_ID));
}

BOOST_AUTO_TEST_CASE(redemption_rejects_replay_without_mutation)
{
    auto state{State()};
    BOOST_REQUIRE(cybou::RedeemInviteVoucher(Voucher(), Context(), ACCEPT, state));
    const auto snapshot{state};

    const auto result{cybou::RedeemInviteVoucher(Voucher(), Context(), ACCEPT, state)};
    BOOST_CHECK(result.error == cybou::InviteRedemptionError::INVALID_VOUCHER);
    BOOST_CHECK(result.voucher_error == cybou::InviteVoucherError::ALREADY_CONSUMED);
    BOOST_CHECK_EQUAL(state.onboarding_pool, snapshot.onboarding_pool);
    BOOST_CHECK_EQUAL(state.accounts.at(BENEFICIARY).system_balance, snapshot.accounts.at(BENEFICIARY).system_balance);
    BOOST_CHECK_EQUAL(state.consumed_voucher_ids.size(), snapshot.consumed_voucher_ids.size());
}

BOOST_AUTO_TEST_CASE(redemption_failures_never_partially_mutate_state)
{
    const auto assert_unchanged = [](const cybou::InviteRedemptionState& state, const cybou::InviteRedemptionState& before) {
        BOOST_CHECK_EQUAL(state.onboarding_pool, before.onboarding_pool);
        BOOST_CHECK(state.accounts == before.accounts);
        BOOST_CHECK(state.consumed_voucher_ids == before.consumed_voucher_ids);
    };

    auto state{State()};
    auto before{state};
    auto result{cybou::RedeemInviteVoucher(Voucher(), Context(), REJECT, state)};
    BOOST_CHECK(result.voucher_error == cybou::InviteVoucherError::INVALID_AUTHORITY_SIGNATURE);
    assert_unchanged(state, before);

    state = State();
    state.accounts.clear();
    before = state;
    result = cybou::RedeemInviteVoucher(Voucher(), Context(), ACCEPT, state);
    BOOST_CHECK(result.error == cybou::InviteRedemptionError::ACCOUNT_NOT_FOUND);
    assert_unchanged(state, before);

    state = State();
    state.onboarding_pool = cybou::WELCOME_GRANT - 1;
    before = state;
    result = cybou::RedeemInviteVoucher(Voucher(), Context(), ACCEPT, state);
    BOOST_CHECK(result.error == cybou::InviteRedemptionError::INSUFFICIENT_ONBOARDING_POOL);
    assert_unchanged(state, before);

    state = State();
    state.accounts.at(BENEFICIARY).system_balance = std::numeric_limits<uint64_t>::max() - cybou::WELCOME_GRANT + 1;
    before = state;
    result = cybou::RedeemInviteVoucher(Voucher(), Context(), ACCEPT, state);
    BOOST_CHECK(result.error == cybou::InviteRedemptionError::SYSTEM_BALANCE_OVERFLOW);
    assert_unchanged(state, before);
}

BOOST_AUTO_TEST_CASE(redemption_state_serialization_is_canonical_and_strict)
{
    const cybou::InviteRedemptionState empty{
        .onboarding_pool = cybou::WELCOME_GRANT,
        .accounts{},
        .consumed_voucher_ids{},
    };
    BOOST_CHECK_EQUAL(
        HexStr(cybou::SerializeInviteRedemptionState(empty)),
        "0170170000000000000000000000000000");

    auto state{State()};
    state.accounts.emplace(uint256::FromUserHex("01").value(), cybou::AccountBalanceState{1, 2});
    state.consumed_voucher_ids.insert(VOUCHER_ID);
    state.consumed_voucher_ids.insert(uint256::FromUserHex("03").value());

    const auto bytes{cybou::SerializeInviteRedemptionState(state)};
    const auto decoded{cybou::DeserializeInviteRedemptionState(bytes)};
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == state);
    BOOST_CHECK(cybou::SerializeInviteRedemptionState(*decoded) == bytes);
    BOOST_CHECK(cybou::InviteRedemptionStateHash(*decoded) == cybou::InviteRedemptionStateHash(state));

    auto malformed{bytes};
    malformed[0] = 2;
    BOOST_CHECK(!cybou::DeserializeInviteRedemptionState(malformed));

    malformed = bytes;
    malformed.pop_back();
    BOOST_CHECK(!cybou::DeserializeInviteRedemptionState(malformed));

    malformed = bytes;
    malformed.push_back(0);
    BOOST_CHECK(!cybou::DeserializeInviteRedemptionState(malformed));
}

BOOST_AUTO_TEST_CASE(redemption_state_hash_is_sensitive_to_every_state_class)
{
    const auto state{State()};
    const auto root{cybou::InviteRedemptionStateHash(state)};

    auto changed{state};
    ++changed.onboarding_pool;
    BOOST_CHECK(cybou::InviteRedemptionStateHash(changed) != root);

    changed = state;
    ++changed.accounts.at(BENEFICIARY).balance;
    BOOST_CHECK(cybou::InviteRedemptionStateHash(changed) != root);

    changed = state;
    ++changed.accounts.at(BENEFICIARY).system_balance;
    BOOST_CHECK(cybou::InviteRedemptionStateHash(changed) != root);

    changed = state;
    changed.consumed_voucher_ids.insert(VOUCHER_ID);
    BOOST_CHECK(cybou::InviteRedemptionStateHash(changed) != root);
}

BOOST_AUTO_TEST_SUITE_END()
