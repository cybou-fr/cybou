// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/mail_service.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <array>

BOOST_FIXTURE_TEST_SUITE(cybou_mail_service_tests, BasicTestingSetup)

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
    const auto device_id = bob->GetKeyStore().GetDeviceId();
    BOOST_REQUIRE(device_id);
    const auto published = fixture.runtime->FindActiveIdentityKemPackage(*bob_id, *device_id);
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
