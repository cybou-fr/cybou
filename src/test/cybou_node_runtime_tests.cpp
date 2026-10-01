// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/crypto/cleanse.h>
#include <cybou/hex.h>
#include <cybou/kv_store.h>
#include <cybou/poa_finalizer.h>
#include <cybou/secret_file.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_test_setup.h>

#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>

#include <fstream>
#include <thread>
#ifndef _WIN32
#include <sys/stat.h>
#endif

BOOST_FIXTURE_TEST_SUITE(cybou_node_runtime_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(secret_files_are_private_and_reject_links)
{
    CybouServiceTestFixture fixture;
    const auto path = fixture.directory / "secret.bin";
    const std::array<unsigned char, 4> secret{1, 2, 3, 4};
    BOOST_REQUIRE(cybou::CreateSecretFile(path, secret));
    const auto read = cybou::ReadSecretFile(path, 4);
    BOOST_REQUIRE(read);
    BOOST_CHECK(*read == std::vector<unsigned char>(secret.begin(), secret.end()));
    BOOST_CHECK(!cybou::CreateSecretFile(path, secret));
#ifndef _WIN32
    struct stat info{};
    BOOST_REQUIRE_EQUAL(::stat(path.c_str(), &info), 0);
    BOOST_CHECK_EQUAL(info.st_mode & 0777, 0600);
    const auto link = fixture.directory / "secret-link.bin";
    std::filesystem::create_symlink(path, link);
    BOOST_CHECK(!cybou::ReadSecretFile(link, 4));
#endif
}

BOOST_AUTO_TEST_CASE(event_log_privacy_modes_filter_sensitive_identifiers)
{
    CybouServiceTestFixture fixture;
    const auto minimal_path = fixture.directory / "minimal-events.jsonl";
    const auto lab_path = fixture.directory / "lab-events.jsonl";
    const cybou::EventFields fields{{"operation_id", std::string{"operation-secret"}},
        {"account_id", std::string{"account-secret"}}, {"peer", std::string{"peer-secret"}},
        {"network_id", std::string{"public-network"}}};
    {
        cybou::EventWriter writer{minimal_path};
        writer.Write(cybou::NodeEvent::operation_accepted, fields);
    }
    {
        cybou::EventWriter writer{lab_path, cybou::EventLogMode::LAB};
        writer.Write(cybou::NodeEvent::operation_accepted, fields);
    }
    std::ifstream minimal_file{minimal_path};
    std::ifstream lab_file{lab_path};
    const std::string minimal{std::istreambuf_iterator<char>{minimal_file}, {}};
    const std::string lab{std::istreambuf_iterator<char>{lab_file}, {}};
    BOOST_CHECK(minimal.find("operation-secret") == std::string::npos);
    BOOST_CHECK(minimal.find("account-secret") == std::string::npos);
    BOOST_CHECK(minimal.find("peer-secret") == std::string::npos);
    BOOST_CHECK(minimal.find("public-network") != std::string::npos);
    BOOST_CHECK(lab.find("operation-secret") != std::string::npos);
    BOOST_CHECK(lab.find("account-secret") != std::string::npos);
    BOOST_CHECK(lab.find("peer-secret") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(public_event_writer_rejects_secret_fields)
{
    CybouServiceTestFixture fixture;
    const auto path = fixture.directory / "events.jsonl";
    cybou::EventWriter writer{path};
    BOOST_CHECK_THROW(writer.Write(cybou::NodeEvent::node_started, {{"mnemonic", std::string{"secret"}}}), std::invalid_argument);
    BOOST_CHECK_THROW(writer.Write(cybou::NodeEvent::node_started, {{"role", std::string(257, 'x')}}), std::invalid_argument);
    writer.Write(cybou::NodeEvent::node_status, {{"network_id", fixture.runtime->GetNetworkId().GetHex()},{"height",std::uint64_t{1}}});
    BOOST_CHECK(writer.Good());
    const auto snapshot=fixture.runtime->GetDiagnostics();
    BOOST_CHECK(snapshot.initialized);
    BOOST_CHECK_EQUAL(snapshot.role, "finalizer");
    BOOST_CHECK_EQUAL(snapshot.height,fixture.runtime->GetStatus().finalized_height);
}

BOOST_AUTO_TEST_CASE(runtime_finalizes_account_and_observer_verifies_block)
{
    CybouServiceTestFixture fixture;
    const auto status = fixture.runtime->GetStatus();
    BOOST_CHECK(status.is_initialized);
    BOOST_CHECK(status.is_finalizer);
    const auto alice = fixture.CreateIdentity("alice.cybou");
    const auto account = alice->GetAccountId();
    BOOST_REQUIRE(account);
    BOOST_CHECK(fixture.runtime->GetAccountState(*account));
    const auto block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(block);

    cybou::NodeRuntimeConfig observer_config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "observer",
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(!observer.GetStatus().is_finalizer);
    BOOST_REQUIRE(observer.CommitBlock(*block));
    BOOST_CHECK(observer.GetAccountState(*account) == fixture.runtime->GetAccountState(*account));
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(0), 1);
    BOOST_REQUIRE_EQUAL(block->block.operations.size(), 1U);
    const auto op_id = cybou::ComputeOperationId(block->block.operations.front());
    BOOST_REQUIRE(op_id);
    const auto found = observer.FindFinalizedOperation(*op_id);
    BOOST_CHECK(found.status == cybou::FinalizedOperationLookupStatus::FOUND);
    BOOST_CHECK_EQUAL(found.height, 1U);
    BOOST_CHECK_EQUAL(found.operation_index, 0U);
    BOOST_CHECK(found.block_id == cybou::ComputeBlockId(block->block));
    const auto missing_id = *op_id == uint256::ONE ? *cybou::ParseUint256UserHex("02") : uint256::ONE;
    const auto missing = observer.FindFinalizedOperation(missing_id);
    BOOST_CHECK(missing.status == cybou::FinalizedOperationLookupStatus::NOT_FOUND);
    BOOST_CHECK_EQUAL(missing.scanned_height, 1U);
}

BOOST_AUTO_TEST_CASE(runtime_halts_on_valid_poa_equivocation)
{
    CybouServiceTestFixture fixture;
    const auto canonical = fixture.runtime->ProduceBlock();
    BOOST_REQUIRE(canonical);

    auto conflicting_block = canonical->block;
    conflicting_block.resulting_state_root.begin()[0] ^= 0x80;
    cybou::KVStore alternate_signer_db{cybou::KVStoreOptions{.memory_only = true}};
    cybou::RecoveryEntropy operator_entropy{};
    operator_entropy[0] = fixture.validator_seed[0];
    cybou::PoaFinalizer alternate_signer{alternate_signer_db, fixture.runtime->GetNetworkId(),
        fixture.definition.genesis_block_id, operator_entropy,
        fixture.definition.poa_finalizer_public_key};
    const auto alternate_signature = alternate_signer.SignFinality(0,
        fixture.definition.genesis_block_id, conflicting_block);
    BOOST_REQUIRE(alternate_signature.certificate);

    cybou::FinalizedBlock conflicting{.block = conflicting_block,
        .certificate = *alternate_signature.certificate};
    const auto result = fixture.runtime->CommitBlock(conflicting);
    BOOST_CHECK(result.error == cybou::BlockTransitionError::POA_EQUIVOCATION_DETECTED);
    const auto status = fixture.runtime->GetStatus();
    BOOST_CHECK(status.poa_safety_halted);
    BOOST_CHECK(status.runtime_state == cybou::NodeRuntimeState::SAFETY_HALTED);
    BOOST_CHECK(!fixture.runtime->ProduceBlock());
    const auto evidence = fixture.runtime->ReadPoaSafetyEvidence();
    BOOST_CHECK(evidence.status == cybou::PoaEvidenceReadStatus::EQUIVOCATION);
    BOOST_REQUIRE(evidence.equivocation);
    BOOST_CHECK(evidence.equivocation->first.block_id != evidence.equivocation->second.block_id);
}

BOOST_AUTO_TEST_CASE(runtime_resolves_only_finalized_root_publications)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("publication-owner.cybou");
    const auto account = identity->GetAccountId();
    BOOST_REQUIRE(account);
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x31);
    publication.chunk_authorization_root.fill(0x42);
    publication.chunk_count = 1;
    cybou::RootRecipientCapsule capsule;
    capsule.encapsulation.fill(0x53);
    capsule.wrapped_content_key.fill(0x64);
    publication.recipient_capsules.push_back(capsule);
    const auto commitment = cybou::ComputeRootPublicationPayloadCommitment(publication);
    BOOST_REQUIRE(commitment);
    const auto loaded = fixture.runtime->GetStore().LoadState();
    const auto* record = loaded && loaded.state ? loaded.state->identities.Find(*account) : nullptr;
    BOOST_REQUIRE(record);
    cybou::IdentityOperationAuthorization auth{
        .account_id = *account,
        .nonce = record->nonce,
        .key_epoch = record->key_epoch,
        .kind = cybou::IdentityOperationKind::ROOT_PUBLICATION,
        .payload_commitment = *commitment,
    };
    const auto digest = cybou::ComputeIdentityOperationDigest(fixture.runtime->GetNetworkId(), auth);
    BOOST_REQUIRE(digest);
    const auto signature = identity->GetKeyStore().SignAuthorization(*digest);
    BOOST_REQUIRE(signature);
    auth.signature = *signature;
    const cybou::ProtocolOperation operation{cybou::AuthorizedRootPublication{auth, publication}};
    const auto submitted = fixture.runtime->SubmitOperation(operation);
    BOOST_REQUIRE(submitted);
    BOOST_CHECK(!fixture.runtime->FindFinalizedRootPublication(submitted.op_id));
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    BOOST_CHECK(fixture.runtime->FindFinalizedRootPublication(submitted.op_id) == publication);
    BOOST_CHECK(!fixture.runtime->FindFinalizedRootPublication(uint256::ONE));
}

