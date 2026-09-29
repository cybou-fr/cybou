// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/private_application_schema.h>
#include <cybou/canonical_cbor.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <vector>

namespace {

template <typename T>
T Filled(const unsigned char byte)
{
    T result{};
    result.fill(byte);
    return result;
}

cybou::MailMessage SampleMail()
{
    cybou::MailMessage mail;
    mail.message_id = Filled<cybou::PrivateItemId>(1);
    mail.reply_to_message_id = Filled<cybou::PrivateItemId>(2);
    mail.recipient_account_id = *cybou::AccountId::FromBytes(Filled<cybou::PrivateItemId>(3));
    mail.client_timestamp_ms = 1700000000000ULL;
    mail.subject = "Report";
    mail.body = "Encrypted mail body";
    mail.attachments.push_back({
        .attachment_id = Filled<cybou::PrivateItemId>(4),
        .filename = "report.pdf",
        .logical_size = 12345,
        .media_type = std::string{"application/pdf"},
        .root_chunk_id = Filled<cybou::ChunkId>(5),
        .content_key = Filled<cybou::ContentKey>(6),
    });
    return mail;
}

cybou::FilesMutationBatch SampleFiles()
{
    cybou::FileItem item;
    item.item_id = Filled<cybou::PrivateItemId>(7);
    item.parent_id = Filled<cybou::PrivateItemId>(8);
    item.kind = cybou::FileItemKind::FILE;
    item.name = "note.txt";
    item.logical_size = 4;
    item.root_chunk_id = Filled<cybou::ChunkId>(9);
    item.content_key = Filled<cybou::ContentKey>(10);
    cybou::FilesMutationBatch batch;
    batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item.item_id, item});
    batch.mutations.push_back({cybou::FileMutationKind::DELETE_ITEM,
        Filled<cybou::PrivateItemId>(11), std::nullopt});
    return batch;
}

cybou::IdentityRecoveryBridge SampleBridge()
{
    cybou::IdentityRecoveryBridge bridge;
    bridge.account_id = *cybou::AccountId::FromBytes(Filled<cybou::PrivateItemId>(12));
    bridge.next_key_epoch = 3;
    bridge.historical_seeds.push_back({0, Filled<cybou::XWingSeed>(13)});
    bridge.historical_seeds.push_back({1, Filled<cybou::XWingSeed>(14)});
    return bridge;
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_private_application_schema_tests)

BOOST_AUTO_TEST_CASE(all_three_schemas_round_trip_deterministically)
{
    const std::array<cybou::PrivateApplicationDocument, 3> documents{
        cybou::PrivateApplicationDocument{SampleMail()},
        cybou::PrivateApplicationDocument{SampleFiles()},
        cybou::PrivateApplicationDocument{SampleBridge()},
    };
    for (const auto& document : documents) {
        const auto encoded = cybou::EncodePrivateApplicationDocument(document);
        BOOST_REQUIRE(encoded);
        const auto decoded = cybou::DecodePrivateApplicationDocument(*encoded);
        BOOST_REQUIRE(decoded);
        BOOST_CHECK(*decoded == document);
        BOOST_CHECK(cybou::EncodePrivateApplicationDocument(*decoded) == encoded);
    }
}

BOOST_AUTO_TEST_CASE(rejects_noncanonical_version_type_and_unknown_fields)
{
    BOOST_CHECK(!cybou::DecodePrivateApplicationDocument(std::vector<unsigned char>{0x82, 0x18, 0x01, 0x01}));
    const auto encoded = cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{SampleMail()});
    BOOST_REQUIRE(encoded);
    auto root = cybou::DecodeCanonicalCbor(*encoded);
    auto& fields = std::get<cybou::CborValue::Array>(root.value);
    fields[1] = cybou::CborValue::Unsigned(2);
    BOOST_CHECK(!cybou::DecodePrivateApplicationDocument(cybou::EncodeCanonicalCbor(root)));
    fields[1] = cybou::CborValue::Unsigned(1);
    fields[0] = cybou::CborValue::Unsigned(99);
    BOOST_CHECK(!cybou::DecodePrivateApplicationDocument(cybou::EncodeCanonicalCbor(root)));
    fields[0] = cybou::CborValue::Unsigned(1);
    fields.push_back(cybou::CborValue::Null());
    BOOST_CHECK(!cybou::DecodePrivateApplicationDocument(cybou::EncodeCanonicalCbor(root)));
}

BOOST_AUTO_TEST_CASE(rejects_invalid_identifiers_keys_names_and_sizes)
{
    auto mail = SampleMail();
    mail.attachments[0].root_chunk_id = {};
    BOOST_CHECK(!cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{mail}));
    mail = SampleMail();
    mail.attachments[0].content_key = {};
    BOOST_CHECK(!cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{mail}));
    mail = SampleMail();
    mail.attachments[0].filename = "../secret";
    BOOST_CHECK(!cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{mail}));
    mail = SampleMail();
    mail.body.assign(128 * 1024 + 1, 'x');
    BOOST_CHECK(!cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{mail}));

    auto files = SampleFiles();
    files.mutations[0].item->content_key.reset();
    BOOST_CHECK(!cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{files}));
    files = SampleFiles();
    files.mutations[0].item->name = "bad/name";
    BOOST_CHECK(!cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{files}));
    files = SampleFiles();
    files.mutations[0].item->kind = static_cast<cybou::FileItemKind>(9);
    BOOST_CHECK(!cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{files}));

    auto bridge = SampleBridge();
    std::swap(bridge.historical_seeds[0], bridge.historical_seeds[1]);
    BOOST_CHECK(!cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{bridge}));
    bridge = SampleBridge();
    bridge.historical_seeds[0].seed = {};
    BOOST_CHECK(!cybou::EncodePrivateApplicationDocument(cybou::PrivateApplicationDocument{bridge}));

    const auto valid = cybou::EncodePrivateApplicationDocument(
        cybou::PrivateApplicationDocument{SampleMail()});
    BOOST_REQUIRE(valid);
    auto decoded_cbor = cybou::DecodeCanonicalCbor(*valid);
    auto& fields = std::get<cybou::CborValue::Array>(decoded_cbor.value);
    auto& attachments = std::get<cybou::CborValue::Array>(fields[8].value);
    auto& attachment = std::get<cybou::CborValue::Array>(attachments[0].value);
    attachment[5] = cybou::CborValue::Bytes(std::vector<std::uint8_t>(32, 0));
    BOOST_CHECK(!cybou::DecodePrivateApplicationDocument(cybou::EncodeCanonicalCbor(decoded_cbor)));
}

BOOST_AUTO_TEST_SUITE_END()
