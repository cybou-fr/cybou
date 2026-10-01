// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/hex.h>
#include <cybou/keystore.h>
#include <cybou/p2p/peer_manager.h>
#include <cybou/p2p/peer_admission.h>
#include <cybou/p2p/inbound_server.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_test_identity_helpers.h>
#include <test/cybou_test_setup.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

struct TestTlsIdentity {
    std::filesystem::path certificate;
    std::filesystem::path private_key;
    std::array<unsigned char, 32> pin{};
};

std::optional<TestTlsIdentity> CreateTestTlsIdentity(const std::filesystem::path& directory)
{
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> key_context{
        EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free};
    if (!key_context || EVP_PKEY_keygen_init(key_context.get()) <= 0 ||
        EVP_PKEY_CTX_set_group_name(key_context.get(), "prime256v1") <= 0) return std::nullopt;
    EVP_PKEY* raw_key{nullptr};
    if (EVP_PKEY_generate(key_context.get(), &raw_key) <= 0) return std::nullopt;
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key{raw_key, EVP_PKEY_free};
    std::unique_ptr<X509, decltype(&X509_free)> certificate{X509_new(), X509_free};
    if (!certificate || X509_set_version(certificate.get(), 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 7) != 1 ||
        !X509_gmtime_adj(X509_getm_notBefore(certificate.get()), 0) ||
        !X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 24 * 60 * 60) ||
        X509_set_pubkey(certificate.get(), key.get()) != 1) return std::nullopt;
    X509_NAME* subject = X509_get_subject_name(certificate.get());
    constexpr unsigned char common_name[]{'C','Y','B','O','U',' ','T','E','S','T'};
    if (!subject || X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, common_name,
            sizeof(common_name), -1, 0) != 1 || X509_set_issuer_name(certificate.get(), subject) != 1 ||
        X509_sign(certificate.get(), key.get(), EVP_sha256()) <= 0) return std::nullopt;

    TestTlsIdentity result{directory / "bootstrap-test-cert.pem", directory / "bootstrap-test-key.pem"};
    const auto cert_path = result.certificate.string();
    const auto key_path = result.private_key.string();
    std::unique_ptr<BIO, decltype(&BIO_free)> cert_bio{BIO_new_file(cert_path.c_str(), "wb"), BIO_free};
    std::unique_ptr<BIO, decltype(&BIO_free)> key_bio{BIO_new_file(key_path.c_str(), "wb"), BIO_free};
    if (!cert_bio || !key_bio || PEM_write_bio_X509(cert_bio.get(), certificate.get()) != 1 ||
        PEM_write_bio_PrivateKey(key_bio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1 ||
        BIO_flush(cert_bio.get()) != 1 || BIO_flush(key_bio.get()) != 1) {
        return std::nullopt;
    }
    const auto pin = cybou::p2p::TlsCertificateSpkiSha256(result.certificate);
    if (!pin) return std::nullopt;
    result.pin = *pin;
    return result;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(cybou_p2p_peer_manager_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(public_peer_policy_rejects_before_opening_socket)
{
    CybouServiceTestFixture fixture;
    auto no_policy_config = cybou::NodeRuntimeConfig{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "missing-admission-policy",
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime no_policy_runtime{std::move(no_policy_config)};
    BOOST_REQUIRE(no_policy_runtime.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(!no_policy_runtime.AdmitPeerAddress("127.0.0.1"));
    cybou::p2p::PeerManager no_policy_peers{no_policy_runtime};
    BOOST_CHECK(!no_policy_peers.Connect("127.0.0.1", 1));
    BOOST_CHECK(no_policy_peers.LastConnectStatus() == cybou::p2p::PeerConnectStatus::ADMISSION_REJECTED);

    auto config = cybou::NodeRuntimeConfig{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "public-admission-observer",
        .memory_only = true,
        .wipe_data = true,
        .peer_admission_policy = std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(
            cybou::p2p::PeerAdmissionPolicy::Public(nullptr)),
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    cybou::p2p::PeerManager peers{runtime};
    BOOST_CHECK(!peers.Connect("127.0.0.1", 1));
    BOOST_CHECK(peers.LastConnectStatus() == cybou::p2p::PeerConnectStatus::ADMISSION_REJECTED);
    BOOST_CHECK_EQUAL(peers.ConnectedCount(), 0);
}

BOOST_AUTO_TEST_CASE(inbound_listener_closes_routes_when_policy_is_missing)
{
    CybouServiceTestFixture fixture;
    cybou::CybouNodeRuntime runtime{{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "inbound-missing-admission-policy",
        .memory_only = true,
        .wipe_data = true,
    }};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    cybou::p2p::InboundPeerServer server{runtime, io, tcp::endpoint{tcp::v4(), 0}};
    std::atomic_bool stopping{false};
    std::jthread listener{[&] { server.Run(stopping); }};

    tcp::socket client{io};
    client.connect(tcp::endpoint{boost::asio::ip::address_v4::loopback(), server.Port()});
    client.non_blocking(true);
    std::array<unsigned char, 1> byte{};
    boost::system::error_code read_error;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (std::chrono::steady_clock::now() < deadline) {
        client.read_some(boost::asio::buffer(byte), read_error);
        if (read_error != boost::asio::error::would_block && read_error != boost::asio::error::try_again) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    stopping = true;
    listener.join();
    BOOST_CHECK(read_error == boost::asio::error::eof || read_error == boost::asio::error::connection_reset);
}

BOOST_AUTO_TEST_CASE(removed_storage_wire_ids_are_rejected)
{
    for (uint8_t type = 21; type <= 28; ++type) {
        BOOST_CHECK(!cybou::p2p::EncodeFrame({static_cast<cybou::p2p::MessageType>(type), {}}));
        const std::array<unsigned char, 10> encoded{
            'C', 'Y', 'P', '2', cybou::p2p::WIRE_VERSION, type, 0, 0, 0, 0};
        BOOST_CHECK(!cybou::p2p::DecodeFrame(encoded));
    }
}

BOOST_AUTO_TEST_CASE(stable_tls_identity_accepts_matching_spki_pin_and_rejects_mismatch)
{
    CybouServiceTestFixture fixture;
    const auto identity = CreateTestTlsIdentity(fixture.directory);
    BOOST_REQUIRE(identity);
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    auto run_session = [&](const std::array<unsigned char, 32>& expected_pin) {
        tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
        bool server_handshake{false};
        std::jthread server{[&] {
            tcp::socket socket{io};
            acceptor.accept(socket);
            cybou::p2p::TlsSessionConfig tls;
            tls.certificate_chain_file = identity->certificate;
            tls.private_key_file = identity->private_key;
            cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER,
                std::move(tls)};
            server_handshake = session.ServeBootstrapRequest([](const cybou::p2p::Frame& request) ->
                std::optional<cybou::p2p::Frame> {
                return cybou::p2p::Frame{cybou::p2p::MessageType::BOOTSTRAP_RESPONSE, request.payload};
            });
        }};
        tcp::socket socket{io};
        socket.connect(tcp::endpoint{loopback, acceptor.local_endpoint().port()});
        cybou::p2p::TlsSessionConfig tls;
        tls.expected_server_spki_sha256 = expected_pin;
        cybou::p2p::PeerSession client{std::move(socket), cybou::p2p::TransportRole::CLIENT,
            std::move(tls)};
        const auto response = client.RequestBootstrap({cybou::p2p::MessageType::BOOTSTRAP_REQUEST, {1, 2, 3}});
        server.join();
        return std::pair{response && response->payload == std::vector<unsigned char>{1, 2, 3}, server_handshake};
    };

    const auto accepted = run_session(identity->pin);
    BOOST_CHECK(accepted.first);
    BOOST_CHECK(accepted.second);

    auto wrong_pin = identity->pin;
    wrong_pin[0] ^= 1;
    const auto rejected = run_session(wrong_pin);
    BOOST_CHECK(!rejected.first);
    std::filesystem::remove(identity->certificate);
    std::filesystem::remove(identity->private_key);
}

BOOST_AUTO_TEST_CASE(bootstrap_capability_requires_session_bound_authorized_identity_proof)
{
    CybouServiceTestFixture fixture;
    std::array<unsigned char, 32> client_seed{};
    std::array<unsigned char, 32> server_seed{};
    client_seed[0] = 0x61;
    server_seed[0] = 0x62;
    uint256 client_raw{}, server_raw{};
    client_raw.begin()[0] = 0x63;
    server_raw.begin()[0] = 0x64;
    const cybou::AccountId client_account{client_raw}, server_account{server_raw};
    const auto client_key = cybou::DeriveIdentityPublicKey(client_seed, cybou::IdentityKeyPurpose::AUTHORIZATION);
    const auto server_key = cybou::DeriveIdentityPublicKey(server_seed, cybou::IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(client_key && server_key);
    const auto signer = [](const std::array<unsigned char, 32>& seed) {
        return [seed](std::span<const unsigned char> message) -> std::optional<std::vector<unsigned char>> {
            const auto signature = cybou::SignIdentityMessage(seed, cybou::IdentityKeyPurpose::AUTHORIZATION, message);
            if (!signature) return std::nullopt;
            std::vector<unsigned char> bytes(signature->ed25519.begin(), signature->ed25519.end());
            bytes.insert(bytes.end(), signature->ml_dsa.begin(), signature->ml_dsa.end());
            return bytes;
        };
    };
    const cybou::p2p::BootstrapProofIdentity client_identity{client_account, signer(client_seed)};
    const cybou::p2p::BootstrapProofIdentity server_identity{server_account, signer(server_seed)};
    const auto network = fixture.runtime->GetNetworkId();
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    tcp::acceptor acceptor{io, {boost::asio::ip::address_v4::loopback(), 0}};
    std::optional<cybou::AccountId> server_verified, client_verified;
    bool server_ok{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const cybou::p2p::BootstrapIdentityResolver resolver = [&](const cybou::AccountId& account) {
            return account == client_account ? client_key : std::nullopt;
        };
        server_ok = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = cybou::p2p::CAP_BOOTSTRAP,
            .nonce = 1701}, {}, {}, nullptr, &server_identity, resolver);
        if (server_ok) server_verified = session.PeerBootstrapAccountId();
    }};
    tcp::socket socket{io};
    socket.connect({boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()});
    cybou::p2p::PeerSession client{std::move(socket), cybou::p2p::TransportRole::CLIENT};
    const cybou::p2p::BootstrapIdentityResolver resolver = [&](const cybou::AccountId& account) {
        return account == server_account ? server_key : std::nullopt;
    };
    const bool client_ok = client.Handshake({.network_id = network, .finalized_height = 0,
        .finalized_tip = fixture.definition.genesis_block_id, .capabilities = cybou::p2p::CAP_BOOTSTRAP,
        .nonce = 1702}, {}, {}, nullptr, &client_identity, resolver);
    if (client_ok) client_verified = client.PeerBootstrapAccountId();
    server.join();
    BOOST_REQUIRE(server_ok && client_ok);
    BOOST_CHECK(server_verified == client_account);
    BOOST_CHECK(client_verified == server_account);

    const auto message = cybou::p2p::BootstrapProofMessage(
        {.network_id = network, .nonce = 1}, {.network_id = network, .nonce = 2},
        std::span<const unsigned char>{}, client_account);
    BOOST_CHECK(message.empty()); // exporter is mandatory; a session nonce is not a substitute.
}

BOOST_AUTO_TEST_CASE(pinned_initial_locator_requires_tls_bound_recovery_identity_claim)
{
    CybouServiceTestFixture fixture;
    const auto tls_identity = CreateTestTlsIdentity(fixture.directory);
    BOOST_REQUIRE(tls_identity);
    std::array<unsigned char, 32> recovery_seed{};
    recovery_seed[0] = 0x71;
    const auto recovery_key = cybou::DeriveIdentityPublicKey(recovery_seed,
        cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(recovery_key);
    uint256 raw_account{};
    raw_account.begin()[0] = 0x72;
    const cybou::AccountId account_id{raw_account};
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    tcp::acceptor acceptor{io, {boost::asio::ip::address_v4::loopback(), 0}};
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::TlsSessionConfig config;
        config.certificate_chain_file = tls_identity->certificate;
        config.private_key_file = tls_identity->private_key;
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER, config};
        const auto signer = [&](std::span<const unsigned char> message) -> std::optional<std::vector<unsigned char>> {
            const auto signature = cybou::SignIdentityMessage(recovery_seed,
                cybou::IdentityKeyPurpose::RECOVERY_ROOT, message);
            if (!signature) return std::nullopt;
            std::vector<unsigned char> bytes(signature->ed25519.begin(), signature->ed25519.end());
            bytes.insert(bytes.end(), signature->ml_dsa.begin(), signature->ml_dsa.end());
            return bytes;
        };
        served = session.ServeBootstrapIdentityClaim(account_id, *recovery_key, signer);
    }};
    tcp::socket socket{io};
    socket.connect({boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()});
    cybou::p2p::TlsSessionConfig config;
    config.expected_server_spki_sha256 = tls_identity->pin;
    cybou::p2p::PeerSession client{std::move(socket), cybou::p2p::TransportRole::CLIENT, config};
    auto claim = client.RequestBootstrapIdentityClaim();
    server.join();
    BOOST_REQUIRE(served && claim);
    BOOST_CHECK(claim->account_id == account_id);
    BOOST_CHECK(claim->recovery_key == *recovery_key);
    BOOST_CHECK(std::ranges::any_of(claim->challenge, [](unsigned char byte) { return byte != 0; }));
    BOOST_CHECK(cybou::ComputeRecoveryKeyId(claim->recovery_key));
    const auto encoded = cybou::EncodeBootstrapIdentityClaim(*claim);
    BOOST_REQUIRE(encoded);
    const auto decoded = cybou::DecodeBootstrapIdentityClaim(*encoded);
    BOOST_CHECK(decoded == claim);
    std::filesystem::remove(tls_identity->certificate);
    std::filesystem::remove(tls_identity->private_key);
}

BOOST_AUTO_TEST_CASE(manager_tracks_two_live_peers_and_drops_closed_sockets)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor first_acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor second_acceptor{io, tcp::endpoint{loopback, 0}};
    std::array<bool, 2> served{false, false};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread first_server{[&] {
        tcp::socket socket{io};
        first_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served[0] = session.Handshake({.network_id = network, .finalized_height = 12, .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 101}) &&
            session.AnswerPing();
    }};
    std::jthread second_server{[&] {
        tcp::socket socket{io};
        second_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served[1] = session.Handshake({.network_id = network, .finalized_height = 13, .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 102}) &&
            session.AnswerPing();
    }};

    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto first_port = first_acceptor.local_endpoint().port();
    const auto second_port = second_acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, first_port));
    BOOST_REQUIRE(manager.Connect(address, second_port));
    BOOST_CHECK(!manager.Connect(address, first_port));
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 2U);
    const auto peers = manager.Peers();
    BOOST_REQUIRE_EQUAL(peers.size(), 2U);
    BOOST_CHECK(peers[0].hello.network_id == network);
    BOOST_CHECK(manager.AuthenticatedFinalizerSessions().empty());
    BOOST_CHECK_EQUAL(manager.PingAll(), 2U);
    first_server.join();
    second_server.join();
    BOOST_CHECK(served[0] && served[1]);
    BOOST_CHECK_EQUAL(manager.PingAll(), 0U);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(manager_tracks_finalizer_proof_only_for_the_live_session)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto network = fixture.runtime->GetNetworkId();
    bool handshake_ok{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        handshake_ok = fixture.HandshakeAsFinalizer(session, {.network_id = network,
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 901});
    }};

    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_REQUIRE(manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    const auto live_routes = manager.AuthenticatedFinalizerSessions();
    BOOST_REQUIRE_EQUAL(live_routes.size(), 1U);
    BOOST_CHECK(live_routes.front().finalizer_authenticated);
    BOOST_CHECK(live_routes.front().hello.capabilities & cybou::p2p::CAP_ACCEPT_OPERATIONS);

    server.join();
    BOOST_CHECK(handshake_ok);
    BOOST_CHECK_EQUAL(manager.PingAll(), 0U);
    BOOST_CHECK(manager.AuthenticatedFinalizerSessions().empty());
}