BOOST_AUTO_TEST_CASE(runtime_resolves_current_identity_kem_package_by_key_epoch)
{
    CybouServiceTestFixture fixture;
    const auto path = fixture.directory / "identity-kem-lookup.cybou";
    std::filesystem::remove(path);
    cybou::CybouIdentityService identity{*fixture.runtime, path};
    const auto words = identity.PrepareNewIdentity();
    BOOST_REQUIRE(words);
    const auto created = identity.CreateIdentitySync("correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(created.success, created.error_message);
    const auto account = identity.GetAccountId();
    BOOST_REQUIRE(account);

    const auto initial = fixture.runtime->FindIdentityKemPackage(*account, 0);
    BOOST_REQUIRE(initial.status == cybou::IdentityKemPackageLookupStatus::FOUND);
    BOOST_CHECK_EQUAL(initial.operation_height, 1U);
    const auto create_block = fixture.runtime->GetBlockAtHeight(initial.operation_height);
    BOOST_REQUIRE(create_block);
    const auto* create = std::get_if<cybou::AccountCreateOp>(&create_block->block.operations[initial.operation_index]);
    BOOST_REQUIRE(create);
    BOOST_CHECK(create->kem_package == initial.package);
    const auto account_bytes = account->Value();
    const auto expected_initial = cybou::ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{fixture.runtime->GetNetworkId().begin(), 32},
        std::span<const unsigned char, 32>{account_bytes.begin(), 32}, 0, initial.package);
    BOOST_REQUIRE(expected_initial);
    BOOST_CHECK(*expected_initial == initial.package_id);

    const auto restore_path = fixture.directory / "identity-kem-restored.cybou";
    std::filesystem::remove(restore_path);
    cybou::CybouIdentityService recovered{*fixture.runtime, restore_path};
    const auto restored = recovered.RestoreIdentitySync(*words, "another correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(restored.success, restored.error_message);
    BOOST_CHECK_EQUAL(fixture.runtime->GetFinalizedHeight().value_or(0), 1U);

    auto next_entropy = cybou::GenerateRecoveryEntropy();
    BOOST_REQUIRE(next_entropy);
    const auto next_words = cybou::EncodeRecoveryWords(*next_entropy);
    cybou::crypto::CleanseMemory(next_entropy->data(), next_entropy->size());
    const auto pending = identity.RotateIdentitySync(next_words, "correct horse battery staple");
    BOOST_REQUIRE(pending.phase == cybou::IdentityOperationPhase::ACCEPTED);
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    const auto finalized = identity.ResumeIdentityRotationSync("correct horse battery staple");
    BOOST_REQUIRE(finalized.phase == cybou::IdentityOperationPhase::FINALIZED);

    const auto rotated = fixture.runtime->FindIdentityKemPackage(*account, 1);
    BOOST_REQUIRE(rotated.status == cybou::IdentityKemPackageLookupStatus::FOUND);
    BOOST_CHECK_EQUAL(rotated.operation_height, 2U);
    BOOST_CHECK(rotated.package != initial.package);
    const auto future = fixture.runtime->FindIdentityKemPackage(*account, 2);
    BOOST_CHECK(future.status == cybou::IdentityKemPackageLookupStatus::KEY_EPOCH_UNAVAILABLE);
    std::filesystem::remove(path);
    std::filesystem::remove(restore_path);
}
BOOST_AUTO_TEST_CASE(runtime_rejects_foreign_genesis_and_block)
{
    CybouServiceTestFixture fixture;
    std::array<unsigned char, 32> foreign_seed{};
    foreign_seed[0] = 0xBC;
    auto foreign_genesis = cybou::CreateDevGenesisState();
    ++foreign_genesis.onboarding_pool;
    cybou::NodeRuntimeConfig config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "foreign-observer",
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_CHECK(!observer.InitializeGenesis(foreign_genesis));
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    const auto foreign_definition = cybou::CreateDevNetworkDefinition(foreign_genesis, cybou::TestPoaFinalizerPublicKey(0xBC));
    cybou::NodeRuntimeConfig foreign_config{
        .network_definition = foreign_definition,
        .data_dir = fixture.directory / "foreign-producer",
        .poa_finalizer_recovery_entropy = foreign_seed,
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime foreign{std::move(foreign_config)};
    BOOST_REQUIRE(foreign.InitializeGenesis(foreign_genesis));
    const auto block = foreign.ProduceBlock();
    BOOST_REQUIRE(block);
    BOOST_CHECK(!observer.CommitBlock(*block));
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0);
}

BOOST_AUTO_TEST_CASE(runtime_explicit_peers_take_priority_over_discovered)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "peer-priority",
        .local_p2p_endpoint = std::make_pair("127.0.0.1", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));

    // Operator-approved validator endpoints.
    runtime.SetExplicitPeerEndpoints({{"10.0.0.10", 8333}, {"10.0.0.11", 8333}});
    // Malicious flood: lexicographically smaller addresses that would eclipse
    // the validator topology in a single sorted set, plus this node's own
    // listener, plus out-of-scope targets.
    std::vector<std::pair<std::string, uint16_t>> flood;
    for (int i = 1; i <= 40; ++i) {
        flood.emplace_back("1.1.1." + std::to_string(i), 7000);
    }
    flood.emplace_back("127.0.0.1", 29001); // own listener
    flood.emplace_back("127.0.0.1", 29002); // own address, other port
    flood.emplace_back("0.0.0.0", 8333);    // unspecified
    flood.emplace_back("224.0.0.1", 8333);  // multicast
    flood.emplace_back("169.254.1.1", 8333); // link-local
    runtime.AddDiscoveredPeerEndpoints(flood);

    const auto gossip = runtime.GetPeerEndpointsForGossip();
    // Capped at 32 targets: explicit peers first, discovered flood behind them.
    BOOST_REQUIRE_EQUAL(gossip.size(), 32U);
    // Explicit validator peers always come first, in front of any discovered
    // lexicographically-smaller hint.
    BOOST_CHECK_EQUAL(gossip[0].first, "10.0.0.10");
    BOOST_CHECK_EQUAL(gossip[1].first, "10.0.0.11");
    BOOST_CHECK_EQUAL(gossip[0].second, 8333);
    // Flood addresses are present but strictly behind the explicit peers.
    for (size_t i = 2; i < gossip.size(); ++i) {
        BOOST_CHECK(gossip[i].first.rfind("1.1.1.", 0) == 0);
    }
}

