// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/evidence.h>
#include <cybou/identity_crypto.h>
#include <cybou/mail_service.h>
#include <cybou/network_definition.h>
#include <cybou/signing.h>
#include <cybou/validator.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <string>
#include <vector>

namespace {

struct MockValidator {
    uint256 validator_id;
    std::array<unsigned char, 32> seed{};
    cybou::IdentityHybridPublicKey consensus_pubkey;

    static MockValidator Create(uint8_t index)
    {
        MockValidator node;
        node.seed.fill(0);
        node.seed[0] = index;
        const auto keypair = cybou::GenerateValidatorKeyPair(node.seed).value();
        node.consensus_pubkey = keypair.public_key;
        node.validator_id = cybou::ComputeValidatorId(keypair.public_key);
        return node;
    }

    cybou::BftCommitVote SignCommit(
        const uint256& network_id,
        const uint256& block_id,
        uint64_t height,
        const uint256& val_set_commitment,
        uint32_t round = 0) const
    {
        const uint256 digest = cybou::ComputeBftCommitDigest(network_id, block_id, height, round, val_set_commitment);
        cybou::BftCommitVote vote;
        vote.validator_id = validator_id;
        vote.signature = *cybou::SignValidatorVote(seed, digest);
        return vote;
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(cybou_evidence_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(mail_evidence_bundle_creation_and_verification)
{
    const uint256 network_id{uint256::FromUserHex("42").value()};

    // 1. Validator setup (N=1 Authority mode)
    const auto validator = MockValidator::Create(1);
    cybou::ValidatorSet val_set;
    val_set.validators.push_back(cybou::Validator{
        .validator_id = validator.validator_id,
        .consensus_public_key = validator.consensus_pubkey,
        .weight = 1,
    });
    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(val_set);

    // 2. Sender account and device key setup
    std::array<unsigned char, 32> sender_device_seed{};
    sender_device_seed.fill(0x11);
    const auto sender_device_pubkey = *cybou::DeriveIdentityPublicKey(
        sender_device_seed, cybou::IdentityKeyPurpose::DEVICE);
    const auto sender_device_id = *cybou::ComputeDeviceKeyId(sender_device_pubkey);
    const cybou::AccountId sender_id{uint256::FromUserHex("5001").value()};
    const cybou::AccountId recipient_id{uint256::FromUserHex("5002").value()};

    // 3. Sender creates AuthorizedMail with salted content commitment
    const std::string plaintext = "Confidential French pilot communication";
    const std::span<const unsigned char> plaintext_bytes{
        reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size()};

    const uint256 salt{uint256::FromUserHex("77778888").value()};
    const uint256 content_commitment = cybou::ComputeMailContentCommitment(salt, plaintext_bytes);

    cybou::MailPayload mail_payload{
        .version = cybou::MAIL_TX_VERSION,
        .recipient = recipient_id,
        .discovery_tag = uint256::FromUserHex("dddd").value(),
        .content_commitment = content_commitment,
        .ciphertext = {0xca, 0xfe, 0xba, 0xbe}, // Mock ciphertext
    };

    const auto mail_commitment = *cybou::ComputeMailPayloadCommitment(mail_payload);

    cybou::DeviceAuthorization auth{
        .account_id = sender_id,
        .device_id = sender_device_id,
        .nonce = 0,
        .activation_nonce = 0,
        .kind = cybou::DeviceOperationKind::MAIL,
        .payload_commitment = mail_commitment,
        .signature = {},
    };

    const auto op_digest = *cybou::ComputeDeviceOperationDigest(network_id, auth);
    auth.signature = *cybou::SignIdentityMessage(
        sender_device_seed, cybou::IdentityKeyPurpose::DEVICE,
        std::span<const unsigned char>(op_digest.begin(), op_digest.size()));

    const cybou::AuthorizedMail mail_op{
        .authorization = auth,
        .mail = mail_payload,
    };

    // 4. Proposed Block at height 1
    cybou::ProtocolOperation proto_op{mail_op};
    cybou::CybouBlock block{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = uint256::FromUserHex("1000").value(),
        .height = 1,
        .operations = {proto_op},
        .resulting_state_root = uint256::FromUserHex("aaaa").value(),
    };
    const uint256 block_id = cybou::ComputeBlockId(block);

    // 5. BFT Finality Certificate
    cybou::BftFinalityCertificate cert{
        .version = cybou::BFT_FINALITY_CERTIFICATE_VERSION,
        .network_id = network_id,
        .block_id = block_id,
        .height = 1,
        .round = 0,
        .validator_set_commitment = val_set_commitment,
        .commit_votes = {
            validator.SignCommit(network_id, block_id, 1, val_set_commitment),
        },
    };

    // 6. Create MailEvidenceBundle
    auto bundle_opt = cybou::CreateMailEvidenceBundle(block, 0, cert, sender_device_pubkey, network_id);
    BOOST_REQUIRE(bundle_opt.has_value());
    auto bundle = *bundle_opt;

    // 7. Verify bundle passes complete cryptographic checks
    BOOST_CHECK(cybou::VerifyMailEvidenceBundle(bundle, val_set, network_id) ==
                cybou::EvidenceVerificationError::NONE);

    // 8. Verify voluntary content disclosure with valid content commitment
    BOOST_CHECK(cybou::VerifyDisclosedMailContent(bundle, content_commitment));

    // Wrong content commitment fails disclosure verification
    const uint256 wrong_commitment{uint256::FromUserHex("9999").value()};
    BOOST_CHECK(!cybou::VerifyDisclosedMailContent(bundle, wrong_commitment));

    // 9. Tamper tests:
    // Mismatched network ID
    BOOST_CHECK(cybou::VerifyMailEvidenceBundle(bundle, val_set, uint256::FromUserHex("999").value()) ==
                cybou::EvidenceVerificationError::NETWORK_MISMATCH);

    // Tampered sender signature
    auto bad_sig_bundle = bundle;
    bad_sig_bundle.mail_operation.authorization.signature.ed25519[0] ^= 0xFF;
    BOOST_CHECK(cybou::VerifyMailEvidenceBundle(bad_sig_bundle, val_set, network_id) ==
                cybou::EvidenceVerificationError::INVALID_OPERATION_SIGNATURE);

    // Replaced sender device key
    auto wrong_key_bundle = bundle;
    wrong_key_bundle.sender_device_key.ed25519[0] ^= 0xFF;
    BOOST_CHECK(cybou::VerifyMailEvidenceBundle(wrong_key_bundle, val_set, network_id) ==
                cybou::EvidenceVerificationError::INVALID_OPERATION_SIGNATURE);

    // Tampered inclusion proof operation hash
    auto bad_proof_bundle = bundle;
    bad_proof_bundle.inclusion_proof.operation_hashes[0] = uint256::FromUserHex("bad").value();
    BOOST_CHECK(cybou::VerifyMailEvidenceBundle(bad_proof_bundle, val_set, network_id) ==
                cybou::EvidenceVerificationError::INCLUSION_PROOF_FAILED);

    // Tampered block header operations root
    auto bad_ops_root_bundle = bundle;
    bad_ops_root_bundle.block_header.operations_root = uint256::FromUserHex("bad").value();
    BOOST_CHECK(cybou::VerifyMailEvidenceBundle(bad_ops_root_bundle, val_set, network_id) ==
                cybou::EvidenceVerificationError::INCLUSION_PROOF_FAILED);

    // Tampered block header height (causes block_id mismatch with certificate)
    auto bad_height_bundle = bundle;
    bad_height_bundle.block_header.height = 2;
    BOOST_CHECK(cybou::VerifyMailEvidenceBundle(bad_height_bundle, val_set, network_id) ==
                cybou::EvidenceVerificationError::BLOCK_ID_MISMATCH);

    // Mismatched certificate validator set
    auto bad_cert_bundle = bundle;
    bad_cert_bundle.finality_certificate.validator_set_commitment = uint256::FromUserHex("bad").value();
    BOOST_CHECK(cybou::VerifyMailEvidenceBundle(bad_cert_bundle, val_set, network_id) ==
                cybou::EvidenceVerificationError::CERTIFICATE_VERIFICATION_FAILED);
}

BOOST_AUTO_TEST_CASE(mail_evidence_bundle_serialization_roundtrip)
{
    const uint256 network_id{uint256::FromUserHex("42").value()};
    const auto validator = MockValidator::Create(1);
    cybou::ValidatorSet val_set;
    val_set.validators.push_back(cybou::Validator{
        .validator_id = validator.validator_id,
        .consensus_public_key = validator.consensus_pubkey,
        .weight = 1,
    });
    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(val_set);

    std::array<unsigned char, 32> sender_device_seed{};
    sender_device_seed.fill(0x22);
    const auto sender_device_pubkey = *cybou::DeriveIdentityPublicKey(
        sender_device_seed, cybou::IdentityKeyPurpose::DEVICE);
    const auto sender_device_id = *cybou::ComputeDeviceKeyId(sender_device_pubkey);
    const cybou::AccountId sender_id{uint256::FromUserHex("6001").value()};
    const cybou::AccountId recipient_id{uint256::FromUserHex("6002").value()};

    const std::string plaintext = "Evidence serialization test";
    const std::span<const unsigned char> plaintext_bytes{
        reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size()};
    const uint256 salt{uint256::FromUserHex("1234").value()};
    const uint256 content_commitment = cybou::ComputeMailContentCommitment(salt, plaintext_bytes);

    cybou::MailPayload mail_payload{
        .version = cybou::MAIL_TX_VERSION,
        .recipient = recipient_id,
        .discovery_tag = uint256::FromUserHex("baad").value(),
        .content_commitment = content_commitment,
        .ciphertext = {0x01, 0x02, 0x03},
    };

    const auto mail_commitment = *cybou::ComputeMailPayloadCommitment(mail_payload);

    cybou::DeviceAuthorization auth{
        .account_id = sender_id,
        .device_id = sender_device_id,
        .nonce = 1,
        .activation_nonce = 0,
        .kind = cybou::DeviceOperationKind::MAIL,
        .payload_commitment = mail_commitment,
        .signature = {},
    };

    const auto op_digest = *cybou::ComputeDeviceOperationDigest(network_id, auth);
    auth.signature = *cybou::SignIdentityMessage(
        sender_device_seed, cybou::IdentityKeyPurpose::DEVICE,
        std::span<const unsigned char>(op_digest.begin(), op_digest.size()));

    const cybou::AuthorizedMail mail_op{
        .authorization = auth,
        .mail = mail_payload,
    };

    cybou::CybouBlock block{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = uint256::FromUserHex("1000").value(),
        .height = 5,
        .operations = {cybou::ProtocolOperation{mail_op}},
        .resulting_state_root = uint256::FromUserHex("bbbb").value(),
    };
    const uint256 block_id = cybou::ComputeBlockId(block);

    cybou::BftFinalityCertificate cert{
        .version = cybou::BFT_FINALITY_CERTIFICATE_VERSION,
        .network_id = network_id,
        .block_id = block_id,
        .height = 5,
        .round = 0,
        .validator_set_commitment = val_set_commitment,
        .commit_votes = {
            validator.SignCommit(network_id, block_id, 5, val_set_commitment),
        },
    };

    auto bundle = *cybou::CreateMailEvidenceBundle(block, 0, cert, sender_device_pubkey, network_id);

    // Serialization & Deserialization
    const auto serialized = cybou::SerializeMailEvidenceBundle(bundle);
    BOOST_REQUIRE(serialized.has_value());
    const auto deserialized = cybou::DeserializeMailEvidenceBundle(*serialized);
    BOOST_REQUIRE(deserialized.has_value());
    BOOST_CHECK(*deserialized == bundle);

    // Verified roundtrip bundle passes all verification
    BOOST_CHECK(cybou::VerifyMailEvidenceBundle(*deserialized, val_set, network_id) ==
                cybou::EvidenceVerificationError::NONE);
    BOOST_CHECK(cybou::VerifyDisclosedMailContent(*deserialized, content_commitment));
}

BOOST_AUTO_TEST_SUITE_END()