BOOST_AUTO_TEST_CASE(configured_authority_bootstrap_session_proves_genesis_finalizer_key)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto endpoint = std::make_pair(loopback.to_string(), acceptor.local_endpoint().port());
    cybou::CybouNodeRuntime authority{{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "configured-authority-bootstrap-proof",
        .poa_finalizer_recovery_entropy = fixture.validator_seed,
        .p2p_endpoint = endpoint,
        .memory_only = true,
        .wipe_data = true,
        .peer_admission_policy = TestLabAdmissionPolicy(),
    }};
    BOOST_REQUIRE(authority.InitializeGenesis(fixture.genesis));

    bool server_handshake{false};
    bool finalizer_authenticated{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        server_handshake = session.Handshake({.network_id = authority.GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_OPERATION_RELAY, .nonce = 9912}, {}, {},
            &fixture.definition.poa_finalizer_public_key);
        finalizer_authenticated = session.PeerFinalizerAuthenticated();
    }};

    cybou::p2p::PeerManager manager{authority};
    BOOST_CHECK(manager.Connect(endpoint.first, endpoint.second));
    server.join();
    BOOST_CHECK(server_handshake);
    BOOST_CHECK(finalizer_authenticated);
}

BOOST_AUTO_TEST_CASE(authenticated_bootstrap_relays_client_operation_to_live_finalizer)
{
    CybouServiceTestFixture fixture;
    cybou::CybouKeyStore bootstrap_keys;
    cybou::CybouKeyStore client_keys;
    BOOST_REQUIRE(bootstrap_keys.GenerateNew());
    BOOST_REQUIRE(client_keys.GenerateNew());

    const auto bootstrap_account = bootstrap_keys.GetAccountId();
    const auto bootstrap_recovery = bootstrap_keys.GetRecoveryPublicKey();
    BOOST_REQUIRE(bootstrap_account && bootstrap_recovery);
    const auto bootstrap_recovery_id = cybou::ComputeRecoveryKeyId(*bootstrap_recovery);
    BOOST_REQUIRE(bootstrap_recovery_id);

    auto genesis = cybou::CreateDevGenesisState();
    genesis.genesis_bootstrap_grants.emplace(*bootstrap_account,
        cybou::GenesisBootstrapGrant{.recovery_key_id = *bootstrap_recovery_id});
    auto definition = cybou::CreateDevNetworkDefinition(genesis,
        cybou::TestPoaFinalizerPublicKey(fixture.validator_seed[0]));
    definition.protocol_parameters.account_creation_work_bits = 0;
    const auto network_id = cybou::NetworkId(definition);

    const auto make_account_create = [&](const cybou::CybouKeyStore& keys) {
        const auto account = keys.GetAccountId();
        const auto recovery = keys.GetRecoveryPublicKey();
        const auto authorization = keys.GetAuthorizationPublicKey();
        if (!account || !recovery || !authorization) return std::optional<cybou::AccountCreateOp>{};
        const cybou::IdentityAuthorization auth{*recovery, *authorization};
        const auto binding = cybou::test::MakeIdentityKemBinding(network_id, *account, auth);
        const auto recovery_pop = keys.SignRecovery(binding.pop_digest);
        const auto authorization_pop = keys.SignAuthorization(binding.pop_digest);
        if (!recovery_pop || !authorization_pop) return std::optional<cybou::AccountCreateOp>{};
        return std::optional<cybou::AccountCreateOp>{cybou::AccountCreateOp{*account, auth,
            binding.package,
            {.network_id = network_id, .account_id = *account,
                .authorization_commitment = binding.authorization_commitment},
            *recovery_pop, *authorization_pop}};
    };
    const auto bootstrap_create = make_account_create(bootstrap_keys);
    const auto client_create = make_account_create(client_keys);
    BOOST_REQUIRE(bootstrap_create && client_create);
    const cybou::ProtocolOperation bootstrap_operation{*bootstrap_create};
    const cybou::ProtocolOperation client_operation{*client_create};
    const auto client_operation_id = cybou::ComputeOperationId(client_operation);
    const auto client_operation_bytes = cybou::SerializeProtocolOperation(client_operation);
    BOOST_REQUIRE(client_operation_id && client_operation_bytes);

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor probe{io, tcp::endpoint{loopback, 0}};
    const auto endpoint = std::make_pair(loopback.to_string(), probe.local_endpoint().port());
    boost::system::error_code probe_close_error;
    probe.close(probe_close_error);

    cybou::CybouNodeRuntime authority{{
        .network_definition = definition,
        .data_dir = fixture.directory / "relay-authority",
        .poa_finalizer_recovery_entropy = fixture.validator_seed,
        .p2p_endpoint = endpoint,
        .memory_only = true,
        .wipe_data = true,
        .peer_admission_policy = TestLabAdmissionPolicy(),
    }};
    BOOST_REQUIRE(authority.InitializeGenesis(genesis));
    BOOST_REQUIRE(authority.SubmitOperation(bootstrap_operation));
    const auto bootstrap_claim_block = authority.ProduceBlock();
    BOOST_REQUIRE(bootstrap_claim_block);

    const auto bootstrap_signer = [&bootstrap_keys](std::span<const unsigned char> message)
        -> std::optional<std::vector<unsigned char>> {
        const auto signature = bootstrap_keys.SignAuthorization(message);
        if (!signature || signature->ml_dsa.size() != 2420) return std::nullopt;
        std::vector<unsigned char> proof(signature->ed25519.begin(), signature->ed25519.end());
        proof.insert(proof.end(), signature->ml_dsa.begin(), signature->ml_dsa.end());
        return proof;
    };
    cybou::CybouNodeRuntime bootstrap{{
        .network_definition = definition,
        .data_dir = fixture.directory / "relay-bootstrap",
        .memory_only = true,
        .wipe_data = true,
        .bootstrap_identity_account = *bootstrap_account,
        .bootstrap_proof_signer = bootstrap_signer,
        .peer_admission_policy = TestLabAdmissionPolicy(),
    }};
    BOOST_REQUIRE(bootstrap.InitializeGenesis(genesis));
    BOOST_REQUIRE(static_cast<bool>(bootstrap.CommitBlock(*bootstrap_claim_block)));
    BOOST_REQUIRE(bootstrap.LocalBootstrapAccountId());

    cybou::p2p::InboundPeerServer server{bootstrap, io, tcp::endpoint{loopback, endpoint.second}};
    BOOST_REQUIRE_NE(server.Port(), 0U);
    std::atomic_bool stopping{false};
    std::jthread listener{[&] { server.Run(stopping); }};
    struct StopListener {
        std::atomic_bool& stopping;
        std::jthread& listener;
        ~StopListener() {
            stopping = true;
            if (listener.joinable()) listener.join();
        }
    } stop_listener{stopping, listener};
    const auto server_endpoint = std::make_pair(std::string{"127.0.0.1"}, server.Port());
    BOOST_REQUIRE_EQUAL(server_endpoint.second, endpoint.second);

    cybou::p2p::PeerManager authority_peers{authority};
    const bool authority_connected = authority_peers.Connect(server_endpoint.first, server_endpoint.second);
    BOOST_REQUIRE_MESSAGE(authority_connected,
        "finalizer bootstrap connection failed: endpoint=" << endpoint.first << ':' << endpoint.second <<
            " initialized=" << authority.GetStatus().is_initialized << " status=" <<
            static_cast<unsigned>(authority_peers.LastConnectStatus()));
    BOOST_REQUIRE_EQUAL(authority_peers.AuthenticatedBootstrapSessions().size(), 1U);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!bootstrap.CanAcceptOperations() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    BOOST_REQUIRE(bootstrap.CanAcceptOperations());

    cybou::CybouNodeRuntime client{{
        .network_definition = definition,
        .data_dir = fixture.directory / "relay-client",
        .p2p_endpoint = endpoint,
        .memory_only = true,
        .wipe_data = true,
        .peer_admission_policy = TestLabAdmissionPolicy(),
    }};
    BOOST_REQUIRE(client.InitializeGenesis(genesis));
    BOOST_REQUIRE(static_cast<bool>(client.CommitBlock(*bootstrap_claim_block)));
    cybou::p2p::PeerManager client_peers{client};
    BOOST_REQUIRE(client_peers.Connect(server_endpoint.first, server_endpoint.second));
    BOOST_REQUIRE_EQUAL(client_peers.AuthenticatedBootstrapSessions().size(), 1U);

    const auto submitted = client_peers.SubmitOperationToAny({server_endpoint}, client_operation);
    BOOST_REQUIRE(submitted.acknowledgment);
    BOOST_CHECK(submitted.acknowledgment->status == cybou::OperationSubmitStatus::RELAY_QUEUED);
    BOOST_CHECK(submitted.acknowledgment->op_id == *client_operation_id);
    BOOST_REQUIRE_EQUAL(authority_peers.AuthenticatedBootstrapSessions().size(), 1U);

    BOOST_CHECK_EQUAL(authority_peers.PollBootstrapRelays(), 1U);
    BOOST_REQUIRE(authority.IsPoaFinalizerEnabled());
    BOOST_REQUIRE(authority.GetOperationStatus(*client_operation_id).kind ==
        cybou::OperationStatusKind::LOCAL_PENDING);
    const auto finalized = authority.ProduceBlock();
    BOOST_REQUIRE_MESSAGE(finalized, "height=" << authority.GetStatus().finalized_height <<
        " poa_halted=" << authority.GetStatus().poa_safety_halted <<
        " signer=" << authority.IsPoaFinalizerEnabled() <<
        " operation_status=" << static_cast<unsigned>(authority.GetOperationStatus(*client_operation_id).kind));
    const bool included = std::any_of(finalized->block.operations.begin(), finalized->block.operations.end(),
        [&](const cybou::ProtocolOperation& op) { return cybou::ComputeOperationId(op) == client_operation_id; });
    BOOST_CHECK(included);
    BOOST_CHECK(authority.GetOperationStatus(*client_operation_id).kind == cybou::OperationStatusKind::FINALIZED);

    client_peers.DisconnectAll();
    authority_peers.DisconnectAll();
}