BOOST_AUTO_TEST_CASE(runtime_discovery_filters_self_and_out_of_scope_addresses)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "peer-policy",
        .local_p2p_endpoint = std::make_pair("203.0.113.5", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    runtime.SetExplicitPeerEndpoints({
        {"10.0.0.7", 8333}, {"172.16.0.7", 8333}, {"192.168.0.7", 8333},
    });

    // With a public listener, loopback/link-local/unspecified/multicast
    // and private discovered targets must not be dialed (SSRF-style pivot).
    runtime.AddDiscoveredPeerEndpoints({
        {"203.0.113.5", 29001},   // own listener: always rejected
        {"127.0.0.1", 8333},      // loopback rejected: listener is public
        {"::1", 8333},            // v6 loopback rejected
        {"169.254.10.20", 8333},  // v4 link-local rejected
        {"fe80::1", 8333},        // v6 link-local rejected
        {"10.1.2.3", 8333},       // RFC1918 rejected: listener is public
        {"172.31.2.3", 8333},     // RFC1918 rejected: listener is public
        {"192.168.2.3", 8333},    // RFC1918 rejected: listener is public
        {"fd00::1", 8333},        // unique-local rejected: listener is public
        {"::ffff:10.1.2.3", 8333}, // mapped RFC1918 rejected as well
        {"0.0.0.0", 8333},        // unspecified rejected
        {"::", 8333},             // unspecified v6 rejected
        {"224.0.0.1", 8333},      // multicast rejected
        {"198.51.100.7", 8333},   // public: accepted
    });
    const auto gossip = runtime.GetPeerEndpointsForGossip();
    BOOST_REQUIRE_EQUAL(gossip.size(), 4U);
    BOOST_CHECK_EQUAL(gossip[0].first, "10.0.0.7");
    BOOST_CHECK_EQUAL(gossip[1].first, "172.16.0.7");
    BOOST_CHECK_EQUAL(gossip[2].first, "192.168.0.7");
    BOOST_CHECK_EQUAL(gossip[3].first, "198.51.100.7");
}

