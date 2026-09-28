// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/mail_envelope.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_mail_envelope_tests)

namespace {

cybou::MailEnvelopeV1 MakeEnvelope(size_t capsule_count = 1)
{
    cybou::MailEnvelopeV1 envelope;
    envelope.mail_id[0] = 1;
    envelope.recipient_state_height = 0x0102030405060708ULL;
    envelope.recipient_state_root[0] = 2;
    envelope.content_nonce[0] = 3;
    envelope.content_ciphertext.resize(cybou::MAIL_ENVELOPE_CONTENT_TAG_BYTES, 0xA5);
    for (size_t i = 0; i < capsule_count; ++i) {
        cybou::MailRecipientCapsuleV1 capsule;
        capsule.device_key_id.back() = static_cast<unsigned char>(i + 1);
        capsule.activation_nonce = i + 4;
        capsule.package_commitment[0] = static_cast<unsigned char>(i + 5);
        capsule.enc[0] = static_cast<unsigned char>(i + 6);
        capsule.wrapped_cek[0] = static_cast<unsigned char>(i + 7);
        envelope.capsules.push_back(capsule);
    }
    return envelope;
}

} // namespace

BOOST_AUTO_TEST_CASE(mail_envelope_v1_round_trips_canonical_bytes)
{
    const auto envelope = MakeEnvelope(2);
    const auto bytes = cybou::SerializeMailEnvelopeV1(envelope);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK_EQUAL(bytes->size(), 96 + 2 * 1240 + 16);
    BOOST_CHECK_EQUAL((*bytes)[0], 1);
    BOOST_CHECK_EQUAL((*bytes)[1], 0x64);
    BOOST_CHECK_EQUAL((*bytes)[2], 0x7a);
    BOOST_CHECK_EQUAL((*bytes)[3], 0x00);
    BOOST_CHECK_EQUAL((*bytes)[4], 0x01);
    BOOST_CHECK_EQUAL((*bytes)[5], 0x00);
    BOOST_CHECK_EQUAL((*bytes)[6], 0x03);
    BOOST_CHECK_EQUAL((*bytes)[39], 0x08); // height is little-endian
    BOOST_CHECK_EQUAL((*bytes)[46], 0x01);

    const auto decoded = cybou::DeserializeMailEnvelopeV1(*bytes, 2);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(*decoded == envelope);
}

BOOST_AUTO_TEST_CASE(mail_envelope_v1_rejects_bad_suite_version_and_framing)
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
    BOOST_CHECK(!cybou::DeserializeMailEnvelopeV1(*bytes, 2));
}

BOOST_AUTO_TEST_CASE(mail_envelope_v1_rejects_noncanonical_capsule_sets)
{
    auto envelope = MakeEnvelope(2);
    envelope.capsules[1].device_key_id = envelope.capsules[0].device_key_id;
    BOOST_CHECK(!cybou::SerializeMailEnvelopeV1(envelope));

    envelope = MakeEnvelope(2);
    std::swap(envelope.capsules[0], envelope.capsules[1]);
    BOOST_CHECK(!cybou::SerializeMailEnvelopeV1(envelope));

    envelope = MakeEnvelope(1);
    envelope.capsules[0].package_commitment.fill(0);
    BOOST_CHECK(!cybou::SerializeMailEnvelopeV1(envelope));
}

BOOST_AUTO_TEST_CASE(mail_envelope_v1_enforces_size_and_capsule_limits)
{
    auto envelope = MakeEnvelope(cybou::MAIL_ENVELOPE_MAX_CAPSULES);
    envelope.content_ciphertext.resize(
        cybou::MAIL_ENVELOPE_MAX_BYTES - 96 - cybou::MAIL_ENVELOPE_MAX_CAPSULES * 1240,
        0xCC);
    const auto max_bytes = cybou::SerializeMailEnvelopeV1(envelope);
    BOOST_REQUIRE(max_bytes);
    BOOST_CHECK_EQUAL(max_bytes->size(), cybou::MAIL_ENVELOPE_MAX_BYTES);
    BOOST_CHECK(cybou::DeserializeMailEnvelopeV1(*max_bytes, cybou::MAIL_ENVELOPE_MAX_CAPSULES));

    envelope.content_ciphertext.push_back(0);
    BOOST_CHECK(!cybou::SerializeMailEnvelopeV1(envelope));
    envelope = MakeEnvelope(cybou::MAIL_ENVELOPE_MAX_CAPSULES + 1);
    BOOST_CHECK(!cybou::SerializeMailEnvelopeV1(envelope));
    envelope = MakeEnvelope();
    envelope.content_ciphertext.resize(cybou::MAIL_ENVELOPE_CONTENT_TAG_BYTES - 1);
    BOOST_CHECK(!cybou::SerializeMailEnvelopeV1(envelope));
}

BOOST_AUTO_TEST_SUITE_END()