BOOST_AUTO_TEST_CASE(submit_operation_rejects_unproven_bootstrap_relay_capability)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("bootstrap-relay-submission.cybou");
    (void)identity;
    const auto source = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source);
    const auto operation = source->block.operations.front();
    const auto operation_bytes = cybou::SerializeProtocolOperation(operation);
    const auto operation_id = cybou::ComputeOperationId(operation);
    BOOST_REQUIRE(operation_bytes && operation_id);

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool handshake_ok{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        handshake_ok = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
                .finalized_height = 1, .finalized_tip = cybou::ComputeBlockId(source->block),
                .capabilities = cybou::p2p::CAP_OPERATION_RELAY, .nonce = 9913});
    }};

    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto result = manager.SubmitOperationToAny(
        {{loopback.to_string(), acceptor.local_endpoint().port()}}, operation);
    server.join();
    BOOST_CHECK(handshake_ok);
    BOOST_CHECK(!result.acknowledgment);
    BOOST_CHECK(!result.delivery_uncertain);
}

BOOST_AUTO_TEST_CASE(unproven_relay_capability_does_not_receive_inbound_operation)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("bootstrap-relay-inbound.cybou");
    (void)identity;
    const auto source = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source);
    const auto operation = source->block.operations.front();

    cybou::CybouNodeRuntime receiver{{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "relay-inbound-observer",
        .memory_only = true,
        .wipe_data = true,
        .peer_admission_policy = TestLabAdmissionPolicy(),
    }};
    BOOST_REQUIRE(receiver.InitializeGenesis(fixture.genesis));

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served = session.Handshake({.network_id = receiver.GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_OPERATION_RELAY, .nonce = 9914});
    }};

    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto result = manager.SubmitOperationToAny(
        {{loopback.to_string(), acceptor.local_endpoint().port()}}, operation);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK(!result.acknowledgment);
    BOOST_CHECK(!result.delivery_uncertain);
}

