// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_store.h>
#include <test/cybou_test_helpers.h>

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <filesystem>
#include <thread>

BOOST_AUTO_TEST_SUITE(cybou_bootstrap_store_tests)

namespace {

std::filesystem::path UniqueStorePath()
{
    static std::atomic<unsigned> sequence{0};
    return std::filesystem::temp_directory_path() /
        ("cybou-bootstrap-store-" + std::to_string(sequence.fetch_add(1)));
}

std::optional<cybou::BootstrapNetworkBinding> TestBinding(
    const unsigned char seed = 0x42, const uint64_t generation = 1)
{
    cybou::RecoveryEntropy entropy{};
    entropy[0] = seed;
    const auto key = cybou::DeriveIdentityPublicKey(entropy, cybou::IdentityKeyPurpose::POA_FINALIZER);
    if (!key) return std::nullopt;
    const auto genesis = cybou::CreateDevGenesisState();
    const auto definition = cybou::CreateDevNetworkDefinition(genesis, *key);
    const auto file = cybou::SerializeCybouNetworkFile({definition, genesis});
    if (!file) return std::nullopt;
    return cybou::CreateBootstrapNetworkBinding(generation, "CYBOU DEV", *file, entropy);
}

} // namespace

BOOST_AUTO_TEST_CASE(provision_claim_and_restart_consume_activation_code_durably)
{
    const auto path = UniqueStorePath();
    std::filesystem::remove_all(path);
    {
        auto provision = cybou::BootstrapStore::Provision(path);
        BOOST_REQUIRE(provision);
        BOOST_REQUIRE(provision->store);
        BOOST_CHECK_EQUAL(provision->activation_code.size(), 64);
        BOOST_CHECK(provision->store->State() == cybou::BootstrapStoreState::EMPTY);
        BOOST_CHECK(!provision->store->CurrentBinding());
        BOOST_CHECK(cybou::BootstrapStore::Provision(path) == std::nullopt);

        const auto binding = TestBinding();
        BOOST_REQUIRE(binding);
        BOOST_CHECK(provision->store->ClaimInitialNetwork("wrong", *binding) ==
            cybou::BootstrapClaimStatus::INVALID_ACTIVATION_CODE);
        BOOST_CHECK(provision->store->ClaimInitialNetwork(provision->activation_code, *binding) ==
            cybou::BootstrapClaimStatus::CLAIMED);
        BOOST_CHECK(provision->store->State() == cybou::BootstrapStoreState::BOUND);
        BOOST_CHECK(provision->store->CurrentBinding()->network_id == binding->network_id);
        BOOST_CHECK(provision->store->ClaimInitialNetwork(provision->activation_code, *binding) ==
            cybou::BootstrapClaimStatus::ALREADY_BOUND);
    }
    {
        auto reopened = cybou::BootstrapStore::Open(path);
        BOOST_REQUIRE(reopened);
        BOOST_CHECK(reopened->State() == cybou::BootstrapStoreState::BOUND);
        const auto binding = reopened->CurrentBinding();
        BOOST_REQUIRE(binding);
        BOOST_CHECK(cybou::VerifyBootstrapNetworkBinding(*binding));
        BOOST_CHECK(reopened->ClaimInitialNetwork("some other activation code", *binding) ==
            cybou::BootstrapClaimStatus::ALREADY_BOUND);
    }
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_CASE(only_one_concurrent_claim_can_bind_store)
{
    const auto path = UniqueStorePath();
    std::filesystem::remove_all(path);
    auto provision = cybou::BootstrapStore::Provision(path);
    BOOST_REQUIRE(provision);
    const auto binding = TestBinding();
    BOOST_REQUIRE(binding);

    cybou::BootstrapClaimStatus first{cybou::BootstrapClaimStatus::STORAGE_ERROR};
    cybou::BootstrapClaimStatus second{cybou::BootstrapClaimStatus::STORAGE_ERROR};
    std::thread one{[&] { first = provision->store->ClaimInitialNetwork(provision->activation_code, *binding); }};
    std::thread two{[&] { second = provision->store->ClaimInitialNetwork(provision->activation_code, *binding); }};
    one.join();
    two.join();
    BOOST_CHECK((first == cybou::BootstrapClaimStatus::CLAIMED && second == cybou::BootstrapClaimStatus::ALREADY_BOUND) ||
        (second == cybou::BootstrapClaimStatus::CLAIMED && first == cybou::BootstrapClaimStatus::ALREADY_BOUND));
    provision.reset();
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_CASE(open_fails_closed_for_missing_or_corrupt_store)
{
    const auto missing = UniqueStorePath();
    BOOST_CHECK(!cybou::BootstrapStore::Open(missing));
    const auto path = UniqueStorePath();
    std::filesystem::remove_all(path);
    auto provision = cybou::BootstrapStore::Provision(path);
    BOOST_REQUIRE(provision);
    provision.reset();
    {
        cybou::KVStore database{cybou::KVStoreOptions{.path = path}};
        database.Write(std::string{"bootstrap/state"}, uint8_t{7}, true);
    }
    BOOST_CHECK(!cybou::BootstrapStore::Open(path));
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_CASE(replacement_requires_old_authority_and_archives_atomically)
{
    const auto path = UniqueStorePath();
    std::filesystem::remove_all(path);
    cybou::RecoveryEntropy old_entropy{};
    old_entropy[0] = 0x42;
    const auto initial = TestBinding(0x42, 1);
    const auto next = TestBinding(0x43, 2);
    const auto third = TestBinding(0x44, 3);
    BOOST_REQUIRE(initial && next && third);

    auto provision = cybou::BootstrapStore::Provision(path);
    BOOST_REQUIRE(provision);
    BOOST_CHECK(provision->store->ClaimInitialNetwork(provision->activation_code, *initial) ==
        cybou::BootstrapClaimStatus::CLAIMED);

    const auto replacement = cybou::CreateBootstrapNetworkReplacement(*initial, *next, old_entropy);
    BOOST_REQUIRE(replacement);
    BOOST_CHECK(cybou::VerifyBootstrapNetworkReplacement(*initial, *replacement));
    const auto encoded = cybou::EncodeBootstrapNetworkReplacement(*initial, *replacement);
    BOOST_REQUIRE(encoded);
    const auto decoded = cybou::DecodeBootstrapNetworkReplacement(*encoded, *initial);
    BOOST_REQUIRE(decoded);

    cybou::RecoveryEntropy wrong_entropy{};
    wrong_entropy[0] = 0x44;
    BOOST_CHECK(!cybou::CreateBootstrapNetworkReplacement(*initial, *next, wrong_entropy));
    auto altered = *replacement;
    altered.new_binding.display_name = "ALTERED";
    BOOST_CHECK(!cybou::VerifyBootstrapNetworkReplacement(*initial, altered));

    BOOST_CHECK(provision->store->ReplaceNetwork(*decoded) == cybou::BootstrapReplacementStatus::REPLACED);
    BOOST_CHECK(provision->store->CurrentBinding()->generation == 2);
    BOOST_CHECK(provision->store->ArchivedBinding(1)->network_id == initial->network_id);
    BOOST_CHECK(provision->store->ReplaceNetwork(*decoded) ==
        cybou::BootstrapReplacementStatus::INVALID_REPLACEMENT);
    cybou::RecoveryEntropy next_entropy{};
    next_entropy[0] = 0x43;
    const auto second_replacement = cybou::CreateBootstrapNetworkReplacement(*next, *third, next_entropy);
    BOOST_REQUIRE(second_replacement);
    BOOST_CHECK(provision->store->ReplaceNetwork(*second_replacement) ==
        cybou::BootstrapReplacementStatus::REPLACED);
    const auto first_transition = provision->store->ArchivedTransition(1);
    const auto second_transition = provision->store->ArchivedTransition(2);
    BOOST_REQUIRE(first_transition && second_transition);
    BOOST_CHECK(first_transition->new_binding.network_id == next->network_id);
    BOOST_CHECK(second_transition->new_binding.network_id == third->network_id);
    const auto archived = provision->store->ArchivedBinding(1);
    BOOST_REQUIRE(archived);
    BOOST_CHECK(archived->network_id == initial->network_id);
    provision.reset();

    auto reopened = cybou::BootstrapStore::Open(path);
    BOOST_REQUIRE(reopened);
    BOOST_CHECK(reopened->CurrentBinding()->generation == 3);
    const auto persisted_archive = reopened->ArchivedBinding(1);
    BOOST_REQUIRE(persisted_archive);
    BOOST_CHECK(persisted_archive->generation == 1);
    const auto persisted_first_transition = reopened->ArchivedTransition(1);
    const auto persisted_second_transition = reopened->ArchivedTransition(2);
    BOOST_REQUIRE(persisted_first_transition && persisted_second_transition);
    BOOST_CHECK(cybou::VerifyBootstrapNetworkReplacement(*persisted_archive, *persisted_first_transition));
    const auto generation_two = reopened->ArchivedBinding(2);
    BOOST_REQUIRE(generation_two);
    BOOST_CHECK(cybou::VerifyBootstrapNetworkReplacement(*generation_two, *persisted_second_transition));
    reopened.reset();
    {
        cybou::KVStore database{cybou::KVStoreOptions{.path = path}};
        std::vector<unsigned char> transition_bytes;
        BOOST_REQUIRE(database.Read(std::string{"bootstrap/transition/1"}, transition_bytes));
        BOOST_REQUIRE(!transition_bytes.empty());
        transition_bytes.front() ^= 0x01;
        database.Write(std::string{"bootstrap/transition/1"}, transition_bytes, true);
    }
    BOOST_CHECK(!cybou::BootstrapStore::Open(path)); // corrupted catch-up history fails closed
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_SUITE_END()
