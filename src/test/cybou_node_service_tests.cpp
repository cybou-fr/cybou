// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#include <cybou/identity_service.h>
#include <cybou/node_service.h>
#include <cybou/p2p/ingress_budget.h>
#include <cybou/p2p/session.h>
#include <cybou/p2p/inbound_server.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_test_setup.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <filesystem>
#include <future>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

BOOST_FIXTURE_TEST_SUITE(cybou_node_service_tests, CybouTestSetup)

namespace {
/// Creates one Identity on \p runtime in the background; \p ok tells whether it was finalized.
std::jthread StartAccountCreate(cybou::CybouNodeRuntime& runtime, std::filesystem::path vault,
    std::shared_ptr<std::atomic<bool>> ok)
{
    return std::jthread{[&runtime, vault = std::move(vault), ok] {
        std::filesystem::remove(vault);
        cybou::CybouIdentityService identity{runtime, vault};
        *ok = identity.PrepareNewIdentity() &&
            identity.CreateIdentitySync("correct horse battery staple", nullptr, std::chrono::seconds{10}).success;
    }};
}

/// Creates one Identity and waits until the block production worker finalized it.
bool SubmitAccountCreate(cybou::CybouNodeRuntime& runtime, const std::filesystem::path& vault)
{
    auto ok = std::make_shared<std::atomic<bool>>(false);
    StartAccountCreate(runtime, vault, ok).join();
    return *ok;
}

class FlakySigner final : public cybou::PoaSigner {
    cybou::RecoveryEntropy m_seed;
public:
    mutable std::atomic<unsigned> failures{1};
    mutable std::vector<unsigned char> first_digest;
    mutable std::atomic<bool> exact_retry{false};
    explicit FlakySigner(cybou::RecoveryEntropy seed) : m_seed{seed} {}
    std::optional<cybou::IdentityHybridPublicKey> PublicKey() const override
    { return cybou::DeriveIdentityPublicKey(m_seed, cybou::IdentityKeyPurpose::POA_FINALIZER); }
    std::optional<cybou::IdentityHybridSignature> Sign(std::span<const unsigned char> message) const override {
        if (failures.load() && failures.fetch_sub(1)) {
            first_digest.assign(message.begin(), message.end());
            return std::nullopt;
        }
        if (!first_digest.empty()) {
            exact_retry = std::equal(first_digest.begin(), first_digest.end(), message.begin(), message.end());
            first_digest.clear();
        }
        return cybou::SignIdentityMessage(m_seed, cybou::IdentityKeyPurpose::POA_FINALIZER, message);
    }
};
}

BOOST_AUTO_TEST_CASE(worker_retries_signing_failure_and_resumes_after_unlock)
{
    CybouServiceTestFixture local;
    cybou::CybouNodeService service{{.runtime = cybou::NodeRuntimeConfig{
        .network_genesis = local.definition, .memory_only = true,
        .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0}, .genesis = local.genesis}};
    service.Start();
    auto signer = std::make_shared<FlakySigner>(local.validator_seed);
    BOOST_REQUIRE(service.Runtime().EnablePoaSigner(signer));
    service.StartBlockProduction(10);
    const auto wait_height = [&](uint64_t height) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (service.Runtime().GetFinalizedHeight().value_or(0) < height && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        return service.Runtime().GetFinalizedHeight().value_or(0) >= height;
    };
    // The worker produces no empty blocks: an idle chain does not grow.
    std::this_thread::sleep_for(std::chrono::milliseconds{150});
    BOOST_CHECK_EQUAL(service.Runtime().GetFinalizedHeight().value_or(0), 0U);
    // Each candidate is finalized in its own block; the first signing attempt fails once.
    BOOST_REQUIRE(SubmitAccountCreate(service.Runtime(), local.directory / "worker-a.cybou"));
    BOOST_REQUIRE(wait_height(1));
    BOOST_REQUIRE(SubmitAccountCreate(service.Runtime(), local.directory / "worker-b.cybou"));
    BOOST_REQUIRE(wait_height(2));
    BOOST_CHECK(!service.Runtime().GetStatus().poa_safety_halted);
    BOOST_CHECK(signer->exact_retry);
    service.Runtime().DisablePoaSigner();
    const auto height = service.Runtime().GetFinalizedHeight().value_or(0);
    // A pending candidate is not finalized while the signer is off, and is once it is back.
    auto third = std::make_shared<std::atomic<bool>>(false);
    auto pending = StartAccountCreate(service.Runtime(), local.directory / "worker-c.cybou", third);
    std::this_thread::sleep_for(std::chrono::milliseconds{150});
    BOOST_CHECK_EQUAL(service.Runtime().GetFinalizedHeight().value_or(0), height);
    BOOST_REQUIRE(service.Runtime().EnablePoaSigner(signer));
    BOOST_REQUIRE(wait_height(height + 1));
    pending.join();
    BOOST_CHECK(*third);
    service.StopBlockProduction();
    BOOST_CHECK_GT(service.Runtime().GetDiagnostics().storage_capacity, 0U);
}

