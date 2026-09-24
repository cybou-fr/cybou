// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/account_creation.h>
#include <cybou/identity_crypto.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>

BOOST_AUTO_TEST_SUITE(cybou_account_creation_tests)

BOOST_AUTO_TEST_CASE(hybrid_pop_and_work_bind_random_account_and_authorization)
{
    std::array<unsigned char, 32> root_seed{};
    std::array<unsigned char, 32> device_seed{};
    for (size_t i{0}; i < 32; ++i) {
        root_seed[i] = static_cast<unsigned char>(i);
        device_seed[i] = static_cast<unsigned char>(i + 32);
    }
    uint256 account_bytes{};
    account_bytes.begin()[0] = 0x42;
    uint256 network_id{};
    network_id.begin()[0] = 0x99;
    const cybou::AccountId account_id{account_bytes};
    const auto root = cybou::DeriveIdentityPublicKey(root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = cybou::DeriveIdentityPublicKey(device_seed, cybou::IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(root && device);
    cybou::IdentityAuthorization auth{*root, *device};
    const auto commitment = cybou::ComputeIdentityAuthorizationCommitment(auth);
    const auto digest = cybou::ComputeAccountCreatePopDigest(network_id, account_id, auth);
    BOOST_REQUIRE(commitment && digest);
    const auto root_pop = cybou::SignIdentityMessage(root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT, *digest);
    const auto device_pop = cybou::SignIdentityMessage(device_seed, cybou::IdentityKeyPurpose::DEVICE, *digest);
    BOOST_REQUIRE(root_pop && device_pop);
    cybou::AccountCreateOp op{
        .account_id = account_id,
        .authorization = auth,
        .work = {.network_id = network_id, .account_id = account_id, .authorization_commitment = *commitment,
                 .work_epoch = 0, .nonce = 0},
        .recovery_pop = *root_pop,
        .device_pop = *device_pop,
    };
    auto params = cybou::DevProtocolParameters();
    params.account_creation_work_bits = 4;
    bool mined{false};
    for (uint64_t nonce{0}; nonce < 1000; ++nonce) {
        op.work.nonce = nonce;
        const auto hash = cybou::ComputeAccountCreateWorkHash(op.work);
        BOOST_REQUIRE(hash);
        if ((*hash)[0] == 0 || (*hash)[0] < 16) { mined = true; break; }
    }
    BOOST_REQUIRE(mined);
    BOOST_CHECK(cybou::ValidateAccountCreateOp(op, network_id, 0, params) == cybou::AccountCreateError::NONE);
    const auto bytes = cybou::SerializeAccountCreateOp(op);
    BOOST_REQUIRE(bytes);
    const auto decoded = cybou::DeserializeAccountCreateOp(*bytes);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(cybou::ValidateAccountCreateOp(*decoded, network_id, 0, params) == cybou::AccountCreateError::NONE);
    uint256 other_network{};
    other_network.begin()[0] = 0x98;
    BOOST_CHECK(cybou::ValidateAccountCreateOp(*decoded, other_network, 0, params) == cybou::AccountCreateError::NETWORK_MISMATCH);
    auto damaged = *decoded;
    damaged.recovery_pop.ed25519[0] ^= 1;
    BOOST_CHECK(cybou::ValidateAccountCreateOp(damaged, network_id, 0, params) == cybou::AccountCreateError::INVALID_RECOVERY_POP);
    damaged = *decoded;
    damaged.device_pop.ml_dsa[0] ^= 1;
    BOOST_CHECK(cybou::ValidateAccountCreateOp(damaged, network_id, 0, params) == cybou::AccountCreateError::INVALID_DEVICE_POP);
    damaged = *decoded;
    damaged.work.authorization_commitment[0] ^= 1;
    BOOST_CHECK(cybou::ValidateAccountCreateOp(damaged, network_id, 0, params) == cybou::AccountCreateError::COMMITMENT_MISMATCH);
    damaged = *decoded;
    damaged.work.work_epoch = 1;
    BOOST_CHECK(cybou::ValidateAccountCreateOp(damaged, network_id, 0, params) == cybou::AccountCreateError::FUTURE_WORK_EPOCH);
    auto truncated = std::span{*bytes}.first(bytes->size() - 1);
    BOOST_CHECK(!cybou::DeserializeAccountCreateOp(truncated));
    auto legacy_version = *bytes;
    legacy_version[0] = 1;
    BOOST_CHECK(!cybou::DeserializeAccountCreateOp(legacy_version));
}

BOOST_AUTO_TEST_SUITE_END()
