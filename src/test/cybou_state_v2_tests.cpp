// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_v2.h>

#include <boost/test/unit_test.hpp>

#include <array>

BOOST_AUTO_TEST_SUITE(cybou_state_v2_tests)

BOOST_AUTO_TEST_CASE(account_create_funds_system_balance_and_roundtrips_state)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, device_seed{};
    root_seed[0] = 1;
    device_seed[0] = 2;
    uint256 raw_account{}, network_id{}, validator_id{}, validator_key{};
    raw_account.begin()[0] = 3;
    network_id.begin()[0] = 4;
    validator_id.begin()[0] = 5;
    validator_key.begin()[0] = 6;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(root && device);
    const IdentityAuthorizationV2 auth{*root, *device};
    const auto commitment = ComputeIdentityAuthorizationCommitmentV2(auth);
    const auto digest = ComputeAccountCreatePopDigestV2(network_id, account, auth);
    BOOST_REQUIRE(commitment && digest);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *digest);
    const auto device_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *digest);
    BOOST_REQUIRE(root_pop && device_pop);
    const AccountCreateOpV2 create{account, auth,
        {.network_id = network_id, .account_id = account, .authorization_commitment = *commitment},
        *root_pop, *device_pop};
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouStateV2 state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(ValidatorV1{validator_id, validator_key, 1});
    auto damaged_create = create;
    damaged_create.device_pop.ed25519[0] ^= 1;
    BOOST_CHECK(ApplyAccountCreateV2(damaged_create, network_id, 0, params, state) == AccountCreateStateErrorV2::INVALID_CREATE);
    BOOST_CHECK(state.accounts.empty());
    BOOST_CHECK(state.identities.Accounts().empty());
    BOOST_CHECK(state.onboarding_pool == params.onboarding_bonus);
    BOOST_CHECK(ApplyAccountCreateV2(create, network_id, 0, params, state) == AccountCreateStateErrorV2::NONE);
    BOOST_CHECK(state.onboarding_pool == 0);
    BOOST_CHECK(state.accounts.at(account).system_balance == params.onboarding_bonus);
    BOOST_CHECK(state.accounts.at(account).balance == 0);
    BOOST_CHECK(state.identities.Find(account) != nullptr);
    const auto bytes = SerializeCybouStateV2(state);
    const auto hash = CybouStateHashV2(state);
    BOOST_REQUIRE(bytes && hash);
    const auto restored = DeserializeCybouStateV2(*bytes);
    BOOST_REQUIRE(restored);
    BOOST_CHECK(SerializeCybouStateV2(*restored) == bytes);
    BOOST_CHECK(CybouStateHashV2(*restored) == hash);

    BOOST_CHECK(ApplyAccountCreateV2(create, network_id, 1, params, state) == AccountCreateStateErrorV2::ACCOUNT_EXISTS);
    BOOST_CHECK(SerializeCybouStateV2(state) == bytes);
    auto damaged = *bytes;
    damaged[0] = 1;
    BOOST_CHECK(!DeserializeCybouStateV2(damaged));
    damaged = *bytes;
    damaged.push_back(0);
    BOOST_CHECK(!DeserializeCybouStateV2(damaged));
    BOOST_CHECK(!DeserializeCybouStateV2(std::span{*bytes}.first(bytes->size() - 1)));
    damaged = *bytes;
    damaged[1 + 8 * 3 + 4] ^= 1; // monetary AccountID no longer matches identity registry
    BOOST_CHECK(!DeserializeCybouStateV2(damaged));
}

BOOST_AUTO_TEST_CASE(insufficient_pool_does_not_register_identity)
{
    using namespace cybou;
    CybouStateV2 state{};
    auto params = DevProtocolParameters();
    params.onboarding_bonus = 1;
    const AccountCreateOpV2 empty{};
    BOOST_CHECK(ApplyAccountCreateV2(empty, uint256{}, 0, params, state) == AccountCreateStateErrorV2::INSUFFICIENT_ONBOARDING_POOL);
    BOOST_CHECK(state.accounts.empty());
    BOOST_CHECK(state.identities.Accounts().empty());
}

BOOST_AUTO_TEST_SUITE_END()