BOOST_AUTO_TEST_CASE(production_wait_preserves_early_events_deadlines_and_stop)
{
    CybouServiceTestFixture local;
    auto& runtime = *local.runtime;
    std::atomic_bool stop{false};
    const auto forever = std::chrono::steady_clock::time_point::max();
    const auto revision = runtime.BlockProductionRevision();
    // An event before entering wait must not be lost.
    runtime.DisablePoaSigner();
    auto early = std::async(std::launch::async, [&] { runtime.WaitForBlockProductionChange(revision, stop, forever); });
    const bool early_ready = early.wait_for(std::chrono::seconds{2}) == std::future_status::ready;
    if (!early_ready) { stop = true; runtime.WakeBlockProduction(); }
    early.get();
    BOOST_CHECK(early_ready);
    stop = false;
    const auto idle_revision = runtime.BlockProductionRevision();
    auto idle = std::async(std::launch::async, [&] { runtime.WaitForBlockProductionChange(idle_revision, stop, forever); });
    BOOST_CHECK(idle.wait_for(std::chrono::milliseconds{150}) == std::future_status::timeout);
    stop = true;
    runtime.WakeBlockProduction();
    const bool stopped = idle.wait_for(std::chrono::seconds{2}) == std::future_status::ready;
    idle.get();
    BOOST_CHECK(stopped);
    stop = false;
    const auto timed_revision = runtime.BlockProductionRevision();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{100};
    runtime.WaitForBlockProductionChange(timed_revision, stop, deadline);
    BOOST_CHECK(std::chrono::steady_clock::now() >= deadline);
}

BOOST_AUTO_TEST_CASE(relay_admission_wakes_production_but_duplicate_does_not)
{
    CybouServiceTestFixture local;
    auto identity = local.CreateIdentity("wakeup-source.cybou");
    const auto block = local.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(block && !block->block.operations.empty());
    const auto bytes = cybou::SerializeProtocolOperation(block->block.operations.front());
    BOOST_REQUIRE(bytes);
    cybou::CybouNodeRuntime receiver{{.network_genesis = local.definition,
        .data_dir = local.directory / "wakeup-receiver", .memory_only = true,
        .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0}};
    BOOST_REQUIRE(receiver.InitializeGenesis(local.genesis));
    const auto before = receiver.BlockProductionRevision();
    BOOST_CHECK(receiver.EnqueueRelayedOperation(*bytes, 0) == cybou::OperationRelayEnqueueStatus::QUEUED);
    BOOST_CHECK_NE(receiver.BlockProductionRevision(), before);
    const auto admitted = receiver.BlockProductionRevision();
    receiver.EnqueueRelayedOperation(*bytes, 0);
    BOOST_CHECK_EQUAL(receiver.BlockProductionRevision(), admitted);
    BOOST_CHECK_EQUAL(receiver.CandidateOperationCount(), 1U);
}

