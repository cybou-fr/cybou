// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/mail_service.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <array>

BOOST_FIXTURE_TEST_SUITE(cybou_mail_service_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(separate_mail_key_encrypts_and_decrypts_payload)
{
    cybou::CybouKeyStore sender;
    cybou::CybouKeyStore recipient;
    BOOST_REQUIRE(sender.GenerateNew());
    BOOST_REQUIRE(recipient.GenerateNew());
    const auto sender_id = sender.GetAccountId();
    const auto recipient_id = recipient.GetAccountId();
    const auto mail_key = recipient.GetPublicKey();
    BOOST_REQUIRE(sender_id && recipient_id && mail_key);
    cybou::ProtectedMail mail{.sender = *sender_id, .recipient = *recipient_id,
        .timestamp = 1, .subject = "Private", .body = "Hello CYBOU"};
    const auto serialized = mail.Serialize();
    BOOST_CHECK(cybou::ProtectedMail::Deserialize(serialized) == mail);
    uint256 salt = uint256::ONE;
    const auto commitment = cybou::ComputeMailContentCommitment(salt, serialized);
    const auto encrypted = cybou::EncryptMailPayload(*mail_key, *sender_id, *recipient_id, salt, mail);
    BOOST_REQUIRE(encrypted);
    const auto decrypted = cybou::DecryptMailPayload(recipient, *sender_id, *recipient_id, commitment, *encrypted);
    BOOST_REQUIRE(decrypted);
    BOOST_CHECK(decrypted->second == mail);
    BOOST_CHECK(!cybou::DecryptMailPayload(sender, *sender_id, *recipient_id, commitment, *encrypted));
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

BOOST_AUTO_TEST_CASE(service_refuses_unpublished_recipient_mail_key)
{
    CybouServiceTestFixture fixture;
    auto alice = fixture.CreateIdentity("mail-sender.cybou");
    auto bob = fixture.CreateIdentity("mail-recipient.cybou");
    cybou::CybouMailService service{*fixture.runtime, alice->GetKeyStore(), fixture.directory / "sender-mailbox.dat"};
    const auto bob_id = bob->GetAccountId();
    BOOST_REQUIRE(bob_id);
    const auto height = fixture.runtime->GetFinalizedHeight();
    const auto result = service.SendMail(*bob_id, "Subject", "Body");
    BOOST_CHECK(result.error == cybou::SendMailError::CRYPTO_FAILURE);
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
