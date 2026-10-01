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

std::optional<cybou::BootstrapNetworkBinding> TestBinding()
{
    cybou::RecoveryEntropy entropy{};
    entropy[0] = 0x42;
    const auto key = cybou::DeriveIdentityPublicKey(entropy, cybou::IdentityKeyPurpose::POA_FINALIZER);
    if (!key) return std::nullopt;
    const auto genesis = cybou::CreateDevGenesisState();
    const auto definition = cybou::CreateDevNetworkDefinition(genesis, *key);
    const auto file = cybou::SerializeCybouNetworkFile({definition, genesis});
    if (!file) return std::nullopt;
    return cybou::CreateBootstrapNetworkBinding(1, "CYBOU DEV", *file, entropy);
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

BOOST_AUTO_TEST_SUITE_END()