BOOST_AUTO_TEST_CASE(production_interval_does_not_block_shutdown_or_pending_restart)
{
    CybouServiceTestFixture local;
    auto first = local.CreateIdentity("interval-a.cybou");
    auto second = local.CreateIdentity("interval-b.cybou");
    const auto first_block = local.runtime->GetBlockAtHeight(1);
    const auto second_block = local.runtime->GetBlockAtHeight(2);
    BOOST_REQUIRE(first_block && second_block);
    cybou::CybouNodeService service{{.runtime = cybou::NodeRuntimeConfig{
        .network_genesis = local.definition, .data_dir = local.directory / "interval-service",
        .poa_finalizer_recovery_entropy = local.validator_seed, .memory_only = true,
        .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0}, .genesis = local.genesis}};
    service.Start();
    service.StartBlockProduction(60000);
    BOOST_REQUIRE(service.Runtime().SubmitOperation(first_block->block.operations.front()));
    const auto wait_height = [&](uint64_t target) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (service.Runtime().GetFinalizedHeight().value_or(0) < target && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        return service.Runtime().GetFinalizedHeight().value_or(0) >= target;
    };
    BOOST_REQUIRE(wait_height(1));
    BOOST_REQUIRE(service.Runtime().SubmitOperation(second_block->block.operations.front()));
    std::this_thread::sleep_for(std::chrono::milliseconds{150});
    BOOST_CHECK_EQUAL(service.Runtime().GetFinalizedHeight().value_or(0), 1U);
    auto shutdown = std::async(std::launch::async, [&] { service.StopBlockProduction(); });
    BOOST_CHECK(shutdown.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    shutdown.get();
    BOOST_CHECK_EQUAL(service.Runtime().CandidateOperationCount(), 1U);
    service.StartBlockProduction(10);
    BOOST_CHECK(wait_height(2));
    service.StopBlockProduction();
}

BOOST_AUTO_TEST_CASE(storage_budgets_are_separate_and_bytes_are_reserved_before_work)
{
    using Budget = cybou::p2p::IngressBudget;
    Budget budget;
    auto first = budget.AcquireStorageTransfer("peer");
    auto second = budget.AcquireStorageTransfer("peer");
    BOOST_REQUIRE(first && second);
    BOOST_CHECK(!budget.AcquireStorageTransfer("peer"));
    first.reset();
    BOOST_CHECK(budget.AcquireStorageTransfer("peer"));
    const auto now = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < 8; ++i) BOOST_REQUIRE(budget.Admit("peer", Budget::Work::OPERATION, 0, now));
    BOOST_CHECK(!budget.Admit("peer", Budget::Work::OPERATION, 0, now));
    BOOST_CHECK(budget.Admit("peer", Budget::Work::STORAGE_PUT, 16ULL << 20, now));
    BOOST_CHECK(budget.Admit("peer", Budget::Work::STORAGE_GET, 16ULL << 20, now));
    BOOST_CHECK(!budget.Admit("peer", Budget::Work::STORAGE_GET, 1, now));
    BOOST_CHECK(budget.Admit("peer", Budget::Work::STORAGE_PROOF, 100, now + std::chrono::seconds{1}));
    BOOST_CHECK(!budget.Admit("peer", Budget::Work::STORAGE_PUT, 33ULL << 20, now + std::chrono::seconds{1}));
    BOOST_CHECK(budget.Admit("peer", Budget::Work::STORAGE_GET, 1, now + std::chrono::seconds{1}));
}

BOOST_AUTO_TEST_CASE(storage_transfer_lease_survives_budget_owner)
{
    std::shared_ptr<void> lease;
    {
        cybou::p2p::IngressBudget budget;
        lease = budget.AcquireStorageTransfer("peer");
        BOOST_REQUIRE(lease);
    }
    lease.reset();
}

BOOST_AUTO_TEST_CASE(block_decoder_rejects_unbounded_operation_count)
{
    auto encoded = cybou::SerializeBlock(cybou::CybouBlock{});
    BOOST_REQUIRE(encoded);
    BOOST_REQUIRE_EQUAL(encoded->size(), 76U);
    for (size_t i = 72; i < 76; ++i) (*encoded)[i] = 0xff;
    BOOST_CHECK(!cybou::DeserializeBlock(*encoded));
}

