// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_binding_store.h>
#include <test/cybou_test_helpers.h>

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <filesystem>

BOOST_AUTO_TEST_SUITE(cybou_bootstrap_binding_store_tests)

namespace {

std::filesystem::path UniquePath()
{
    static std::atomic<unsigned> sequence{0};
    return std::filesystem::temp_directory_path() /
        ("cybou-bootstrap-binding-store-" + std::to_string(sequence.fetch_add(1)));
}

std::optional<cybou::BootstrapNetworkBinding> MakeBinding(
    const unsigned char seed, const uint64_t generation, const std::string& name = "CYBOU DEV")
{
    cybou::RecoveryEntropy entropy{};
    entropy[0] = seed;
    const auto key = cybou::DeriveIdentityPublicKey(entropy, cybou::IdentityKeyPurpose::POA_FINALIZER);
    if (!key) return std::nullopt;
    const auto genesis = cybou::CreateDevGenesisState();
    const auto definition = cybou::CreateDevNetworkDefinition(genesis, *key);
    const auto file = cybou::SerializeCybouNetworkFile({definition, genesis});
    if (!file) return std::nullopt;
    return cybou::CreateBootstrapNetworkBinding(generation, name, *file, entropy);
}

} // namespace

BOOST_AUTO_TEST_CASE(trusted_binding_pin_persists_generation_and_rejects_rollback)
{
    const auto path = UniquePath();
    std::filesystem::remove_all(path);
    const auto initial = MakeBinding(0x51, 4);
    const auto next = MakeBinding(0x51, 5);
    const auto rollback = MakeBinding(0x51, 3);
    const auto conflict = MakeBinding(0x51, 4, "DIFFERENT NAME");
    const auto changed_authority = MakeBinding(0x52, 6);
    BOOST_REQUIRE(initial && next && rollback && conflict && changed_authority);

    {
        auto store = cybou::BootstrapBindingStore::TrustInitial(path, *initial);
        BOOST_REQUIRE(store);
        BOOST_CHECK(store->AcceptNext(*initial) == cybou::BootstrapBindingAcceptStatus::UNCHANGED);
        BOOST_CHECK(store->AcceptNext(*rollback) == cybou::BootstrapBindingAcceptStatus::GENERATION_ROLLBACK);
        BOOST_CHECK(store->AcceptNext(*conflict) == cybou::BootstrapBindingAcceptStatus::GENERATION_CONFLICT);
        BOOST_CHECK(store->AcceptNext(*changed_authority) == cybou::BootstrapBindingAcceptStatus::AUTHORITY_CHANGED);
        BOOST_CHECK(store->AcceptNext(*next) == cybou::BootstrapBindingAcceptStatus::ACCEPTED);
        BOOST_CHECK(store->Current().generation == 5);
    }
    {
        auto store = cybou::BootstrapBindingStore::Open(path);
        BOOST_REQUIRE(store);
        BOOST_CHECK(store->Current().generation == 5);
        BOOST_CHECK(store->AcceptNext(*initial) == cybou::BootstrapBindingAcceptStatus::GENERATION_ROLLBACK);
    }
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_CASE(initial_binding_requires_an_explicit_trust_write)
{
    const auto path = UniquePath();
    std::filesystem::remove_all(path);
    BOOST_CHECK(!cybou::BootstrapBindingStore::Open(path));
    const auto binding = MakeBinding(0x53, 1);
    BOOST_REQUIRE(binding);
    auto store = cybou::BootstrapBindingStore::TrustInitial(path, *binding);
    BOOST_REQUIRE(store);
    BOOST_CHECK(!cybou::BootstrapBindingStore::TrustInitial(path, *binding));
    store.reset();
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_SUITE_END()
