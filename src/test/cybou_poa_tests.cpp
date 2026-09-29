// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/poa_conflict_detector.h>
#include <cybou/poa_finalizer.h>
#include <cybou/poa_finality.h>
#include <cybou/poa_signing_journal.h>
#include <cybou/crypto/sha256.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>

namespace {

uint256 TestId(const unsigned char first_byte)
{
    uint256 id;
    id.begin()[0] = first_byte;
    return id;
}

cybou::RecoveryEntropy TestEntropy(const unsigned char first_byte)
{
    cybou::RecoveryEntropy entropy{};
    entropy[0] = first_byte;
    entropy[31] = static_cast<unsigned char>(first_byte ^ 0xa5);
    return entropy;
}

cybou::CybouBlock TestBlock(const uint256& parent, const uint64_t height,
    const unsigned char state_tag)
{
    return cybou::CybouBlock{
        .parent_block_id = parent,
        .height = height,
        .operations = {},
        .resulting_state_root = TestId(state_tag),
    };
}

std::unique_ptr<cybou::KVStore> OpenDb(const std::filesystem::path& path, const bool wipe)
{
    return std::make_unique<cybou::KVStore>(cybou::KVStoreOptions{
        .path = path,
        .cache_bytes = 1 << 20,
        .wipe_data = wipe,
    });
}

template <typename Range>
std::string BytesHex(const Range& bytes)
{
    std::ostringstream out;
    for (const auto byte : bytes) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
    return out.str();
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(cybou_poa_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(finality_digest_key_and_encoding_golden_vectors)
{
    cybou::RecoveryEntropy entropy{};
    for (size_t i = 0; i < entropy.size(); ++i) entropy[i] = static_cast<unsigned char>(i);
    const auto key = cybou::DeriveIdentityPublicKey(entropy, cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(key);
    std::array<unsigned char, 32> network_bytes{};
    std::array<unsigned char, 32> parent_bytes{};
    std::array<unsigned char, 32> state_bytes{};
    for (size_t i = 0; i < 32; ++i) {
        network_bytes[i] = static_cast<unsigned char>(i + 1);
        parent_bytes[i] = static_cast<unsigned char>(i + 0x21);
        state_bytes[i] = static_cast<unsigned char>(i + 0x41);
    }
    const uint256 network{network_bytes};
    const uint256 parent{parent_bytes};
    auto vector_block = TestBlock(parent, 7, 0);
    vector_block.resulting_state_root = uint256{state_bytes};
    const auto block_id = cybou::ComputeBlockId(vector_block);
    const auto digest = cybou::ComputePoaFinalityDigest(network, block_id, 7, parent);
    const auto signature = cybou::SignIdentityMessage(entropy, cybou::IdentityKeyPurpose::POA_FINALIZER, digest);
    BOOST_REQUIRE(signature);
    BOOST_CHECK(cybou::VerifyIdentityMessage(*key, *signature, digest));
    BOOST_CHECK_EQUAL(BytesHex(key->ed25519), "8254c6e332edef49152acb98e85b9d566e094aeaa5aeac2bb3670c9929a633d6");
    BOOST_CHECK_EQUAL(BytesHex(signature->ed25519),
        "1a78228567fe880a16481319d354f8a0b1291ee157022b22ceee278495fd15cb183db9dc21d86e2e3fed1f05264454e65fbe333f617c58536fc16f425f8a7409");
    BOOST_CHECK_EQUAL(BytesHex(std::span<const unsigned char>{block_id.begin(), block_id.size()}),
        "64e819bc0ef92a3bc827de365094f94a45f7f085454a8a56f4b581ea9366cde3");
    BOOST_CHECK_EQUAL(BytesHex(std::span<const unsigned char>{digest.begin(), digest.size()}),
        "8e91e9859bf416facf9331b6ed6906a10007244e286e4568b3bc7d744337d6a2");
    const auto key_id = cybou::ComputePoaFinalizerKeyId(*key);
    BOOST_REQUIRE(key_id);
    BOOST_CHECK_EQUAL(BytesHex(*key_id), "4b5cd4996b3b8c560c8a6e2071070795d5cf7f3d26b503c9002e2f214ea5c60c");
    std::array<unsigned char, 32> ml_public_hash{};
    BOOST_REQUIRE(cybou::crypto::ComputeSha256({key->ml_dsa}, ml_public_hash.data()));
    BOOST_CHECK_EQUAL(BytesHex(ml_public_hash), "d4992c7d44ab1ceb9ed7bf307af728d8118374417e2290d61a98c5f3ab8de977");

    cybou::PoaFinalityCertificate certificate{
        .network_id = network,
        .block_id = block_id,
        .height = 7,
        .parent_block_id = parent,
        .signature = {},
    };
    certificate.signature.ed25519.fill(0xa5);
    certificate.signature.ml_dsa.assign(3309, 0x5a);
    const auto encoded = cybou::SerializePoaFinalityCertificate(certificate);
    BOOST_REQUIRE(encoded);
    BOOST_CHECK_EQUAL(encoded->size(), cybou::POA_FINALITY_CERTIFICATE_SIZE);
    BOOST_CHECK((*encoded)[0] == cybou::POA_FINALITY_CERTIFICATE_VERSION);
    BOOST_CHECK(std::all_of(encoded->begin() + 105, encoded->begin() + 169,
        [](const unsigned char byte) { return byte == 0xa5; }));
    BOOST_CHECK(std::all_of(encoded->begin() + 169, encoded->end(),
        [](const unsigned char byte) { return byte == 0x5a; }));
    BOOST_CHECK(cybou::DeserializePoaFinalityCertificate(*encoded) == certificate);
    std::array<unsigned char, 32> certificate_hash{};
    BOOST_REQUIRE(cybou::crypto::ComputeSha256({*encoded}, certificate_hash.data()));
    BOOST_CHECK_EQUAL(BytesHex(certificate_hash), "d189e4967722c7fcd16de274fadbc31d66373b911389cd00b9e19e0d7b81e115");
}

BOOST_AUTO_TEST_CASE(finalizer_retries_same_intent_and_recovers_after_restart)
{
    const auto path = m_args.GetDataDirBase() / "cybou-poa-finalizer-retry";
    std::filesystem::remove_all(path);
    const auto entropy = TestEntropy(11);
    const auto genesis = TestId(21);
    const auto network = TestId(31);
    const auto finalizer_key = cybou::DeriveIdentityPublicKey(entropy,
        cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(finalizer_key);
    const auto block = TestBlock(genesis, 1, 41);

    {
        auto db = OpenDb(path, true);
        cybou::PoaFinalizer finalizer{*db, network, genesis, entropy, *finalizer_key};
        BOOST_CHECK(finalizer.CheckCanonicalTip(0, genesis) == cybou::PoaJournalStatus::NONE);
        const auto first = finalizer.SignFinality(0, genesis, block);
        BOOST_REQUIRE(first.certificate);
        BOOST_CHECK(first.status == cybou::PoaSigningStatus::SIGNED);
        BOOST_CHECK(cybou::VerifyPoaCertificateForBlock(*first.certificate,
            *finalizer_key, network, block));

        const auto retry = finalizer.SignFinality(0, genesis, block);
        BOOST_REQUIRE(retry.certificate);
        BOOST_CHECK(retry.status == cybou::PoaSigningStatus::ALREADY_PREPARED);
        BOOST_CHECK(retry.certificate->network_id == first.certificate->network_id);
        BOOST_CHECK(retry.certificate->block_id == first.certificate->block_id);
        BOOST_CHECK(retry.certificate->height == first.certificate->height);
        BOOST_CHECK(retry.certificate->parent_block_id == first.certificate->parent_block_id);

        auto wrong_network = network;
        wrong_network.begin()[0] ^= 0x80;
        BOOST_CHECK(!cybou::VerifyPoaCertificateForBlock(*first.certificate,
            *finalizer_key, wrong_network, block));
        auto changed_height = block;
        ++changed_height.height;
        BOOST_CHECK(!cybou::VerifyPoaCertificateForBlock(*first.certificate,
            *finalizer_key, network, changed_height));
        auto changed_parent = block;
        changed_parent.parent_block_id = TestId(22);
        BOOST_CHECK(!cybou::VerifyPoaCertificateForBlock(*first.certificate,
            *finalizer_key, network, changed_parent));
        auto changed_state = block;
        changed_state.resulting_state_root = TestId(42);
        BOOST_CHECK(!cybou::VerifyPoaCertificateForBlock(*first.certificate,
            *finalizer_key, network, changed_state));
    }

    {
        auto db = OpenDb(path, false);
        cybou::PoaFinalizer recovered{*db, network, genesis, entropy, *finalizer_key};
        const auto retry = recovered.SignFinality(0, genesis, block);
        BOOST_REQUIRE(retry.certificate);
        BOOST_CHECK(retry.status == cybou::PoaSigningStatus::ALREADY_PREPARED);
        const auto child = TestBlock(cybou::ComputeBlockId(block), 2, 43);
        const auto next = recovered.SignFinality(1, cybou::ComputeBlockId(block), child);
        BOOST_CHECK(next.status == cybou::PoaSigningStatus::SIGNED);
    }

    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_CASE(finalizer_rejects_wrong_recovery_and_halts_on_any_same_height_change)
{
    const auto path = m_args.GetDataDirBase() / "cybou-poa-finalizer-equivocation";
    std::filesystem::remove_all(path);
    const auto entropy = TestEntropy(12);
    const auto wrong_entropy = TestEntropy(13);
    const auto genesis = TestId(23);
    const auto network = TestId(33);
    const auto finalizer_key = cybou::DeriveIdentityPublicKey(entropy,
        cybou::IdentityKeyPurpose::POA_FINALIZER);
    const auto wrong_finalizer_key = cybou::DeriveIdentityPublicKey(wrong_entropy,
        cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(finalizer_key);
    BOOST_REQUIRE(wrong_finalizer_key);

    {
        auto db = OpenDb(path, true);
        BOOST_CHECK_THROW((cybou::PoaFinalizer{*db, network, genesis,
            wrong_entropy, *finalizer_key}), std::invalid_argument);
        BOOST_CHECK_THROW((cybou::PoaFinalizer{*db, network, genesis,
            entropy, *wrong_finalizer_key}), std::invalid_argument);
        cybou::PoaFinalizer finalizer{*db, network, genesis, entropy, *finalizer_key};
        const auto first = TestBlock(genesis, 1, 44);
        BOOST_CHECK(finalizer.SignFinality(0, genesis, first).status == cybou::PoaSigningStatus::SIGNED);
        const auto alternate_block = TestBlock(genesis, 1, 45);
        const auto conflict = finalizer.SignFinality(0, genesis, alternate_block);
        BOOST_CHECK(conflict.status == cybou::PoaSigningStatus::JOURNAL_REJECTED);
        BOOST_CHECK(conflict.journal_status == cybou::PoaJournalStatus::EQUIVOCATION);
        BOOST_CHECK(finalizer.CheckCanonicalTip(1, cybou::ComputeBlockId(first)) ==
            cybou::PoaJournalStatus::JOURNAL_HALTED);
    }

    {
        auto db = OpenDb(path, false);
        BOOST_CHECK_THROW((cybou::PoaFinalizer{*db, network, genesis,
            wrong_entropy, *finalizer_key}), std::invalid_argument);
        cybou::PoaFinalizer recovered{*db, network, genesis, entropy, *finalizer_key};
        BOOST_CHECK(recovered.CheckCanonicalTip(1, TestId(99)) ==
            cybou::PoaJournalStatus::JOURNAL_HALTED);
    }

    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_CASE(finalizer_halts_on_parent_or_canonical_history_mismatch)
{
    const auto base = m_args.GetDataDirBase();
    const auto parent_path = base / "cybou-poa-finalizer-parent-mismatch";
    const auto history_path = base / "cybou-poa-finalizer-history-mismatch";
    std::filesystem::remove_all(parent_path);
    std::filesystem::remove_all(history_path);
    const auto entropy = TestEntropy(15);
    const auto genesis = TestId(26);
    const auto network = TestId(36);
    const auto finalizer_key = cybou::DeriveIdentityPublicKey(entropy,
        cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(finalizer_key);
    const auto block = TestBlock(genesis, 1, 48);

    {
        auto db = OpenDb(parent_path, true);
        cybou::PoaFinalizer finalizer{*db, network, genesis, entropy, *finalizer_key};
        BOOST_CHECK(finalizer.SignFinality(0, genesis, block).status == cybou::PoaSigningStatus::SIGNED);
        const auto wrong_parent = TestBlock(TestId(27), 1, 49);
        const auto result = finalizer.SignFinality(0, genesis, wrong_parent);
        BOOST_CHECK(result.status == cybou::PoaSigningStatus::JOURNAL_REJECTED);
        BOOST_CHECK(result.journal_status == cybou::PoaJournalStatus::EQUIVOCATION);
    }

    {
        auto db = OpenDb(history_path, true);
        cybou::PoaFinalizer finalizer{*db, network, genesis, entropy, *finalizer_key};
        BOOST_CHECK(finalizer.SignFinality(0, genesis, block).status == cybou::PoaSigningStatus::SIGNED);
        BOOST_CHECK(finalizer.CheckCanonicalTip(1, TestId(98)) ==
            cybou::PoaJournalStatus::HISTORY_MISMATCH);
        BOOST_CHECK(finalizer.CheckCanonicalTip(1, cybou::ComputeBlockId(block)) ==
            cybou::PoaJournalStatus::JOURNAL_HALTED);
    }

    std::filesystem::remove_all(parent_path);
    std::filesystem::remove_all(history_path);
}

BOOST_AUTO_TEST_CASE(conflict_detector_persists_observations_and_equivocation_evidence)
{
    const auto base = m_args.GetDataDirBase();
    const auto first_path = base / "cybou-poa-conflict-signer-a";
    const auto second_path = base / "cybou-poa-conflict-signer-b";
    const auto detector_path = base / "cybou-poa-conflict-detector";
    std::filesystem::remove_all(first_path);
    std::filesystem::remove_all(second_path);
    std::filesystem::remove_all(detector_path);

    const auto entropy = TestEntropy(14);
    const auto genesis = TestId(25);
    const auto network = TestId(35);
    const auto finalizer_key = cybou::DeriveIdentityPublicKey(entropy,
        cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(finalizer_key);
    const auto first_block = TestBlock(genesis, 1, 46);
    const auto second_block = TestBlock(genesis, 1, 47);
    cybou::PoaFinalityCertificate first_certificate;
    cybou::PoaFinalityCertificate second_certificate;

    {
        auto first_db = OpenDb(first_path, true);
        auto second_db = OpenDb(second_path, true);
        cybou::PoaFinalizer first_signer{*first_db, network, genesis, entropy, *finalizer_key};
        cybou::PoaFinalizer second_signer{*second_db, network, genesis, entropy, *finalizer_key};
        const auto first = first_signer.SignFinality(0, genesis, first_block);
        const auto second = second_signer.SignFinality(0, genesis, second_block);
        BOOST_REQUIRE(first.certificate);
        BOOST_REQUIRE(second.certificate);
        first_certificate = *first.certificate;
        second_certificate = *second.certificate;
    }

    {
        auto db = OpenDb(detector_path, true);
        cybou::PoaConflictDetector detector{*db, network, *finalizer_key};
        BOOST_CHECK(detector.Observe(first_certificate, first_block) == cybou::PoaConflictStatus::OBSERVED);
        BOOST_CHECK(detector.Observe(first_certificate, first_block) == cybou::PoaConflictStatus::ALREADY_OBSERVED);
    }

    {
        auto db = OpenDb(detector_path, false);
        cybou::PoaConflictDetector recovered{*db, network, *finalizer_key};
        BOOST_CHECK(recovered.Observe(first_certificate, first_block) == cybou::PoaConflictStatus::ALREADY_OBSERVED);
        BOOST_CHECK(recovered.Observe(second_certificate, second_block) == cybou::PoaConflictStatus::SAFETY_CONFLICT);
        BOOST_CHECK(recovered.SafetyHalted());
    }

    {
        auto db = OpenDb(detector_path, false);
        cybou::PoaConflictDetector recovered{*db, network, *finalizer_key};
        BOOST_CHECK(recovered.SafetyHalted());
        BOOST_CHECK(recovered.Observe(first_certificate, first_block) == cybou::PoaConflictStatus::ALREADY_HALTED);
    }

    std::filesystem::remove_all(first_path);
    std::filesystem::remove_all(second_path);
    std::filesystem::remove_all(detector_path);
}

BOOST_AUTO_TEST_SUITE_END()