BOOST_AUTO_TEST_CASE(desktop_finalizer_worker_produces_blocks_and_stops_cleanly)
{
    CybouServiceTestFixture local;
    cybou::CybouNodeService service{{
        .runtime = cybou::NodeRuntimeConfig{
            .network_genesis = local.definition,
            .data_dir = local.directory / "desktop-finalizer-service",
            .poa_finalizer_recovery_entropy = local.validator_seed,
            .memory_only = true,
            .wipe_data = true,
            .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0
        },
        .genesis = local.genesis,
    }};
    service.Start();
    service.StartBlockProduction(10);
    BOOST_REQUIRE(SubmitAccountCreate(service.Runtime(), local.directory / "desktop-a.cybou"));
    BOOST_REQUIRE(SubmitAccountCreate(service.Runtime(), local.directory / "desktop-b.cybou"));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (service.Runtime().GetFinalizedHeight().value_or(0) < 2 &&
        std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    BOOST_REQUIRE_GE(service.Runtime().GetFinalizedHeight().value_or(0), 2U);
    service.Runtime().DisablePoaSigner();
    service.StopBlockProduction();
    const auto stopped_height = service.Runtime().GetFinalizedHeight().value_or(0);
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    BOOST_CHECK_EQUAL(service.Runtime().GetFinalizedHeight().value_or(0), stopped_height);
}

BOOST_AUTO_TEST_CASE(full_node_network_service_starts_without_an_initial_peer)
{
    CybouServiceTestFixture local;
    cybou::CybouNodeService service{{
        .runtime = cybou::NodeRuntimeConfig{
            .network_genesis = local.definition,
            .data_dir = local.directory / "peerless-observer-service",
            .memory_only = true,
            .wipe_data = true,
            .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0
        },
        .genesis = local.genesis,
    }};
    service.Start();

    std::mutex mutex;
    std::condition_variable changed;
    bool callback_seen{false};
    bool reports_waiting_for_peer{false};
    service.StartNetwork({.sync_interval = std::chrono::milliseconds{20}, .sync_batch_size = 1},
        [&](const cybou::SyncPeerResult& result, const cybou::NodeRuntimeStatus& status, size_t peers) {
            std::lock_guard lock{mutex};
            callback_seen = true;
            reports_waiting_for_peer = status.is_initialized && peers == 0 &&
                result.status == cybou::SyncPeerStatus::CONNECTION_FAILED;
            changed.notify_all();
            return true;
        });

    {
        std::unique_lock lock{mutex};
        BOOST_REQUIRE(changed.wait_for(lock, std::chrono::seconds{5}, [&] { return callback_seen; }));
    }
    service.StopNetwork();
    BOOST_CHECK(reports_waiting_for_peer);
}

BOOST_AUTO_TEST_CASE(configured_peer_is_not_eclipsed_by_newer_stale_hello)
{
    CybouServiceTestFixture primary;
    CybouServiceTestFixture secondary;
    CybouServiceTestFixture configured_source;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor first{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor second{io, tcp::endpoint{loopback, 0}};
    std::jthread first_server{[&] {
        tcp::socket socket{io}; first.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        if (session.Handshake({.network_binding=primary.runtime->GetNetworkBinding(),
                .finalized_height=0,.finalized_tip=primary.definition.GetGenesisAnchor(),
                .nonce=30001})) {
            while (session.ServeNext(*configured_source.runtime)) {}
        }
    }};
    std::optional<std::jthread> second_server;
    auto observer=std::make_unique<cybou::CybouNodeRuntime>(cybou::NodeRuntimeConfig{
        .network_genesis=primary.definition,.data_dir=primary.directory/"route-observer",
        .configured_peers={{std::make_pair(loopback.to_string(),first.local_endpoint().port())}},
        .memory_only=true,.wipe_data=true,
        .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0});
    BOOST_REQUIRE(observer->InitializeGenesis(primary.genesis));
    BOOST_CHECK(observer->SyncFromConfiguredPeer(2).status==cybou::SyncPeerStatus::UP_TO_DATE);
    // The original route advances, but its stored HELLO remains at height zero.
    const auto block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(block);
    BOOST_REQUIRE(static_cast<bool>(configured_source.runtime->CommitBlock(*block)));
    BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*block)));
    const auto second_block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(second_block);
    BOOST_REQUIRE(static_cast<bool>(configured_source.runtime->CommitBlock(*second_block)));
    second_server.emplace([&] {
        tcp::socket socket{io}; second.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        if (session.Handshake({.network_binding=secondary.runtime->GetNetworkBinding(),
                .finalized_height=1,.finalized_tip=cybou::ComputeBlockId(block->block),
                .nonce=30002})) {
            while (session.ServeNext(*secondary.runtime)) {}
        }
    });
    observer->SetConfiguredPeerEndpoints({{loopback.to_string(),second.local_endpoint().port()}});
    observer->SyncFromConfiguredPeer(2);
    // Choosing the later, higher HELLO would return only its one stale block.
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(99),2U);
    // A configured peer that stops advancing must not prevent failover.
    if (secondary.runtime->GetFinalizedHeight().value_or(0) < 2)
        BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*second_block)));
    const auto third_block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(third_block);
    BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*third_block)));
    observer->SyncFromConfiguredPeer(2);
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(99),3U);
    // Partial progress from the preferred route must not starve a fresher one.
    if (configured_source.runtime->GetFinalizedHeight().value_or(0) < 3)
        BOOST_REQUIRE(static_cast<bool>(configured_source.runtime->CommitBlock(*third_block)));
    const auto fourth_block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(fourth_block);
    BOOST_REQUIRE(static_cast<bool>(configured_source.runtime->CommitBlock(*fourth_block)));
    BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*fourth_block)));
    const auto fifth_block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(fifth_block);
    BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*fifth_block)));
    const auto progress=observer->SyncFromConfiguredPeer(2);
    BOOST_CHECK_EQUAL(progress.blocks_applied,2U);
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(99),5U);
    observer.reset();
    first_server.join(); second_server->join();
}