BOOST_AUTO_TEST_CASE(runtime_private_listener_accepts_private_discovery)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "private-peer-policy",
        .local_p2p_endpoint = std::make_pair("10.1.1.1", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    runtime.AddDiscoveredPeerEndpoints({
        {"10.1.1.1", 29001},    // own listener remains rejected
        {"10.1.1.2", 29002},    // private discovery is allowed on private nodes
        {"172.16.2.3", 29003},
        {"192.168.2.4", 29004},
        {"198.51.100.5", 29005},
    });

    const auto gossip = runtime.GetPeerEndpointsForGossip();
    BOOST_REQUIRE_EQUAL(gossip.size(), 4U);
    BOOST_CHECK_EQUAL(gossip[0].first, "10.1.1.2");
    BOOST_CHECK_EQUAL(gossip[1].first, "172.16.2.3");
    BOOST_CHECK_EQUAL(gossip[2].first, "192.168.2.4");
    BOOST_CHECK_EQUAL(gossip[3].first, "198.51.100.5");
}

BOOST_AUTO_TEST_CASE(sync_tip_confirmation_requires_the_configured_genesis_finalizer)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor finalizer_acceptor{io, tcp::endpoint{loopback, 0}};
    const auto finalizer_port = finalizer_acceptor.local_endpoint().port();
    const auto tip = fixture.runtime->GetFinalizedTip();
    BOOST_REQUIRE(tip);
    std::atomic_bool finalizer_served{false};
    std::jthread finalizer_server{[&] {
        tcp::socket socket{io};
        finalizer_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const bool handshake = fixture.HandshakeAsFinalizer(session, {.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = *tip,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS | cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 1301});
        finalizer_served = handshake && session.ServeNext(*fixture.runtime) && session.ServeNext(*fixture.runtime);
    }};

    cybou::NodeRuntimeConfig observer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "finalizer-tip-observer",
        .p2p_endpoint = std::make_pair(loopback.to_string(), finalizer_port),
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    const auto finalizer_sync = observer.SyncFromConfiguredPeer(10);
    finalizer_server.join();
    BOOST_CHECK(finalizer_served.load());
    BOOST_CHECK_EQUAL(finalizer_sync.blocks_applied, 1U);
    BOOST_CHECK(finalizer_sync.reached_peer_tip);

    tcp::acceptor provider_acceptor{io, tcp::endpoint{loopback, 0}};
    const auto provider_port = provider_acceptor.local_endpoint().port();
    std::atomic_bool provider_served{false};
    std::jthread provider_server{[&] {
        tcp::socket socket{io};
        provider_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const bool handshake = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = *tip,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 1302});
        provider_served = handshake && session.ServeNext(*fixture.runtime) && session.ServeNext(*fixture.runtime);
    }};

    cybou::NodeRuntimeConfig provider_observer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "provider-tip-observer",
        .p2p_endpoint = std::make_pair(loopback.to_string(), provider_port),
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
    cybou::CybouNodeRuntime provider_observer{std::move(provider_observer_config)};
    BOOST_REQUIRE(provider_observer.InitializeGenesis(fixture.genesis));
    const auto provider_sync = provider_observer.SyncFromConfiguredPeer(10);
    provider_server.join();
    BOOST_CHECK(provider_served.load());
    BOOST_CHECK_EQUAL(provider_sync.blocks_applied, 1U);
    BOOST_CHECK(!provider_sync.reached_peer_tip);
}

BOOST_AUTO_TEST_SUITE_END()
