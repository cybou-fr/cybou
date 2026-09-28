// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/mail_service.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <array>

BOOST_FIXTURE_TEST_SUITE(cybou_mail_service_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(protected_mail_matches_frozen_text_wire_profile)
{
    CybouServiceTestFixture fixture;
    auto alice = fixture.CreateIdentity("mail-text-sender.cybou");
    auto bob = fixture.CreateIdentity("mail-text-recipient.cybou");
    const auto alice_id = alice->GetAccountId();
    const auto bob_id = bob->GetAccountId();
    BOOST_REQUIRE(alice_id);
    BOOST_REQUIRE(bob_id);

    cybou::ProtectedMail mail;
    mail.sender = *alice_id;
    mail.recipient = *bob_id;
    mail.timestamp = 0x0102030405060708ULL;
    mail.subject = "Hi";
    mail.body = "Hello \xF0\x9F\x8C\x90";
    const auto bytes = mail.Serialize();
    BOOST_REQUIRE(bytes);
    BOOST_CHECK_EQUAL(bytes->size(), 1 + 32 + 32 + 8 + 2 + 2 + 4 + mail.body.size());
    BOOST_CHECK_EQUAL((*bytes)[73], 2);
    BOOST_CHECK_EQUAL((*bytes)[74], 0);
    BOOST_CHECK(cybou::ProtectedMail::Deserialize(*bytes) == mail);

    auto malformed = *bytes;
    malformed.push_back(0);
    BOOST_CHECK(!cybou::ProtectedMail::Deserialize(malformed));
    malformed = *bytes;
    malformed[75] = 0xc0; // overlong UTF-8 lead byte in subject
    malformed[76] = 0xaf;
    BOOST_CHECK(!cybou::ProtectedMail::Deserialize(malformed));

    mail.subject.assign(cybou::MAX_PROTECTED_MAIL_SUBJECT_BYTES + 1, 's');
    BOOST_CHECK(!mail.Serialize());
    mail.subject.clear();
    mail.body.assign(cybou::MAX_PROTECTED_MAIL_BODY_BYTES + 1, 'b');
    BOOST_CHECK(!mail.Serialize());
}

BOOST_AUTO_TEST_CASE(service_rejects_unknown_recipient_before_submission)
{
    CybouServiceTestFixture fixture;
    auto alice = fixture.CreateIdentity("mail-alice.cybou");
    const auto mailbox = fixture.directory / "mailbox.dat";
    cybou::CybouMailService service{*fixture.runtime, alice->GetKeyStore(), mailbox};
    auto unknown = cybou::AccountId::FromBytes(std::array<unsigned char, 32>{1});
    BOOST_REQUIRE(unknown);
    const auto result = service.SendMail(*unknown, "Subject", "Body");
    BOOST_CHECK(result.error == cybou::SendMailError::RECIPIENT_NOT_FOUND);
    BOOST_CHECK(service.GetMessages(cybou::MailFolder::SENT).empty());
}

BOOST_AUTO_TEST_CASE(service_fails_closed_until_mail_profile_is_enabled)
{
    CybouServiceTestFixture fixture;
    auto alice = fixture.CreateIdentity("mail-sender.cybou");
    auto bob = fixture.CreateIdentity("mail-recipient.cybou");
    cybou::CybouMailService service{*fixture.runtime, alice->GetKeyStore(), fixture.directory / "sender-mailbox.dat"};
    const auto bob_id = bob->GetAccountId();
    BOOST_REQUIRE(bob_id);


    const auto published = fixture.runtime->FindIdentityKemPackage(*bob_id, 0);
    BOOST_REQUIRE(published.status == cybou::IdentityKemPackageLookupStatus::FOUND);
    BOOST_REQUIRE(cybou::DecodeIdentityKemPackage(published.package));

    const auto height = fixture.runtime->GetFinalizedHeight();
    const auto result = service.SendMail(*bob_id, "Subject", "Body");
    BOOST_CHECK(result.error == cybou::SendMailError::CRYPTO_FAILURE);
    BOOST_CHECK_EQUAL(result.error_message, "Protected Mail profile is not enabled; no message was submitted");
    BOOST_CHECK(fixture.runtime->GetFinalizedHeight() == height);
    BOOST_CHECK(service.GetMessages(cybou::MailFolder::SENT).empty());
}

BOOST_AUTO_TEST_CASE(local_draft_survives_mailbox_reopen)
{
    CybouServiceTestFixture fixture;
    auto alice = fixture.CreateIdentity("mail-draft-owner.cybou");
    const auto mailbox = fixture.directory / "draft-mailbox.dat";
    std::filesystem::remove(mailbox);
    uint256 draft_id;
    {
        cybou::CybouMailService service{*fixture.runtime, alice->GetKeyStore(), mailbox};
        draft_id = service.SaveDraft("", "Draft", "Local text");
        BOOST_CHECK_EQUAL(service.GetMessages(cybou::MailFolder::DRAFTS).size(), 1U);
    }
    cybou::CybouMailService reopened{*fixture.runtime, alice->GetKeyStore(), mailbox};
    BOOST_REQUIRE(reopened.LoadMailbox());
    const auto draft = reopened.GetMessage(draft_id);
    BOOST_REQUIRE(draft);
    BOOST_CHECK_EQUAL(draft->body, "Local text");
    BOOST_CHECK(reopened.DeleteMessage(draft_id));
    std::filesystem::remove(mailbox);
}

BOOST_AUTO_TEST_SUITE_END()
