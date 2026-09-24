// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/account_creation.h>
#include <cybou/bft.h>
#include <cybou/block.h>
#include <cybou/evidence.h>
#include <cybou/keystore.h>
#include <cybou/mail_service.h>
#include <cybou/node_runtime.h>
#include <cybou/signing.h>
#include <cybou/state.h>
#include <random.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <vector>

namespace cybou_mail_service_tests {

using namespace cybou;

namespace {

CybouState CreateTestGenesis(const uint256& val_pub)
{
    return CybouState{
        .onboarding_pool = 1'000'000,
        .security_reward_pool = 100,
        .pending_fee_pool = 0,
        .accounts = {},
        .validator_set = {
            .version = VALIDATOR_SET_VERSION,
            .validators = {{
                .validator_id = val_pub,
                .consensus_public_key = val_pub,
                .weight = 1,
            }},
        },
    };
}

CybouNetworkDefinition CreateTestNetworkDefinition(const CybouState& genesis)
{
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    return CybouNetworkDefinition{
        .protocol_version = CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = CybouStateHash(genesis),
        .genesis_state_root = CybouStateHash(genesis),
        .protocol_parameters = params,
        .initial_validator_set_commitment = ComputeValidatorSetCommitment(genesis.validator_set),
        .operator_authority = std::nullopt,
    };
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(cybou_mail_service_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(protected_mail_serialization_roundtrip)
{
    ProtectedMail mail;
    mail.version = 1;
    mail.sender = AccountId{uint256::FromUserHex("aa").value()};
    mail.recipient = AccountId{uint256::FromUserHex("bb").value()};
    mail.timestamp = 1774390000;
    mail.subject = "Confidential CYBOU Contract";
    mail.body = "This is a strictly E2E encrypted text message delivered via native BFT consensus.";

    const auto bytes = mail.Serialize();
    BOOST_CHECK_GT(bytes.size(), 81);

    const auto deserialized = ProtectedMail::Deserialize(bytes);
    BOOST_REQUIRE(deserialized.has_value());
    BOOST_CHECK_EQUAL(deserialized->version, mail.version);
    BOOST_CHECK(deserialized->sender == mail.sender);
    BOOST_CHECK(deserialized->recipient == mail.recipient);
    BOOST_CHECK_EQUAL(deserialized->timestamp, mail.timestamp);
    BOOST_CHECK_EQUAL(deserialized->subject, mail.subject);
    BOOST_CHECK_EQUAL(deserialized->body, mail.body);

    // Corrupted input
    BOOST_CHECK(!ProtectedMail::Deserialize(std::span{bytes.data(), 80}));
    auto bad_bytes = bytes;
    bad_bytes[0] = 99; // invalid version
    BOOST_CHECK(!ProtectedMail::Deserialize(bad_bytes));
}

BOOST_AUTO_TEST_CASE(x25519_diffie_hellman_agreement)
{
    std::array<unsigned char, 32> alice_seed{};
    std::fill(alice_seed.begin(), alice_seed.end(), 0x11);
    std::array<unsigned char, 32> bob_seed{};
    std::fill(bob_seed.begin(), bob_seed.end(), 0x22);

    const auto alice_ed_pub = DeriveEd25519PublicKey(alice_seed);
    const auto bob_ed_pub = DeriveEd25519PublicKey(bob_seed);
    BOOST_REQUIRE(alice_ed_pub.has_value());
    BOOST_REQUIRE(bob_ed_pub.has_value());

    const auto alice_x_pub = Ed25519PublicKeyToX25519(*alice_ed_pub);
    const auto bob_x_pub = Ed25519PublicKeyToX25519(*bob_ed_pub);
    BOOST_REQUIRE(alice_x_pub.has_value());
    BOOST_REQUIRE(bob_x_pub.has_value());

    const auto alice_x_priv = Ed25519SeedToX25519PrivateKey(alice_seed);
    const auto bob_x_priv = Ed25519SeedToX25519PrivateKey(bob_seed);
    BOOST_REQUIRE(alice_x_priv.has_value());
    BOOST_REQUIRE(bob_x_priv.has_value());

    const auto secret_alice = X25519DeriveSharedSecret(*alice_x_priv, *bob_x_pub);
    const auto secret_bob = X25519DeriveSharedSecret(*bob_x_priv, *alice_x_pub);
    BOOST_REQUIRE(secret_alice.has_value());
    BOOST_REQUIRE(secret_bob.has_value());

    // Both parties arrive at the identical 32-byte shared secret
    BOOST_CHECK(*secret_alice == *secret_bob);
    BOOST_CHECK(!secret_alice->empty());
}

BOOST_AUTO_TEST_CASE(payload_encrypt_and_decrypt)
{
    CybouKeyStore bob_keystore;
    BOOST_REQUIRE(bob_keystore.GenerateNew());
    const auto bob_pub = bob_keystore.GetPublicKey();
    const auto bob_account = bob_keystore.GetAccountId();
    BOOST_REQUIRE(bob_pub.has_value());
    BOOST_REQUIRE(bob_account.has_value());

    CybouKeyStore alice_keystore;
    BOOST_REQUIRE(alice_keystore.GenerateNew());
    const auto alice_account = alice_keystore.GetAccountId();
    BOOST_REQUIRE(alice_account.has_value());

    ProtectedMail mail;
    mail.sender = *alice_account;
    mail.recipient = *bob_account;
    mail.timestamp = 1774391111;
    mail.subject = "Secret Roadmap";
    mail.body = "CYBOU native email with zero Bitcoin Script bloat.";

    uint256 salt;
    GetRandBytes(salt);
    const auto plain_bytes = mail.Serialize();
    const uint256 commitment = ComputeMailContentCommitment(salt, plain_bytes);

    const auto encrypted = EncryptMailPayload(*bob_pub, *alice_account, *bob_account, salt, mail);
    BOOST_REQUIRE(encrypted.has_value());
    BOOST_CHECK_GT(encrypted->size(), 160);

    // Decrypt with Bob's keystore
    const auto decrypted = DecryptMailPayload(bob_keystore, *alice_account, *bob_account, commitment, *encrypted);
    BOOST_REQUIRE(decrypted.has_value());
    BOOST_CHECK_EQUAL(decrypted->first.GetHex(), salt.GetHex());
    BOOST_CHECK_EQUAL(decrypted->second.subject, mail.subject);
    BOOST_CHECK_EQUAL(decrypted->second.body, mail.body);
    BOOST_CHECK(decrypted->second.sender == *alice_account);
    BOOST_CHECK(decrypted->second.recipient == *bob_account);

    // Decrypt with Alice's keystore should fail (she is not the recipient)
    const auto fail_dec = DecryptMailPayload(alice_keystore, *alice_account, *bob_account, commitment, *encrypted);
    BOOST_CHECK(!fail_dec.has_value());

    // Tampered ciphertext should fail
    auto tampered = *encrypted;
    tampered[tampered.size() - 5] ^= 0xFF;
    BOOST_CHECK(!DecryptMailPayload(bob_keystore, *alice_account, *bob_account, commitment, tampered).has_value());
}

BOOST_AUTO_TEST_CASE(mailbox_persistence_and_sync_lifecycle)
{
    const auto test_dir = m_args.GetDataDirBase() / "test_mail_service";
    std::filesystem::create_directories(test_dir);

    std::array<unsigned char, 32> val_priv{};
    val_priv[0] = 0xAA;
    const auto val_pub = *DeriveEd25519PublicKey(val_priv);

    auto gen_state = CreateTestGenesis(val_pub);

    // Create Alice and Bob on-chain in genesis state
    std::array<unsigned char, 32> alice_seed{};
    std::fill(alice_seed.begin(), alice_seed.end(), 0x33);
    CybouKeyStore alice_keystore;
    BOOST_REQUIRE(alice_keystore.LoadFromSeed(alice_seed));
    const auto alice_id = *alice_keystore.GetAccountId();
    const auto alice_pub = *alice_keystore.GetPublicKey();

    std::array<unsigned char, 32> bob_seed{};
    std::fill(bob_seed.begin(), bob_seed.end(), 0x44);
    CybouKeyStore bob_keystore;
    BOOST_REQUIRE(bob_keystore.LoadFromSeed(bob_seed));
    const auto bob_id = *bob_keystore.GetAccountId();
    const auto bob_pub = *bob_keystore.GetPublicKey();

    gen_state.accounts[alice_id] = AccountState{
        .balance = 500,
        .system_balance = 500, // plenty for fees
        .creation_height = 0,
        .creation_epoch = 0,
        .active_authorization_key = alice_pub,
        .next_nonce = 0,
        .last_mail_epoch = 0,
        .mail_count_in_epoch = 0,
    };

    gen_state.accounts[bob_id] = AccountState{
        .balance = 500,
        .system_balance = 500,
        .creation_height = 0,
        .creation_epoch = 0,
        .active_authorization_key = bob_pub,
        .next_nonce = 0,
        .last_mail_epoch = 0,
        .mail_count_in_epoch = 0,
    };

    auto net_def = CreateTestNetworkDefinition(gen_state);

    NodeRuntimeConfig runtime_config{
        .network_definition = net_def,
        .data_dir = test_dir / "node",
        .validator_private_key = val_priv,
        .memory_only = true,
    };
    CybouNodeRuntime runtime(std::move(runtime_config));
    BOOST_REQUIRE(runtime.InitializeGenesis(gen_state));

    const auto alice_mb_path = test_dir / "alice_mailbox.dat";
    const auto bob_mb_path = test_dir / "bob_mailbox.dat";

    CybouMailService alice_mail(runtime, alice_keystore, alice_mb_path);
    CybouMailService bob_mail(runtime, bob_keystore, bob_mb_path);

    // Initial state: empty mailboxes
    BOOST_CHECK_EQUAL(alice_mail.GetMessages(MailFolder::SENT).size(), 0);
    BOOST_CHECK_EQUAL(bob_mail.GetMessages(MailFolder::INBOX).size(), 0);
    BOOST_CHECK_EQUAL(bob_mail.GetUnreadCount(), 0);

    // Alice drafts a message
    const auto draft_id = alice_mail.SaveDraft(bob_id.Value().GetHex(), "Draft Subject", "Draft Body");
    BOOST_CHECK_EQUAL(alice_mail.GetMessages(MailFolder::DRAFTS).size(), 1);
    BOOST_CHECK(alice_mail.GetMessage(draft_id).has_value());
    alice_mail.DeleteMessage(draft_id);
    BOOST_CHECK_EQUAL(alice_mail.GetMessages(MailFolder::DRAFTS).size(), 0);

    // Alice sends mail to Bob
    const auto send_res = alice_mail.SendMail(bob_id, "Welcome to CYBOU", "Native E2E email test message");
    BOOST_REQUIRE(send_res);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(send_res.error), static_cast<uint8_t>(SendMailError::NONE));
    BOOST_CHECK_GT(send_res.fee, 0);

    // Sent folder now has 1 message with PENDING_FINALITY
    const auto alice_sent = alice_mail.GetMessages(MailFolder::SENT);
    BOOST_REQUIRE_EQUAL(alice_sent.size(), 1);
    BOOST_CHECK_EQUAL(alice_sent[0].mail_id.GetHex(), send_res.mail_id.GetHex());
    BOOST_CHECK(alice_sent[0].finality == MailFinalityStatus::PENDING_FINALITY);

    // Authority produces block containing Alice's MailOp
    const auto produced_block = runtime.ProduceBlock();
    BOOST_REQUIRE(produced_block.has_value());
    BOOST_CHECK_EQUAL(produced_block->block.height, 1);
    BOOST_CHECK_EQUAL(produced_block->block.operations.size(), 1);

    // Bob syncs mailbox from the new block
    const size_t bob_received = bob_mail.SyncMailbox();
    BOOST_CHECK_EQUAL(bob_received, 1);
    BOOST_CHECK_EQUAL(bob_mail.GetUnreadCount(), 1);

    const auto bob_inbox = bob_mail.GetMessages(MailFolder::INBOX);
    BOOST_REQUIRE_EQUAL(bob_inbox.size(), 1);
    BOOST_CHECK_EQUAL(bob_inbox[0].subject, "Welcome to CYBOU");
    BOOST_CHECK_EQUAL(bob_inbox[0].body, "Native E2E email test message");
    BOOST_CHECK(bob_inbox[0].sender == alice_id);
    BOOST_CHECK(bob_inbox[0].recipient == bob_id);
    BOOST_CHECK(bob_inbox[0].finality == MailFinalityStatus::FINAL);
    BOOST_CHECK_EQUAL(bob_inbox[0].block_height, 1);
    BOOST_CHECK(!bob_inbox[0].read);

    // Cryptographic evidence bundle attached and valid
    BOOST_REQUIRE(bob_inbox[0].evidence_bundle.has_value());
    BOOST_CHECK_EQUAL(
        VerifyDisclosedMailContent(
            *bob_inbox[0].evidence_bundle,
            bob_inbox[0].salt,
            ProtectedMail{
                .version = 1,
                .sender = alice_id,
                .recipient = bob_id,
                .timestamp = bob_inbox[0].timestamp,
                .subject = "Welcome to CYBOU",
                .body = "Native E2E email test message",
            }.Serialize()),
        true);

    // Mark as read
    BOOST_CHECK(bob_mail.MarkAsRead(bob_inbox[0].mail_id, true));
    BOOST_CHECK_EQUAL(bob_mail.GetUnreadCount(), 0);

    // Alice syncs mailbox: outgoing message becomes FINAL with evidence bundle
    alice_mail.SyncMailbox();
    const auto alice_sent_updated = alice_mail.GetMessages(MailFolder::SENT);
    BOOST_REQUIRE_EQUAL(alice_sent_updated.size(), 1);
    BOOST_CHECK(alice_sent_updated[0].finality == MailFinalityStatus::FINAL);
    BOOST_CHECK_EQUAL(alice_sent_updated[0].block_height, 1);
    BOOST_REQUIRE(alice_sent_updated[0].evidence_bundle.has_value());

    // Test persistence: reopen Bob's mailbox from disk
    CybouMailService bob_reopened(runtime, bob_keystore, bob_mb_path);
    const auto reloaded_inbox = bob_reopened.GetMessages(MailFolder::INBOX);
    BOOST_REQUIRE_EQUAL(reloaded_inbox.size(), 1);
    BOOST_CHECK_EQUAL(reloaded_inbox[0].subject, "Welcome to CYBOU");
    BOOST_CHECK_EQUAL(reloaded_inbox[0].body, "Native E2E email test message");
    BOOST_CHECK(reloaded_inbox[0].read); // read-state persisted locally!
    BOOST_CHECK_EQUAL(bob_reopened.GetLastScannedHeight(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace cybou_mail_service_tests