BOOST_AUTO_TEST_CASE(manager_pings_only_the_requested_peer_budget_and_rotates)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor first_acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor second_acceptor{io, tcp::endpoint{loopback, 0}};
    std::array<bool, 2> served{false, false};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread first_server{[&] {
        tcp::socket socket{io};
        first_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served[0] = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 1801}) &&
            session.AnswerPing();
    }};
    std::jthread second_server{[&] {
        tcp::socket socket{io};
        second_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served[1] = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 1802}) &&
            session.AnswerPing();
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    BOOST_REQUIRE(manager.Connect(address, first_acceptor.local_endpoint().port()));
    BOOST_REQUIRE(manager.Connect(address, second_acceptor.local_endpoint().port()));
    BOOST_CHECK_EQUAL(manager.PingSome(1), 1U);
    BOOST_CHECK_EQUAL(manager.PingSome(1), 1U);
    first_server.join();
    second_server.join();
    BOOST_CHECK(served[0] && served[1]);
}

BOOST_AUTO_TEST_CASE(manager_refuses_wrong_network_peer)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto wrong_network = cybou::ParseUint256UserHex("02");
    BOOST_REQUIRE(wrong_network);
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        session.Handshake({.network_id = *wrong_network, .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 103});
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::WRONG_NETWORK);
    server.join();
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(proof_request_is_bound_to_the_discovered_provider_id)
{
    CybouServiceTestFixture fixture;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    boost::asio::io_context io;
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    auto make_provider = [&](const std::string& name) {
        cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
            .data_dir = fixture.directory / name, .memory_only = true, .wipe_data = true,
            .storage_enabled = true, .storage_capacity_bytes = 64ULL << 20, .peer_admission_policy = TestLabAdmissionPolicy()};
        auto runtime = std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
        if (!runtime->InitializeGenesis(fixture.genesis)) throw std::runtime_error{"provider genesis failed"};
        return runtime;
    };
    auto first = make_provider("proof-provider-1");
    auto second = make_provider("proof-provider-2");
    const auto first_id = first->LocalProviderId();
    const auto second_id = second->LocalProviderId();
    BOOST_REQUIRE(first_id && second_id);
    BOOST_REQUIRE(*first_id != *second_id);
    const auto network = fixture.runtime->GetNetworkId();
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    std::atomic_bool first_handshake{false};
    std::jthread first_server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        first_handshake = session.Handshake({.network_id = network,
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_STORAGE | cybou::p2p::CAP_STORAGE_PROOFS,
            .nonce = 0x3101}, [provider = first.get()](auto message) {
                return provider->SignProviderProof(message);
            });
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_REQUIRE(manager.Connect(address, port));
    first_server.join();
    BOOST_REQUIRE(first_handshake.load());
    const auto discovered = manager.StoragePeers();
    BOOST_REQUIRE_EQUAL(discovered.size(), 1U);
    BOOST_CHECK(discovered.front().provider_id == first_id);
    manager.DisconnectAll();

    std::atomic_bool second_handshake{false};
    std::atomic_bool request_served{false};
    std::jthread second_server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        second_handshake = session.Handshake({.network_id = network,
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_STORAGE | cybou::p2p::CAP_STORAGE_PROOFS,
            .nonce = 0x3102}, [provider = second.get()](auto message) {
                return provider->SignProviderProof(message);
            });
        if (second_handshake) request_served = session.ServeNext(*second);
    }};
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto current = manager.StoragePeers();
    BOOST_REQUIRE_EQUAL(current.size(), 1U);
    BOOST_CHECK(current.front().provider_id == second_id);
    cybou::ChunkId chunk{};
    chunk[0] = 1;
    const auto operation = uint256::ONE;
    BOOST_CHECK(!manager.GetChunkAuthorizationProof(address, port, *first_id, operation, chunk));
    manager.DisconnectAll();
    second_server.join();
    BOOST_CHECK(second_handshake.load());
    BOOST_CHECK(!request_served.load());
}

