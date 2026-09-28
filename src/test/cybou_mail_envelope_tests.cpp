// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/mail_envelope.h>
#include <boost/test/unit_test.hpp>

#include <array>

BOOST_AUTO_TEST_SUITE(cybou_mail_envelope_tests)

namespace {
cybou::MailEnvelopeV1 MakeEnvelope()
{
    cybou::MailEnvelopeV1 envelope;
    envelope.mail_id[0] = 1;
    envelope.recipient_state_height = 0x0102030405060708ULL;
    envelope.recipient_state_root[0] = 2;
    envelope.content_nonce[0] = 3;
    envelope.recipient_capsule.key_epoch = 4;
    envelope.recipient_capsule.package_commitment[0] = 5;
    envelope.recipient_capsule.enc[0] = 6;
    envelope.recipient_capsule.wrapped_cek[0] = 7;
    envelope.content_ciphertext.resize(cybou::MAIL_ENVELOPE_CONTENT_TAG_BYTES, 0xA5);
    return envelope;
}
}

BOOST_AUTO_TEST_CASE(mail_envelope_v1_round_trips_one_account_key_epoch_capsule)
{
    const auto envelope = MakeEnvelope();
    const auto bytes = cybou::SerializeMailEnvelopeV1(envelope);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK(cybou::DeserializeMailEnvelopeV1(*bytes, 4) == envelope);
    BOOST_CHECK(!cybou::DeserializeMailEnvelopeV1(*bytes, 5));
}

BOOST_AUTO_TEST_CASE(mail_envelope_v1_rejects_bad_suite_and_framing)
{
    const auto bytes = cybou::SerializeMailEnvelopeV1(MakeEnvelope());
    BOOST_REQUIRE(bytes);
    auto malformed = *bytes;
    malformed[0] = 2;
    BOOST_CHECK(!cybou::DeserializeMailEnvelopeV1(malformed));
    malformed = *bytes;
    malformed[1] ^= 1;
    BOOST_CHECK(!cybou::DeserializeMailEnvelopeV1(malformed));
    malformed = *bytes;
    malformed.push_back(0);
    BOOST_CHECK(!cybou::DeserializeMailEnvelopeV1(malformed));
    malformed = *bytes;
    malformed.pop_back();
    BOOST_CHECK(!cybou::DeserializeMailEnvelopeV1(malformed));
}

BOOST_AUTO_TEST_CASE(mail_envelope_v1_enforces_size)
{
    auto envelope = MakeEnvelope();
    envelope.content_ciphertext.resize(cybou::MAIL_ENVELOPE_MAX_BYTES, 0xCC);
    BOOST_CHECK(!cybou::SerializeMailEnvelopeV1(envelope));
    envelope = MakeEnvelope();
    envelope.recipient_capsule.package_commitment.fill(0);
    BOOST_CHECK(!cybou::SerializeMailEnvelopeV1(envelope));
}

BOOST_AUTO_TEST_SUITE_END()
