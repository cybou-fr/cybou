// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/protocol_operation.h>

#include <uint256.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(cybou_protocol_operation_tests)

BOOST_AUTO_TEST_CASE(account_create_has_canonical_typed_roundtrip)
{
    const cybou::AccountId account_id{uint256::FromUserHex("a1").value()};
    const cybou::AccountCreateOpV1 account_create{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = account_id,
        .initial_authorization{.authorization_descriptor = uint256::FromUserHex("b2").value()},
        .creation_work{
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = uint256::FromUserHex("c3").value(),
            .account_id = account_id,
            .initial_authorization_commitment = uint256::FromUserHex("d4").value(),
            .work_epoch = 7,
            .nonce = 9,
        },
    };
    const cybou::ProtocolOperationV1 operation{account_create};

    BOOST_CHECK(cybou::OperationType(operation) == cybou::ProtocolOperationType::ACCOUNT_CREATE);
    const auto encoded{cybou::SerializeProtocolOperation(operation)};
    BOOST_REQUIRE_GE(encoded.size(), 2U);
    BOOST_CHECK_EQUAL(encoded[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL(encoded[1], static_cast<uint8_t>(cybou::ProtocolOperationType::ACCOUNT_CREATE));

    const auto decoded{cybou::DeserializeProtocolOperation(encoded)};
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == operation);
}

BOOST_AUTO_TEST_CASE(unknown_or_malformed_operation_is_rejected)
{
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(std::span<const unsigned char>{}).has_value());

    cybou::ProtocolOperationV1 operation{};
    auto encoded{cybou::SerializeProtocolOperation(operation)};
    encoded[0] = 0xff;
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(encoded).has_value());

    encoded = cybou::SerializeProtocolOperation(operation);
    encoded[1] = 0xff;
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(encoded).has_value());

    encoded = cybou::SerializeProtocolOperation(operation);
    encoded.pop_back();
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(encoded).has_value());
}

BOOST_AUTO_TEST_SUITE_END()