BOOST_AUTO_TEST_CASE(explicit_validator_peer_evicts_discovered_peer_at_capacity)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    const auto network = fixture.runtime->GetNetworkId();
    std::vector<std::unique_ptr<tcp::acceptor>> acceptors;
    std::vector<std::unique_ptr<tcp::acceptor>> replacement_acceptors;
    std::vector<std::jthread> replacement_servers;
    std::vector<uint16_t> ports;
    std::array<std::atomic_bool, cybou::p2p::MAX_OUTBOUND_PEERS> handshakes{};
    for (size_t i = 0; i < handshakes.size(); ++i) {
        acceptors.push_back(std::make_unique<tcp::acceptor>(io, tcp::endpoint{loopback, 0}));
        ports.push_back(acceptors.back()->local_endpoint().port());
        auto* listener = acceptors.back().get();
        replacement_servers.emplace_back([&, i, listener] {
            tcp::socket socket{io};
            listener->accept(socket);
            cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
            handshakes[i] = session.Handshake({.network_id = network,
                .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
                .capabilities = 0, .nonce = 200 + i});
        });
    }
    replacement_acceptors.push_back(std::make_unique<tcp::acceptor>(io, tcp::endpoint{loopback, 0}));
    const auto replacement_port = replacement_acceptors.back()->local_endpoint().port();

    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    for (size_t i = 0; i < cybou::p2p::MAX_OUTBOUND_PEERS; ++i) {
        BOOST_REQUIRE(manager.Connect(address, ports[i]));
    }
    BOOST_CHECK(!manager.Connect(address, ports.back())); // discovered peers cannot displace connections
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), cybou::p2p::MAX_OUTBOUND_PEERS);
    manager.SetExplicitEndpoints({{address, replacement_port}});
    replacement_servers.emplace_back([&] {
        tcp::socket socket{io};
        replacement_acceptors.back()->accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        handshakes.back() = session.Handshake({.network_id = network,
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = 0, .nonce = 300});
    });
    BOOST_REQUIRE(manager.Connect(address, replacement_port)); // explicit validator replaces a discovered peer
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), cybou::p2p::MAX_OUTBOUND_PEERS);
    const auto peers = manager.Peers();
    BOOST_REQUIRE_EQUAL(peers.size(), cybou::p2p::MAX_OUTBOUND_PEERS);
    BOOST_CHECK(std::any_of(peers.begin(), peers.end(), [&](const auto& peer) {
        return peer.address == address && peer.port == replacement_port;
    }));
    manager.DisconnectAll();
    for (auto& server : replacement_servers) server.join();
    BOOST_CHECK(std::all_of(handshakes.begin(), handshakes.end(), [](const auto& ok) { return ok.load(); }));
}