BOOST_AUTO_TEST_CASE(full_node_network_worker_recovers_after_peer_protocol_error)
{
    CybouServiceTestFixture local;
    CybouServiceTestFixture foreign{0x41};
    BOOST_REQUIRE(local.runtime->ProduceBlock());
    BOOST_REQUIRE(foreign.runtime->ProduceBlock());

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor recovery_acceptor{io, tcp::endpoint{loopback, 0}};
    std::atomic_bool peer_served_bad_block{false};
    std::atomic_bool peer_served_valid_block{false};
    std::jthread bad_peer{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const auto foreign_block = foreign.runtime->GetBlockAtHeight(1);
        if (foreign_block && session.Handshake({
                .network_binding = local.runtime->GetNetworkBinding(),
                .finalized_height = 1,
                .finalized_tip = cybou::ComputeBlockId(foreign_block->block),
                .nonce = 29001}) && session.ServeNext(*foreign.runtime)) {
            peer_served_bad_block.store(true);
        }
    }};
    std::jthread recovery_peer{[&] {
        tcp::socket socket{io};
        recovery_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const auto valid_block = local.runtime->GetBlockAtHeight(1);
        if (valid_block && session.Handshake({
                .network_binding = local.runtime->GetNetworkBinding(),
                .finalized_height = 1,
                .finalized_tip = cybou::ComputeBlockId(valid_block->block),
                .nonce = 29002}) && session.ServeNext(*local.runtime) && session.ServeNext(*local.runtime)) {
            peer_served_valid_block.store(true);
        }
    }};

    cybou::CybouNodeService service{{
        .runtime = cybou::NodeRuntimeConfig{
            .network_genesis = local.definition,
            .data_dir = local.directory / "service-observer",
            .configured_peers = {{std::make_pair(loopback.to_string(), acceptor.local_endpoint().port())}},
            .memory_only = true,
            .wipe_data = true,
            .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0
        },
        .genesis = local.genesis,
    }};
    service.Start();
    service.Runtime().SetConfiguredPeerEndpoints({
        {loopback.to_string(), acceptor.local_endpoint().port()},
        {loopback.to_string(), recovery_acceptor.local_endpoint().port()},
    });

    std::mutex mutex;
    std::condition_variable changed;
    size_t callbacks{0};
    bool malformed_peer_result_was_retryable{false};
    bool recovered{false};
    service.StartNetwork(
        {.sync_interval = std::chrono::milliseconds{25}, .sync_batch_size = 1, .listen_endpoint = std::nullopt},
        [&](const cybou::SyncPeerResult& result, const cybou::NodeRuntimeStatus& runtime_status, size_t) {
            {
                std::lock_guard lock{mutex};
                // The runtime isolates the peer's protocol error and exposes
                // the failed sync as retryable CONNECTION_FAILED to the worker.
                if (callbacks == 0) malformed_peer_result_was_retryable =
                    result.status == cybou::SyncPeerStatus::CONNECTION_FAILED;
                ++callbacks;
                recovered = recovered || runtime_status.finalized_height == 1;
            }
            changed.notify_all();
            return true;
        });

    {
        std::unique_lock lock{mutex};
        BOOST_REQUIRE(changed.wait_for(lock, std::chrono::seconds{10}, [&] { return callbacks >= 2 && recovered; }));
    }
    service.StopNetwork();
    bad_peer.join();
    recovery_peer.join();

    BOOST_CHECK(peer_served_bad_block.load());
    BOOST_CHECK(peer_served_valid_block.load());
    BOOST_CHECK(malformed_peer_result_was_retryable);
    BOOST_CHECK_GE(callbacks, 2U);
    BOOST_CHECK_EQUAL(service.Runtime().GetFinalizedHeight().value_or(99), 1U);
}

BOOST_AUTO_TEST_SUITE_END()
