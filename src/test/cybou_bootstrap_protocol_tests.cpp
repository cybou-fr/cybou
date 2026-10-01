// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_protocol.h>
#include <cybou/bootstrap_binding_store.h>
#include <test/cybou_test_helpers.h>

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <filesystem>

BOOST_AUTO_TEST_SUITE(cybou_bootstrap_protocol_tests)

namespace {
std::filesystem::path UniquePath()
{
    static std::atomic<unsigned> sequence{0};
    return std::filesystem::temp_directory_path() / ("cybou-bootstrap-protocol-" +
        std::to_string(sequence.fetch_add(1)));
}
std::optional<cybou::BootstrapNetworkBinding> Binding(unsigned char seed, uint64_t generation)
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
std::optional<cybou::BootstrapResponse> Exchange(cybou::BootstrapProtocolHandler& handler,
    const cybou::BootstrapRequest& request)
{
    const auto payload = cybou::EncodeBootstrapRequest(request);
    if (!payload) return std::nullopt;
    const auto response = handler.Handle({cybou::p2p::MessageType::BOOTSTRAP_REQUEST, *payload});
    if (!response || response->type != cybou::p2p::MessageType::BOOTSTRAP_RESPONSE) return std::nullopt;
    return cybou::DecodeBootstrapResponse(response->payload);
}
}

BOOST_AUTO_TEST_CASE(status_claim_and_replacement_round_trip)
{
    const auto path = UniquePath();
    std::filesystem::remove_all(path);
    auto provision = cybou::BootstrapStore::Provision(path);
    BOOST_REQUIRE(provision);
    cybou::BootstrapProtocolHandler handler{*provision->store};

    auto status = Exchange(handler, {});
    BOOST_REQUIRE(status);
    BOOST_CHECK(status->status == cybou::BootstrapResponseStatus::EMPTY);
    BOOST_CHECK(!status->binding);

    const auto first = Binding(0x42, 1);
    const auto next = Binding(0x43, 2);
    const auto third = Binding(0x44, 3);
    BOOST_REQUIRE(first && next && third);
    cybou::BootstrapRequest claim;
    claim.kind = cybou::BootstrapRequestKind::CLAIM;
    claim.activation_code = "incorrect";
    claim.binding = first;
    auto invalid = Exchange(handler, claim);
    BOOST_REQUIRE(invalid);
    BOOST_CHECK(invalid->status == cybou::BootstrapResponseStatus::INVALID_ACTIVATION_CODE);
    claim.activation_code = provision->activation_code;
    auto claimed = Exchange(handler, claim);
    BOOST_REQUIRE(claimed);
    BOOST_CHECK(claimed->status == cybou::BootstrapResponseStatus::CLAIMED);
    BOOST_REQUIRE(claimed->binding);
    BOOST_CHECK(claimed->binding->network_id == first->network_id);

    status = Exchange(handler, {});
    BOOST_REQUIRE(status);
    BOOST_CHECK(status->status == cybou::BootstrapResponseStatus::BOUND);

    cybou::RecoveryEntropy old_entropy{};
    old_entropy[0] = 0x42;
    auto replacement = cybou::CreateBootstrapNetworkReplacement(*first, *next, old_entropy);
    BOOST_REQUIRE(replacement);
    cybou::BootstrapRequest replace;
    replace.kind = cybou::BootstrapRequestKind::REPLACE;
    replace.previous_binding = first;
    replace.replacement = replacement;
    const auto encoded = cybou::EncodeBootstrapRequest(replace);
    BOOST_REQUIRE(encoded);
    const auto decoded = DecodeBootstrapRequest(*encoded, first);
    BOOST_REQUIRE(decoded);
    auto replaced = Exchange(handler, *decoded);
    BOOST_REQUIRE(replaced);
    BOOST_CHECK(replaced->status == cybou::BootstrapResponseStatus::REPLACED);
    BOOST_REQUIRE(replaced->binding);
    BOOST_CHECK_EQUAL(replaced->binding->generation, 2);
    BOOST_CHECK(provision->store->ArchivedBinding(1)->network_id == first->network_id);

    cybou::RecoveryEntropy next_entropy{};
    next_entropy[0] = 0x43;
    const auto second_replacement = cybou::CreateBootstrapNetworkReplacement(*next, *third, next_entropy);
    BOOST_REQUIRE(second_replacement);
    BOOST_CHECK(provision->store->ReplaceNetwork(*second_replacement) ==
        cybou::BootstrapReplacementStatus::REPLACED);

    const auto client_path = UniquePath();
    std::filesystem::remove_all(client_path);
    auto client = cybou::BootstrapBindingStore::TrustInitial(client_path, *first);
    BOOST_REQUIRE(client);
    for (const uint64_t generation : {uint64_t{1}, uint64_t{2}}) {
        cybou::BootstrapRequest get_transition;
        get_transition.kind = cybou::BootstrapRequestKind::GET_TRANSITION;
        get_transition.transition_from_generation = generation;
        const auto transition_response = Exchange(handler, get_transition);
        BOOST_REQUIRE(transition_response);
        BOOST_REQUIRE(transition_response->status == cybou::BootstrapResponseStatus::TRANSITION);
        const auto decoded_transition = cybou::DecodeBootstrapNetworkReplacement(
            transition_response->transition, client->Current());
        BOOST_REQUIRE(decoded_transition);
        BOOST_CHECK(client->AcceptReplacement(*decoded_transition) ==
            cybou::BootstrapBindingAcceptStatus::ACCEPTED);
    }
    BOOST_CHECK_EQUAL(client->Current().generation, 3);
    BOOST_CHECK(client->Current().network_id == third->network_id);
    cybou::BootstrapRequest unavailable_transition;
    unavailable_transition.kind = cybou::BootstrapRequestKind::GET_TRANSITION;
    unavailable_transition.transition_from_generation = 3;
    const auto unavailable = Exchange(handler, unavailable_transition);
    BOOST_REQUIRE(unavailable);
    BOOST_CHECK(unavailable->status == cybou::BootstrapResponseStatus::INVALID_REQUEST);
    client.reset();
    std::filesystem::remove_all(client_path);

    auto malformed = *encoded;
    malformed.pop_back();
    BOOST_CHECK(!DecodeBootstrapRequest(malformed, first));
    const auto bad_response = std::vector<unsigned char>{'C','Y','B','S','1',1,0,0,0,0};
    BOOST_CHECK(!cybou::DecodeBootstrapResponse(bad_response));
    provision.reset();
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_CASE(codec_rejects_unbounded_and_inconsistent_messages)
{
    cybou::BootstrapRequest request;
    request.kind = cybou::BootstrapRequestKind::CLAIM;
    request.activation_code = "x";
    request.binding = Binding(0x42, 1);
    BOOST_REQUIRE(request.binding);
    request.activation_code.assign(129, 'x');
    BOOST_CHECK(!cybou::EncodeBootstrapRequest(request));
    const std::vector<unsigned char> unknown{'C','Y','B','Q','1',99};
    BOOST_CHECK(!cybou::DecodeBootstrapRequest(unknown));
}

BOOST_AUTO_TEST_SUITE_END()