BOOST_AUTO_TEST_CASE(manager_refuses_hello_tip_conflicting_with_known_block)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool handshake_ok{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        handshake_ok = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = uint256::ONE,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 111});
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    server.join();
    BOOST_CHECK(handshake_ok);
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::HANDSHAKE_FAILED);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);

    tcp::acceptor matching_acceptor{io, tcp::endpoint{loopback, 0}};
    bool matching_handshake_ok{false};
    std::jthread matching_server{[&] {
        tcp::socket socket{io};
        matching_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        matching_handshake_ok = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = *fixture.runtime->GetFinalizedTip(),
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 112});
    }};
    BOOST_CHECK(manager.Connect(loopback.to_string(), matching_acceptor.local_endpoint().port()));
    matching_server.join();
    BOOST_CHECK(matching_handshake_ok);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 1U);
}

BOOST_AUTO_TEST_CASE(manager_refuses_wrong_genesis_tip)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto wrong_tip = fixture.definition.genesis_block_id == uint256::ONE ?
        *cybou::ParseUint256UserHex("02") : uint256::ONE;
    bool handshake_ok{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        handshake_ok = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = wrong_tip,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 115});
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    server.join();
    BOOST_CHECK(handshake_ok);
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::HANDSHAKE_FAILED);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(manager_reports_unavailable_endpoint)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto port = acceptor.local_endpoint().port();
    acceptor.close();
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), port));
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::UNAVAILABLE);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(storage_placeme_peer_that_closes_without_hello)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        socket.close();
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::UNAVAILABLE);
    server.join();
}

BOOST_AUTO_TEST_CASE(manager_rejects_peer_without_block_service)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 107});
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto result = manager.SyncFromPeer(address, port, 1);
    server.join();
    BOOST_CHECK(result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(manager_distinguishes_malformed_block_response_from_disconnect)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    const auto network = fixture.runtime->GetNetworkId();
    for (const bool malformed : {true, false}) {
        tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
        bool served{false};
        std::jthread server{[&] {
            tcp::socket socket{io};
            acceptor.accept(socket);
            cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
            served = session.Handshake({.network_id = network, .finalized_height = 1,
                .finalized_tip = fixture.definition.genesis_block_id, .capabilities = cybou::p2p::CAP_SERVE_BLOCKS,
                .nonce = malformed ? 108ULL : 109ULL});
            const auto request = session.ReceiveFrame();
            served = served && request && request->type == cybou::p2p::MessageType::GET_BLOCK;
            if (served && malformed) {
                served = session.SendFrame({cybou::p2p::MessageType::BLOCK_META,
                    {0xff, 0xff, 0xff, 0x7f}});
            }
        }};
        cybou::p2p::PeerManager manager{*fixture.runtime};
        const auto address = loopback.to_string();
        const auto port = acceptor.local_endpoint().port();
        BOOST_REQUIRE(manager.Connect(address, port));
        const auto result = manager.SyncFromPeer(address, port, 1);
        server.join();
        BOOST_CHECK(served);
        BOOST_CHECK(result.status == (malformed ? cybou::SyncPeerStatus::PROTOCOL_ERROR :
            cybou::SyncPeerStatus::CONNECTION_FAILED));
        BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
    }
}

BOOST_AUTO_TEST_CASE(manager_retries_peer_that_cannot_serve_announced_height)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    for (const bool promises_block : {false, true}) {
        tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
        bool served{false};
        std::jthread server{[&] {
            tcp::socket socket{io};
            acceptor.accept(socket);
            cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
            served = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
                .finalized_height = promises_block ? 1ULL : 0ULL, .finalized_tip = fixture.definition.genesis_block_id,
                .capabilities = cybou::p2p::CAP_SERVE_BLOCKS,
                .nonce = promises_block ? 113ULL : 114ULL}) &&
                session.ServeNext(*fixture.runtime);
        }};
        cybou::p2p::PeerManager manager{*fixture.runtime};
        const auto address = loopback.to_string();
        const auto port = acceptor.local_endpoint().port();
        BOOST_REQUIRE(manager.Connect(address, port));
        const auto result = manager.SyncFromPeer(address, port, 1);
        server.join();
        BOOST_CHECK(served);
        BOOST_CHECK(result.status == (promises_block ? cybou::SyncPeerStatus::CONNECTION_FAILED :
            cybou::SyncPeerStatus::UP_TO_DATE));
        BOOST_CHECK_EQUAL(manager.ConnectedCount(), promises_block ? 0U : 1U);
    }
}

BOOST_AUTO_TEST_CASE(manager_syncs_two_verified_blocks_on_one_session)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "observer", .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served = session.Handshake({.network_id = network, .finalized_height = 2,
            .finalized_tip = fixture.runtime->GetFinalizedTip().value(),
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS | cybou::p2p::CAP_BLOCK_INVENTORY, .nonce = 104}) &&
            session.ServeNext(*fixture.runtime) && session.ServeNext(*fixture.runtime) &&
            session.ServeNext(*fixture.runtime);
    }};
    cybou::p2p::PeerManager manager{observer};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto result = manager.SyncFromPeer(address, port, 2);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK_EQUAL(result.blocks_applied, 2U);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(0), 2U);
    BOOST_CHECK(observer.GetFinalizedTip() == fixture.runtime->GetFinalizedTip());
}

BOOST_AUTO_TEST_CASE(manager_fans_out_finalized_block_without_duplicate_payload)
{
    CybouServiceTestFixture fixture;
    const auto produced = fixture.runtime->ProduceBlock();
    BOOST_REQUIRE(produced);
    const auto block_id = cybou::ComputeBlockId(produced->block);
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "block-gossip-observer", .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_BLOCK_ANNOUNCEMENTS, .nonce = 121}) &&
            session.ServeNext(observer);
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_REQUIRE(manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    BOOST_CHECK_EQUAL(manager.FanoutRecentBlocks(), 1U);
    BOOST_CHECK_EQUAL(manager.FanoutRecentBlocks(), 0U);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 1U);
    BOOST_CHECK(observer.GetFinalizedTip().value_or(uint256{}) == block_id);
}

BOOST_AUTO_TEST_CASE(manager_rejects_block_that_disagrees_with_inventory)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "inventory-observer", .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        if (!session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = fixture.runtime->GetFinalizedTip().value(),
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS | cybou::p2p::CAP_BLOCK_INVENTORY,
            .nonce = 115})) return;
        const auto request = session.ReceiveFrame();
        if (!request || request->type != cybou::p2p::MessageType::GET_BLOCKS) return;
        std::vector<unsigned char> inventory{1, 1, 0, 0, 0, 0, 0, 0, 0};
        inventory.insert(inventory.end(), uint256::ONE.begin(), uint256::ONE.end());
        if (!session.SendFrame({cybou::p2p::MessageType::BLOCK_INV, inventory})) return;
        const auto get_block = session.ReceiveFrame();
        if (!get_block || get_block->type != cybou::p2p::MessageType::GET_BLOCK ||
            get_block->payload.size() != 8) return;
        const auto block = fixture.runtime->GetBlockAtHeight(1);
        const auto encoded = block ? cybou::SerializeFinalizedBlock(*block) : std::nullopt;
        if (!encoded) return;
        std::vector<unsigned char> block_size(4);
        const auto size = static_cast<uint32_t>(encoded->size());
        for (unsigned i = 0; i < 4; ++i) block_size[i] = static_cast<unsigned char>(size >> (8 * i));
        served = session.SendFrame({cybou::p2p::MessageType::BLOCK_META, block_size}) &&
            session.SendFrame({cybou::p2p::MessageType::BLOCK_CHUNK, *encoded});
    }};
    cybou::p2p::PeerManager manager{observer};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto result = manager.SyncFromPeer(address, port, 1);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK(result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR);
    BOOST_CHECK_EQUAL(result.blocks_applied, 0U);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0U);
}

BOOST_AUTO_TEST_CASE(manager_rejects_block_conflicting_with_announced_finalized_tip)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "tip-observer", .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = uint256::ONE,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 110}) &&
            session.ServeNext(*fixture.runtime);
    }};
    cybou::p2p::PeerManager manager{observer};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto result = manager.SyncFromPeer(address, port, 1);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK(result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR);
    BOOST_CHECK_EQUAL(result.blocks_applied, 0U);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0U);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(manager_submits_canonical_operation_with_separate_acknowledgment)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("p2p-account.cybou");
    const auto source_block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source_block);
    BOOST_REQUIRE_EQUAL(source_block->block.operations.size(), 1U);
    const auto operation = source_block->block.operations.front();
    const auto op_id = cybou::ComputeOperationId(operation);
    BOOST_REQUIRE(op_id);

    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "other-producer", .poa_finalizer_recovery_entropy = fixture.validator_seed,
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime receiver{std::move(config)};
    BOOST_REQUIRE(receiver.InitializeGenesis(fixture.genesis));
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served = fixture.HandshakeAsFinalizer(session, {.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 105}) &&
            session.ServeNext(receiver) && session.ServeNext(receiver);
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto submitted = manager.SubmitOperation(address, port, operation);
    const auto repeated = manager.SubmitOperation(address, port, operation);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK(submitted.status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_CHECK(submitted.op_id == *op_id);
    BOOST_CHECK(repeated.status == cybou::OperationSubmitStatus::ALREADY_PENDING);
    BOOST_CHECK(repeated.op_id == *op_id);
    BOOST_CHECK_EQUAL(receiver.GetFinalizedHeight().value_or(99), 0U);
    const auto finalized = receiver.ProduceBlock();
    BOOST_REQUIRE(finalized);
    BOOST_CHECK_EQUAL(finalized->block.operations.size(), 1U);
    BOOST_CHECK(finalized->block.operations.front() == operation);
}

BOOST_AUTO_TEST_CASE(manager_submits_to_next_peer_when_first_cannot_accept_operations)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("p2p-failover.cybou");
    const auto source = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source);
    const auto operation = source->block.operations.front();
    const auto op_id = cybou::ComputeOperationId(operation);
    BOOST_REQUIRE(op_id);
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "failover-producer",
        .poa_finalizer_recovery_entropy = fixture.validator_seed, .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime receiver{std::move(config)};
    BOOST_REQUIRE(receiver.InitializeGenesis(fixture.genesis));

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor first{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor second{io, tcp::endpoint{loopback, 0}};
    bool first_handshake{false};
    bool second_served{false};
    std::jthread first_server{[&] {
        tcp::socket socket{io};
        first.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        first_handshake = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = 0, .nonce = 116});
    }};
    std::jthread second_server{[&] {
        tcp::socket socket{io};
        second.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        second_served = fixture.HandshakeAsFinalizer(session, {.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 117}) &&
            session.ServeNext(receiver);
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto result = manager.SubmitOperationToAny({{address, first.local_endpoint().port()},
        {address, second.local_endpoint().port()}}, operation);
    first_server.join();
    second_server.join();
    BOOST_CHECK(first_handshake);
    BOOST_CHECK(second_served);
    BOOST_REQUIRE(result.acknowledgment);
    BOOST_CHECK(result.acknowledgment->status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_CHECK(result.acknowledgment->op_id == *op_id);
    BOOST_CHECK(!result.delivery_uncertain);
    BOOST_REQUIRE(result.endpoint);
    BOOST_CHECK_EQUAL(result.endpoint->second, second.local_endpoint().port());
    BOOST_CHECK_EQUAL(receiver.GetFinalizedHeight().value_or(99), 0U);
    const auto finalized = receiver.ProduceBlock();
    BOOST_REQUIRE(finalized);
    BOOST_CHECK_EQUAL(finalized->block.operations.size(), 1U);
    BOOST_CHECK(finalized->block.operations.front() == operation);
}

BOOST_AUTO_TEST_CASE(manager_distinguishes_rejection_from_missing_operation_acknowledgment)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("p2p-ack-status.cybou");
    const auto source = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source);
    const auto operation = source->block.operations.front();
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();

    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "rejecting-observer", .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime rejecting{std::move(config)};
    BOOST_REQUIRE(rejecting.InitializeGenesis(fixture.genesis));
    tcp::acceptor reject_acceptor{io, tcp::endpoint{loopback, 0}};
    bool rejected_response_sent{false};
    std::jthread reject_server{[&] {
        tcp::socket socket{io};
        reject_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        rejected_response_sent = fixture.HandshakeAsFinalizer(session, {.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 118}) &&
            session.ServeNext(rejecting);
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto rejected = manager.SubmitOperationToAny({{address, reject_acceptor.local_endpoint().port()}}, operation);
    reject_server.join();
    BOOST_CHECK(rejected_response_sent);
    BOOST_REQUIRE(rejected.acknowledgment);
    BOOST_CHECK(rejected.acknowledgment->status == cybou::OperationSubmitStatus::REJECTED);
    BOOST_CHECK(rejected.endpoint);
    BOOST_CHECK(!rejected.delivery_uncertain);

    tcp::acceptor dropped_acceptor{io, tcp::endpoint{loopback, 0}};
    bool dropped_handshake{false};
    std::jthread dropped_server{[&] {
        tcp::socket socket{io};
        dropped_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        dropped_handshake = fixture.HandshakeAsFinalizer(session, {.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 119});
    }};
    cybou::NodeRuntimeConfig observer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "unconfirmed-observer",
        .p2p_endpoint = std::make_pair(address, dropped_acceptor.local_endpoint().port()),
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    const auto unconfirmed = observer.SubmitOperation(operation);
    dropped_server.join();
    BOOST_CHECK(dropped_handshake);
    BOOST_CHECK(unconfirmed.status == cybou::OperationSubmitStatus::REJECTED);
    BOOST_CHECK(unconfirmed.delivery_uncertain);
}

BOOST_AUTO_TEST_CASE(runtime_routes_submission_and_verified_sync_over_configured_peer)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("runtime-route.cybou");
    const auto source = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source);
    const auto operation = source->block.operations.front();

    cybou::NodeRuntimeConfig producer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "route-producer", .poa_finalizer_recovery_entropy = fixture.validator_seed,
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime producer{std::move(producer_config)};
    BOOST_REQUIRE(producer.InitializeGenesis(fixture.genesis));
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto network = producer.GetNetworkId();
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        served = fixture.HandshakeAsFinalizer(session, {.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = cybou::p2p::CAP_SERVE_BLOCKS |
                cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 106}) &&
            session.ServeNext(producer) && producer.ProduceBlock().has_value() &&
            session.ServeNext(producer) && session.ServeNext(producer);
    }};
    cybou::NodeRuntimeConfig observer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "route-observer",
        .p2p_endpoint = std::make_pair(loopback.to_string(), acceptor.local_endpoint().port()),
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(observer.CanSubmitOperations());
    const auto submitted = observer.SubmitOperation(operation);
    BOOST_CHECK(submitted.status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0U);
    const auto synced = observer.SyncFromConfiguredPeer(1);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK_EQUAL(synced.blocks_applied, 1U);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(0), 1U);
    BOOST_CHECK(observer.GetFinalizedTip() == producer.GetFinalizedTip());
    BOOST_CHECK_EQUAL(observer.ConnectedPeerCount(), 1U);
}

BOOST_AUTO_TEST_CASE(runtime_discovers_and_syncs_from_a_second_peer)
{
    CybouServiceTestFixture fixture;
    const auto block = fixture.runtime->ProduceBlock();
    BOOST_REQUIRE(block);

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor seed_acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor source_acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor* seed_listener{&seed_acceptor};
    tcp::acceptor* source_listener{&source_acceptor};
    if (seed_listener->local_endpoint().port() > source_listener->local_endpoint().port()) {
        std::swap(seed_listener, source_listener);
    }
    const auto network = fixture.runtime->GetNetworkId();
    const auto seed_endpoint = std::make_pair(loopback.to_string(), seed_listener->local_endpoint().port());
    const auto source_endpoint = std::make_pair(loopback.to_string(), source_listener->local_endpoint().port());

    cybou::NodeRuntimeConfig seed_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "multi-peer-seed", .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime seed{std::move(seed_config)};
    BOOST_REQUIRE(seed.InitializeGenesis(fixture.genesis));
    seed.SetExplicitPeerEndpoints({source_endpoint});

    std::atomic_bool seed_served{false};
    std::atomic_bool source_served{false};
    std::jthread seed_server{[&] {
        tcp::socket socket{io};
        seed_listener->accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const bool handshake = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS | cybou::p2p::CAP_PEER_DISCOVERY, .nonce = 7811});
        seed_served = handshake && session.ServeNext(seed) && session.ServeNext(seed) &&
            session.ServeNext(seed) && session.ServeNext(seed);
    }};
    std::jthread source_server{[&] {
        tcp::socket socket{io};
        source_listener->accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        // Both peers advertise the same stale HELLO snapshot. The source's
        // canonical store is already ahead, so stopping at the first
        // UP_TO_DATE peer would mask its new block.
        const bool handshake = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 7812});
        source_served = handshake && session.ServeNext(*fixture.runtime) && session.ServeNext(*fixture.runtime);
    }};

    cybou::NodeRuntimeConfig observer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "multi-peer-observer", .p2p_endpoint = seed_endpoint,
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    auto observer = std::make_unique<cybou::CybouNodeRuntime>(std::move(observer_config));
    BOOST_REQUIRE(observer->InitializeGenesis(fixture.genesis));

    const auto first_sync = observer->SyncFromConfiguredPeer(1);
    BOOST_CHECK(first_sync.IsConnected());
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(99), 0U);
    const auto second_sync = observer->SyncFromConfiguredPeer(1);
    BOOST_CHECK(second_sync.IsConnected());
    BOOST_CHECK_EQUAL(second_sync.blocks_applied, 1U);
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(0), 1U);
    BOOST_CHECK_EQUAL(observer->ConnectedPeerCount(), 2U);

    seed_server.join();
    source_server.join();
    BOOST_CHECK(seed_served.load());
    BOOST_CHECK(source_served.load());
}

BOOST_AUTO_TEST_CASE(manager_discovers_peers_from_connected_peer)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto network = fixture.runtime->GetNetworkId();

    const std::vector<std::pair<std::string, uint16_t>> seed_peers{
        {"192.168.1.50", 29460},
        {"192.168.1.51", 29460},
    };

    // Remote node with CAP_PEER_DISCOVERY
    cybou::NodeRuntimeConfig remote_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "remote-discovery-node",
        .p2p_endpoint = std::make_pair("192.168.1.49", uint16_t{29460}),
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime remote_runtime{std::move(remote_config)};
    BOOST_REQUIRE(remote_runtime.InitializeGenesis(fixture.genesis));
    remote_runtime.AddDiscoveredPeerEndpoints(seed_peers);

    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        BOOST_REQUIRE(session.Handshake({
            .network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_PEER_DISCOVERY,
            .nonce = 7771,
        }));
        session.ServeNext(remote_runtime);
    }};

    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_REQUIRE(manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 1U);

    // Initial known endpoints on local runtime should be empty
    BOOST_CHECK_EQUAL(fixture.runtime->GetPeerEndpointsForGossip().size(), 0U);

    // DiscoverPeers queries remote peer and populates local runtime
    BOOST_CHECK_EQUAL(manager.DiscoverPeers(0), 0U);
    const size_t added = manager.DiscoverPeers(1);
    server.join();

    BOOST_CHECK_GE(added, 2U);
    const auto known = manager.KnownEndpoints();
    BOOST_CHECK_GE(known.size(), 2U);
}

BOOST_AUTO_TEST_SUITE_END()
