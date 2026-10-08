// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#include <cybou/operation_submit.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/hex.h>
#include <cybou/identity_material.h>
#include <cybou/keystore.h>
#include <cybou/kv_store.h>
#include <cybou/name_service.h>
#include <cybou/block_executor.h>
#include <cybou/poa_finalizer.h>
#include <cybou/secret_file.h>
#include <cybou/identity_signer.h>
#include <cybou/observation_report.h>
#include <cybou/network_observation.h>
#include <cybou/storage_io_scheduler.h>
#include <future>
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

BOOST_AUTO_TEST_CASE(observation_cache_refreshes_without_identity_gui_or_diagnostics)
{
    cybou::NodeRuntimeConfig config{
        .network_genesis = cybou::CreateTestNetworkGenesis(cybou::CreateTestGenesisState(),
            cybou::TestPoaFinalizerPublicKey(31), cybou::TestNetworkPublicKey(31)),
        .data_dir = m_data_dir / "observation-runtime", .memory_only = true,
        .peer_admission_policy = TestPeerAdmissionPolicy()
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(cybou::CreateTestGenesisState()));
    cybou::ObservationBytes32 nonce{}; nonce.fill(37);
    cybou::ObservationReport report;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{8};
    do {
        report = cybou::DecodeObservationReport(runtime.ReadObservationReport(nonce));
        if (report.cursor.known && report.storage.known) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    } while (std::chrono::steady_clock::now() < deadline);
    BOOST_CHECK(report.cursor.known); BOOST_CHECK_EQUAL(report.cursor.height, 0U);
    BOOST_CHECK(report.storage.known); BOOST_CHECK_EQUAL(report.storage.capacity_bytes, uint64_t{15} << 30);
    BOOST_CHECK_EQUAL(report.storage.stored_mib, 0U); BOOST_CHECK_EQUAL(report.storage.provider_used_mib, 0U);
    BOOST_CHECK(!report.traffic.known && !report.cpu.known); // complete windows have not elapsed
    const auto snapshot = runtime.GetNetworkObservation();
    BOOST_CHECK(snapshot->local.cursor.known && snapshot->local.storage.known);
    BOOST_CHECK(!snapshot->remote_history.empty()); // background samples before any GUI read
    BOOST_CHECK_EQUAL(snapshot->local.storage.capacity_bytes, report.storage.capacity_bytes);
    BOOST_CHECK_EQUAL(snapshot->remote.fresh_remote_groups, 0U);
    BOOST_CHECK(report.challenge == nonce);
    BOOST_CHECK(cybou::Hash256{report.network_binding} == runtime.GetNetworkBinding());
}

BOOST_AUTO_TEST_CASE(observation_storage_idle_hint_rejects_active_work)
{
    cybou::StorageIoScheduler scheduler;
    const auto wait_for_idle = [&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
        do {
            if (scheduler.IsIdle()) return true;
            std::this_thread::yield();
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    };
    BOOST_CHECK(wait_for_idle()); // A contended mutex is conservatively reported as busy.
    std::promise<void> entered, release; auto gate = release.get_future();
    auto job = scheduler.Submit({}, cybou::StorageIoScheduler::Kind::READ, [&] {
        entered.set_value(); gate.wait(); return true;
    });
    const auto ready = entered.get_future().wait_for(std::chrono::seconds{2});
    const bool idle_during_job = scheduler.IsIdle();
    release.set_value(); BOOST_CHECK(job.get());
    BOOST_CHECK(ready == std::future_status::ready); BOOST_CHECK(!idle_during_job);
    BOOST_CHECK(wait_for_idle());
}

BOOST_AUTO_TEST_CASE(remote_history_preserves_gaps_cohorts_and_bounded_retention)
{
    using Clock = cybou::NetworkObservationHistory::Clock;
    const auto start = Clock::time_point{};
    cybou::NetworkObservationHistory history{start};
    cybou::p2p::ObservationGroupSnapshot group; group.cohort_revision = 1;
    group.storage.capacity_bytes = 0; group.storage.contributors = 1;
    history.Observe(group, start);
    history.Observe(group, start + std::chrono::seconds{1});
    auto first = history.Snapshot(start + std::chrono::seconds{1});
    BOOST_REQUIRE_EQUAL(first.size(), 1U); BOOST_REQUIRE(first[0].storage.capacity_bytes);
    BOOST_CHECK_EQUAL(*first[0].storage.capacity_bytes, 0U);
    group.cohort_revision = 2; group.storage.capacity_bytes.reset();
    history.Observe(group, start + std::chrono::seconds{5});
    group.storage.capacity_bytes = 17;
    history.Observe(group, start + std::chrono::seconds{20}); // no catch-up samples
    const auto gap = history.Snapshot(start + std::chrono::seconds{20});
    BOOST_REQUIRE_EQUAL(gap.size(), 3U);
    BOOST_CHECK(!gap[1].storage.capacity_bytes); BOOST_CHECK_EQUAL(gap[2].end_elapsed_ms, 20000U);
    BOOST_CHECK_EQUAL(gap[0].cohort_revision, 1U); BOOST_CHECK_EQUAL(gap[2].cohort_revision, 2U);
    BOOST_REQUIRE(first[0].storage.capacity_bytes); // returned snapshot remains immutable
    for (int n = 5; n <= 200; ++n) history.Observe(group, start + std::chrono::seconds{n * 5});
    const auto bounded = history.Snapshot(start + std::chrono::seconds{1000});
    BOOST_CHECK_LE(bounded.size(), 180U);
    BOOST_CHECK(history.Snapshot(start + std::chrono::seconds{1900}).empty());
    history.Observe(group, start + std::chrono::seconds{1900});
    BOOST_CHECK(history.Snapshot(start + std::chrono::seconds{1899}).empty());
    history.Observe(group, start + std::chrono::seconds{1900});
    BOOST_REQUIRE_EQUAL(history.Snapshot(start + std::chrono::seconds{1900}).size(), 1U);
}

BOOST_AUTO_TEST_CASE(network_observation_snapshot_is_immutable_and_runtime_scoped)
{
    CybouServiceTestFixture fixture;
    auto& runtime = *fixture.runtime;
    const auto empty = runtime.GetNetworkObservation();
    BOOST_CHECK(cybou::Hash256{empty->network_binding} == runtime.GetNetworkBinding());
    BOOST_CHECK_EQUAL(empty->remote.selected_remote_groups, 0U);
    BOOST_CHECK(!empty->remote.storage.capacity_bytes);
    cybou::p2p::ObservationSession session{817, empty->network_binding, true};
    const auto address = boost::asio::ip::make_address("192.168.1.9");
    auto groups = runtime.GetObservationGroups();
    BOOST_REQUIRE(groups->Select(session, address));
    cybou::ObservationReport report; report.network_binding = empty->network_binding;
    report.storage = {true, uint64_t{15} << 30, 17, uint64_t{10} << 30, 11};
    BOOST_REQUIRE(groups->Record(session, address, report));
    const auto populated = runtime.GetNetworkObservation();
    BOOST_CHECK_EQUAL(populated->remote.fresh_remote_groups, 1U);
    BOOST_REQUIRE(populated->remote.storage.provider_used_bytes);
    BOOST_CHECK_EQUAL(*populated->remote.storage.provider_used_bytes, uint64_t{11} << 20);
    BOOST_CHECK_EQUAL(empty->remote.fresh_remote_groups, 0U); // previous value never mutates
    groups->Close(session.handle);
    const auto closed = runtime.GetNetworkObservation();
    BOOST_CHECK_EQUAL(closed->remote.fresh_remote_groups, 0U);
    BOOST_CHECK(!closed->remote.storage.capacity_bytes);
    BOOST_CHECK_EQUAL(populated->remote.fresh_remote_groups, 1U);
    cybou::CybouNodeRuntime replacement{cybou::NodeRuntimeConfig{
        .network_genesis = fixture.definition, .data_dir = fixture.directory / "replacement-observer",
        .memory_only = true, .peer_admission_policy = TestPeerAdmissionPolicy()}};
    BOOST_CHECK_EQUAL(replacement.GetNetworkObservation()->remote.selected_remote_groups, 0U);
}

BOOST_AUTO_TEST_CASE(process_cpu_intervals_use_elapsed_time_and_reset_on_missing_data)
{
    cybou::ProcessCpuMeter meter;
    const auto start = cybou::ProcessCpuMeter::Clock::time_point{};
    BOOST_CHECK(!meter.Observe(cybou::ProcessCpuReading{0, 4}, start).interval_percent);
    BOOST_CHECK(!meter.Observe(cybou::ProcessCpuReading{0, 4}, start + std::chrono::microseconds{500}).interval_percent);
    const auto first = meter.Observe(cybou::ProcessCpuReading{4000000000ULL, 4}, start + std::chrono::seconds{2});
    BOOST_REQUIRE(first.interval_percent);
    BOOST_CHECK_CLOSE(*first.interval_percent, 50.0, 0.001);
    BOOST_CHECK(!first.mean_percent);
    const auto mean = meter.Observe(cybou::ProcessCpuReading{60000000000ULL, 4}, start + std::chrono::seconds{60});
    BOOST_REQUIRE(mean.mean_percent);
    BOOST_CHECK_CLOSE(*mean.mean_percent, 25.0, 0.001); // weighted elapsed time, not mean of interval percentages
    BOOST_CHECK_EQUAL(mean.mean_window_ms, 60000U);
    BOOST_CHECK_EQUAL(mean.mean_intervals, 2U);
    const auto idle = meter.Observe(cybou::ProcessCpuReading{60000000000ULL, 4}, start + std::chrono::seconds{62});
    BOOST_REQUIRE(idle.interval_percent);
    BOOST_CHECK_EQUAL(*idle.interval_percent, 0.0);
    BOOST_CHECK_EQUAL(idle.mean_age_ms, 2000U);
    BOOST_CHECK(!meter.Observe(std::nullopt, start + std::chrono::seconds{63}).mean_percent);
    BOOST_CHECK(!meter.Observe(cybou::ProcessCpuReading{60000000000ULL, 4}, start + std::chrono::seconds{65}).interval_percent);
    BOOST_CHECK(!meter.Observe(cybou::ProcessCpuReading{60000000000ULL, 2}, start + std::chrono::seconds{67}).interval_percent);
    BOOST_CHECK(!meter.Observe(cybou::ProcessCpuReading{1, 2}, start + std::chrono::seconds{68}).interval_percent);
    BOOST_CHECK(!meter.Observe(cybou::ProcessCpuReading{2, 2}, start + std::chrono::seconds{66}).interval_percent);
    BOOST_CHECK(!meter.Observe(cybou::ProcessCpuReading{2, 2}, start + std::chrono::seconds{66}).interval_percent);
#if defined(_WIN32) || defined(__linux__)
    const auto os = cybou::ReadProcessCpu();
    BOOST_REQUIRE(os);
    BOOST_CHECK_GT(os->processors, 0U);
#endif
}

BOOST_AUTO_TEST_CASE(observation_history_retains_complete_intervals_independently_of_reads)
{
    using Clock = cybou::TrafficMeter::Clock;
    const auto start = Clock::time_point{};
    cybou::TrafficMeter traffic{start};
    traffic.Record(10, 20, start);
    BOOST_CHECK(traffic.Snapshot(start + std::chrono::seconds{4}).history.empty());
    const auto initial = traffic.Snapshot(start + std::chrono::seconds{5}).history;
    BOOST_REQUIRE_EQUAL(initial.size(), 1U);
    BOOST_CHECK_EQUAL(initial.front().primary, 10U);
    BOOST_CHECK_EQUAL(initial.front().end_elapsed_ms, 5000U);
    traffic.Record(30, 40, start + std::chrono::seconds{901});
    const auto retained = traffic.Snapshot(start + std::chrono::seconds{904}).history;
    BOOST_REQUIRE_EQUAL(retained.size(), 180U);
    BOOST_CHECK_EQUAL(retained.front().primary, 10U); // partial tail must not erase the oldest complete interval
    BOOST_CHECK_EQUAL(retained.back().primary, 0U);
    const auto advanced = traffic.Snapshot(start + std::chrono::seconds{905}).history;
    BOOST_CHECK_EQUAL(advanced.front().primary, 0U);
    BOOST_CHECK_EQUAL(advanced.back().primary, 30U);
    traffic.Record(70, 80, start + std::chrono::seconds{906});
    traffic.Record(5, 6, start + std::chrono::seconds{1}); // delayed writer at colliding slot
    BOOST_CHECK_EQUAL(traffic.Snapshot(start + std::chrono::seconds{910}).history.back().primary, 70U);
    BOOST_CHECK(cybou::TrafficMeter{start + std::chrono::seconds{910}}.Snapshot(start + std::chrono::seconds{910}).history.empty());
    cybou::FinalizationMeter finalized{start};
    finalized.Record(3, cybou::BlockObservation::LOCAL_PRODUCTION, start);
    finalized.Record(2, cybou::BlockObservation::ANNOUNCEMENT, start);
    finalized.Record(1000, cybou::BlockObservation::HISTORY, start);
    const auto operations = finalized.Snapshot(start + std::chrono::seconds{5}).history;
    BOOST_REQUIRE_EQUAL(operations.size(), 1U);
    BOOST_CHECK_EQUAL(operations.front().primary, 5U);
    BOOST_CHECK_EQUAL(operations.front().secondary, 3U);
    finalized.Reset(start + std::chrono::seconds{5});
    BOOST_CHECK(finalized.Snapshot(start + std::chrono::seconds{9}).history.empty());
}

BOOST_AUTO_TEST_CASE(finalization_observation_windows_separate_history_and_reset)
{
    using Clock = cybou::FinalizationMeter::Clock;
    const auto start = Clock::time_point{};
    cybou::FinalizationMeter meter{start};
    meter.Record(3, cybou::BlockObservation::LOCAL_PRODUCTION, start);
    meter.Record(2, cybou::BlockObservation::ANNOUNCEMENT, start + std::chrono::seconds{59});
    meter.Record(1000, cybou::BlockObservation::HISTORY, start);
    BOOST_CHECK(!meter.Snapshot(start + std::chrono::seconds{59}).windows[0].complete);
    const auto minute = meter.Snapshot(start + std::chrono::seconds{60});
    BOOST_CHECK(minute.windows[0].complete);
    BOOST_CHECK_EQUAL(minute.windows[0].observed_operations, 5U);
    BOOST_CHECK_EQUAL(minute.windows[0].local_produced_operations, 3U);
    BOOST_CHECK_EQUAL(minute.windows[0].history_operations, 1000U);
    BOOST_CHECK(!minute.windows[1].complete);
    meter.Record(1, cybou::BlockObservation::ANNOUNCEMENT, start + std::chrono::seconds{60});
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{60}).windows[0].observed_operations, 5U);
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{61}).windows[0].observed_operations, 3U);
    const auto five = meter.Snapshot(start + std::chrono::seconds{300});
    BOOST_CHECK(five.windows[1].complete);
    BOOST_CHECK_EQUAL(five.windows[0].observed_operations, 0U);
    BOOST_CHECK_EQUAL(five.windows[1].observed_operations, 6U);
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{900}).windows[2].observed_operations, 6U);
    meter.Record(7, cybou::BlockObservation::ANNOUNCEMENT, start + std::chrono::seconds{901});
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{902}).windows[2].observed_operations, 10U);
    meter.Reset(start + std::chrono::seconds{902});
    const auto reset = meter.Snapshot(start + std::chrono::seconds{902});
    BOOST_CHECK(!reset.windows[0].complete);
    BOOST_CHECK_EQUAL(reset.observed_total, 0U);
    BOOST_CHECK_EQUAL(reset.history_total, 0U);
}

BOOST_AUTO_TEST_CASE(passive_traffic_windows_exclude_partial_seconds_and_expire)
{
    using Clock = cybou::TrafficMeter::Clock;
    const auto start = Clock::time_point{};
    cybou::TrafficMeter meter{start};
    meter.Record(120, 60, start);
    meter.Record(600, 1200, start + std::chrono::seconds{59});
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{59}).window_ms, 0U);
    const auto minute = meter.Snapshot(start + std::chrono::seconds{60});
    BOOST_CHECK_EQUAL(minute.window_ms, 60000U);
    BOOST_CHECK_EQUAL(minute.window_received_bytes, 720U);
    BOOST_CHECK_EQUAL(minute.window_sent_bytes, 1260U);
    meter.Record(1000, 2000, start + std::chrono::seconds{60});
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{60}).window_received_bytes, 720U);
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{61}).window_received_bytes, 1600U);
    const auto idle = meter.Snapshot(start + std::chrono::seconds{121});
    BOOST_CHECK_EQUAL(idle.window_received_bytes, 0U);
    BOOST_CHECK_EQUAL(idle.received_bytes, 1720U);
    meter.Record(7, 9, start + std::chrono::seconds{122});
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{123}).window_received_bytes, 7U);
    BOOST_CHECK_EQUAL(cybou::TrafficMeter{start + std::chrono::seconds{123}}.Snapshot(start + std::chrono::seconds{123}).window_ms, 0U);
    std::jthread a{[&] { for (int i = 0; i < 1000; ++i) meter.Record(1, 2, start + std::chrono::seconds{123}); }};
    std::jthread b{[&] { for (int i = 0; i < 1000; ++i) meter.Record(3, 4, start + std::chrono::seconds{123}); }};
    a.join(); b.join();
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{124}).received_bytes, 5727U);
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{124}).window_sent_bytes, 6009U);
    meter.Record(11, 13, start + std::chrono::seconds{62}); // delayed writer, same ring slot
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{124}).window_received_bytes, 4007U);
    BOOST_CHECK_EQUAL(meter.Snapshot(start + std::chrono::seconds{124}).received_bytes, 5738U);
}

BOOST_AUTO_TEST_CASE(finalized_snapshot_is_shared_immutable_and_reopens_from_disk)
{
    CybouServiceTestFixture fixture;
    auto& store = fixture.runtime->GetStore();
    const auto before = store.GetStateSnapshot();
    BOOST_REQUIRE(before);
    BOOST_CHECK(before.state == store.GetStateSnapshot().state);
    const auto before_bytes = cybou::SerializeCybouState(*before.state);
    const auto alice = fixture.CreateIdentity("snapshot-alice.cybou");
    const auto after = store.GetStateSnapshot();
    BOOST_REQUIRE(after);
    BOOST_CHECK(before.state != after.state);
    BOOST_CHECK(before_bytes == cybou::SerializeCybouState(*before.state));
    BOOST_CHECK(cybou::SerializeCybouState(*after.state) ==
        cybou::SerializeCybouState(*store.LoadState().state));

    cybou::CybouStateStore reopened{store.GetDatabase(), fixture.definition};
    const auto restored = reopened.GetStateSnapshot();
    BOOST_REQUIRE(restored);
    BOOST_CHECK(cybou::SerializeCybouState(*restored.state) ==
        cybou::SerializeCybouState(*after.state));

    const auto block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(block);
    BOOST_CHECK(!fixture.runtime->CommitBlock(*block));
    BOOST_CHECK(after.state == store.GetStateSnapshot().state);
    BOOST_REQUIRE(fixture.runtime->ProduceBlock()); // explicit empty block
    BOOST_CHECK(after.state == store.GetStateSnapshot().state);

    // Raw mutation simulates offline corruption; a new owner must fail closed.
    store.GetDatabase().Write(std::string{"cybou/hash"}, cybou::Hash256{});
    BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    cybou::CybouStateStore corrupt{store.GetDatabase(), fixture.definition};
    BOOST_CHECK(corrupt.GetStateSnapshot().error == cybou::StateLoadError::CORRUPT);
}

BOOST_AUTO_TEST_CASE(secret_files_are_private_and_reject_links)
{
    CybouServiceTestFixture fixture;
    const auto path = fixture.directory / "secret.bin";
    const std::array<unsigned char, 4> secret{1, 2, 3, 4};
    BOOST_REQUIRE(cybou::CreateSecretFile(path, secret));
    const auto read = cybou::ReadSecretFile(path, 4);
    BOOST_REQUIRE(read);
    BOOST_CHECK(*read == std::vector<unsigned char>(secret.begin(), secret.end()));
    BOOST_CHECK(!cybou::ReadSecretFile(path, secret.size() - 1));
    BOOST_CHECK(!cybou::ReadSecretFile(path, 0));
    BOOST_CHECK(!cybou::ReadSecretFile(path, (1U << 20) + 1));
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
    const auto detailed_path = fixture.directory / "detailed-events.jsonl";
    const cybou::EventFields fields{{"operation_id", std::string{"operation-secret"}},
        {"account_id", std::string{"account-secret"}}, {"peer", std::string{"peer-secret"}},
        {"network_binding", std::string{"public-network"}}};
    {
        cybou::EventWriter writer{minimal_path};
        writer.Write(cybou::NodeEvent::operation_accepted, fields);
    }
    {
        cybou::EventWriter writer{detailed_path, cybou::EventLogMode::DETAILED};
        writer.Write(cybou::NodeEvent::operation_accepted, fields);
    }
    std::ifstream minimal_file{minimal_path};
    std::ifstream detailed_file{detailed_path};
    const std::string minimal{std::istreambuf_iterator<char>{minimal_file}, {}};
    const std::string detailed{std::istreambuf_iterator<char>{detailed_file}, {}};
    BOOST_CHECK(minimal.find("operation-secret") == std::string::npos);
    BOOST_CHECK(minimal.find("account-secret") == std::string::npos);
    BOOST_CHECK(minimal.find("peer-secret") == std::string::npos);
    BOOST_CHECK(minimal.find("public-network") != std::string::npos);
    BOOST_CHECK(detailed.find("operation-secret") != std::string::npos);
    BOOST_CHECK(detailed.find("account-secret") != std::string::npos);
    BOOST_CHECK(detailed.find("peer-secret") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(public_event_writer_rejects_secret_fields)
{
    CybouServiceTestFixture fixture;
    const auto path = fixture.directory / "events.jsonl";
    cybou::EventWriter writer{path};
    BOOST_CHECK_THROW(writer.Write(cybou::NodeEvent::node_started, {{"mnemonic", std::string{"secret"}}}), std::invalid_argument);
    BOOST_CHECK_THROW(writer.Write(cybou::NodeEvent::node_started, {{"role", std::string(257, 'x')}}), std::invalid_argument);
    writer.Write(cybou::NodeEvent::node_status, {{"network_binding", fixture.runtime->GetNetworkBinding().GetHex()},{"height",std::uint64_t{1}}});
    BOOST_CHECK(writer.Good());
    const auto snapshot=fixture.runtime->GetDiagnostics();
    BOOST_CHECK(snapshot.initialized);
    BOOST_CHECK_EQUAL(snapshot.node_type, "Full Node");
    BOOST_CHECK_GT(snapshot.observed_unix_ms, 0U);
    const auto next = fixture.runtime->GetDiagnostics();
    BOOST_CHECK_GE(next.uptime_ms, snapshot.uptime_ms);
    BOOST_CHECK_EQUAL(next.pending_operations, 0U);
    BOOST_CHECK_EQUAL(next.pending_operation_bytes, 0U);
    BOOST_CHECK_EQUAL(snapshot.height,fixture.runtime->GetStatus().finalized_height);
}

BOOST_AUTO_TEST_CASE(event_writer_records_node_status_only_on_change)
{
    CybouServiceTestFixture fixture;
    const auto path = fixture.directory / "status-events.jsonl";
    {
        cybou::EventWriter writer{path};
        auto snapshot = fixture.runtime->GetDiagnostics();
        for (int tick = 0; tick < 5; ++tick) writer.Observe(snapshot);
        snapshot.height += 1;
        writer.Observe(snapshot);
        writer.Observe(snapshot);
        BOOST_CHECK(writer.Good());
    }
    std::ifstream file{path};
    size_t status_lines{0};
    for (std::string line; std::getline(file, line);) {
        if (line.find("\"event\":\"node_status\"") != std::string::npos) ++status_lines;
    }
    BOOST_CHECK_EQUAL(status_lines, 2U);
}

BOOST_AUTO_TEST_CASE(runtime_finalizes_account_and_observer_verifies_block)
{
    CybouServiceTestFixture fixture;
    const auto status = fixture.runtime->GetStatus();
    BOOST_CHECK(status.is_initialized);
    BOOST_CHECK(status.poa_signer_active);
    const auto alice = fixture.CreateIdentity("alice.cybou");
    const auto account = alice->GetAccountId();
    BOOST_REQUIRE(account);
    BOOST_CHECK(fixture.runtime->GetAccountState(*account));
    const auto block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(block);

    cybou::NodeRuntimeConfig observer_config{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "observer",
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(!observer.GetStatus().poa_signer_active);
    BOOST_REQUIRE(observer.CommitBlock(*block));
    BOOST_CHECK_EQUAL(observer.GetDiagnostics().finalization.history_total, 1U);
    BOOST_CHECK_EQUAL(observer.GetDiagnostics().finalization.observed_total, 0U);
    BOOST_CHECK(!observer.CommitBlock(*block, true, cybou::BlockObservation::ANNOUNCEMENT));
    BOOST_CHECK_EQUAL(observer.GetDiagnostics().finalization.history_total, 1U);
    BOOST_CHECK_EQUAL(fixture.runtime->GetDiagnostics().finalization.local_produced_total,
        fixture.runtime->GetDiagnostics().finalization.observed_total);
    BOOST_CHECK_GT(fixture.runtime->GetDiagnostics().finalization.local_produced_total, 0U);
    cybou::CybouNodeRuntime announcement_observer{{.network_genesis = fixture.definition,
        .data_dir = fixture.directory / "announcement-observer", .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0}};
    BOOST_REQUIRE(announcement_observer.InitializeGenesis(fixture.genesis));
    BOOST_REQUIRE(announcement_observer.CommitBlock(*block, true, cybou::BlockObservation::ANNOUNCEMENT));
    BOOST_CHECK_EQUAL(announcement_observer.GetDiagnostics().finalization.observed_total, 1U);
    BOOST_CHECK_EQUAL(announcement_observer.GetDiagnostics().finalization.local_produced_total, 0U);
    BOOST_CHECK_EQUAL(announcement_observer.GetDiagnostics().finalization.history_total, 0U);
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
    const auto missing_id = *op_id == cybou::Hash256::ONE ? cybou::Hash256{uint8_t{2}} : cybou::Hash256::ONE;
    const auto missing = observer.FindFinalizedOperation(missing_id);
    BOOST_CHECK(missing.status == cybou::FinalizedOperationLookupStatus::NOT_FOUND);
    BOOST_CHECK_EQUAL(missing.scanned_height, 1U);
}

BOOST_AUTO_TEST_CASE(ordinary_node_executes_candidates_before_relay)
{
    CybouServiceTestFixture fixture;
    const auto alice = fixture.CreateIdentity("alice.cybou");
    cybou::CybouNameService names{*fixture.runtime, alice->GetKeyStore(), fixture.directory / "alice.cybou"};
    const auto claimed = names.ClaimSync("alice", "correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(claimed.success, claimed.message);
    std::optional<cybou::ProtocolOperation> commit, reveal;
    uint64_t commit_height{0};
    for (uint64_t height{2}; height <= fixture.runtime->GetFinalizedHeight().value_or(0); ++height) {
        const auto block = fixture.runtime->GetBlockAtHeight(height);
        BOOST_REQUIRE(block);
        for (const auto& operation : block->block.operations) {
            if (std::holds_alternative<cybou::AuthorizedNameCommit>(operation)) { commit = operation; commit_height = height; }
            if (std::holds_alternative<cybou::AuthorizedNameReveal>(operation)) reveal = operation;
        }
    }
    BOOST_REQUIRE(commit && reveal);

    cybou::CybouNodeRuntime ordinary{{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "ordinary-candidates",
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    }};
    BOOST_REQUIRE(ordinary.InitializeGenesis(fixture.genesis));
    for (uint64_t height{1}; height < commit_height; ++height) {
        BOOST_REQUIRE(ordinary.CommitBlock(*fixture.runtime->GetBlockAtHeight(height)));
    }
    const auto commit_bytes = cybou::SerializeProtocolOperation(*commit);
    const auto reveal_bytes = cybou::SerializeProtocolOperation(*reveal);
    const auto commit_id = cybou::ComputeOperationId(*commit);
    const auto reveal_id = cybou::ComputeOperationId(*reveal);
    BOOST_REQUIRE(commit_bytes && reveal_bytes && commit_id && reveal_id);

    // Correctly signed but not executable on this node's finalized state: never relayed.
    BOOST_CHECK(ordinary.EnqueueRelayedOperation(*reveal_bytes, 0) == cybou::OperationRelayEnqueueStatus::INVALID_OPERATION);
    BOOST_CHECK(!ordinary.HasRelayedOperation(*reveal_id));
    BOOST_CHECK(!ordinary.HasCandidateOperation(*reveal_id));

    BOOST_CHECK(ordinary.EnqueueRelayedOperation(*commit_bytes, 0) == cybou::OperationRelayEnqueueStatus::QUEUED);
    BOOST_CHECK(ordinary.HasRelayedOperation(*commit_id));
    BOOST_CHECK(ordinary.HasCandidateOperation(*commit_id));
    const auto queued = ordinary.GetDiagnostics();
#if defined(_WIN32) || defined(__linux__)
    BOOST_REQUIRE(queued.process_resident_bytes.has_value());
    BOOST_CHECK_GT(*queued.process_resident_bytes, 0U);
#else
    BOOST_CHECK(!queued.process_resident_bytes.has_value());
#endif
    BOOST_CHECK_EQUAL(queued.pending_operations, 1U);
    BOOST_CHECK_EQUAL(queued.pending_operation_bytes, commit_bytes->size());
    // An ordinary node's own pool is not finalizer acceptance.
    BOOST_CHECK(ordinary.GetOperationStatus(*commit_id).kind != cybou::OperationStatusKind::LOCAL_PENDING);

    BOOST_REQUIRE(ordinary.CommitBlock(*fixture.runtime->GetBlockAtHeight(commit_height)));
    BOOST_CHECK(!ordinary.HasCandidateOperation(*commit_id));
    BOOST_CHECK(!ordinary.HasRelayedOperation(*commit_id));
    BOOST_CHECK_EQUAL(ordinary.CandidateOperationCount(), 0U);
    const auto drained = ordinary.GetDiagnostics();
    BOOST_CHECK_EQUAL(drained.pending_operations, 0U);
    BOOST_CHECK_EQUAL(drained.pending_operation_bytes, 0U);
    BOOST_CHECK(ordinary.EnqueueRelayedOperation(*commit_bytes, 0) == cybou::OperationRelayEnqueueStatus::DUPLICATE);
}

BOOST_AUTO_TEST_CASE(runtime_finalizer_can_be_armed_and_disarmed_with_a_vault_signer)
{
    CybouServiceTestFixture fixture;
    cybou::CybouNodeRuntime runtime{cybou::NodeRuntimeConfig{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "vault-finalizer-runtime",
        .memory_only = true,
        .wipe_data = true,
        .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0
    }};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(!runtime.GetStatus().poa_signer_active);
    BOOST_CHECK(!runtime.ProduceBlock());

    auto material = cybou::GenerateIdentityMaterial();
    BOOST_REQUIRE(material);
    cybou::crypto::CleanseMemory(material->recovery_entropy.data(), material->recovery_entropy.size());
    material->recovery_entropy = fixture.validator_seed;
    cybou::CybouKeyStore keystore;
    BOOST_REQUIRE(keystore.LoadMaterial(std::move(*material)));
    auto signer = std::make_shared<cybou::CybouKeyStorePoaSigner>(keystore);
    BOOST_REQUIRE(runtime.EnablePoaSigner(signer));
    BOOST_CHECK(runtime.GetStatus().poa_signer_active);
    BOOST_REQUIRE(runtime.ProduceBlock());

    runtime.DisablePoaSigner();
    BOOST_CHECK(!runtime.GetStatus().poa_signer_active);
    BOOST_CHECK(!runtime.ProduceBlock());

    cybou::CybouKeyStore wrong_keystore;
    BOOST_REQUIRE(wrong_keystore.GenerateNew());
    auto wrong_signer = std::make_shared<cybou::CybouKeyStorePoaSigner>(wrong_keystore);
    BOOST_CHECK(!runtime.EnablePoaSigner(wrong_signer));
    BOOST_CHECK(!runtime.GetStatus().poa_signer_active);
}

BOOST_AUTO_TEST_CASE(runtime_resolves_valid_poa_equivocation_deterministically)
{
    CybouServiceTestFixture fixture;
    const auto canonical = fixture.runtime->ProduceBlock();
    BOOST_REQUIRE(canonical);

    // Both competing certificates must cover independently executable blocks.
    // Merely changing state_root produces an invalid block, regardless of its ID.
    CybouServiceTestFixture alternate{fixture.validator_seed[0]};
    auto identity = alternate.CreateIdentity("equivocation.cybou");
    const auto conflicting = alternate.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(conflicting);
    const auto& conflicting_block = conflicting->block;
    const auto previous_snapshot = fixture.runtime->GetStore().GetStateSnapshot();
    const auto previous_bytes = cybou::SerializeCybouState(*previous_snapshot.state);
    const auto result = fixture.runtime->CommitBlock(*conflicting);
    const bool conflicting_wins = cybou::ComputeBlockId(conflicting_block) < cybou::ComputeBlockId(canonical->block);
    if (conflicting_wins) {
        BOOST_CHECK(result.error == cybou::BlockTransitionError::NONE);
        BOOST_CHECK_EQUAL(fixture.runtime->GetDiagnostics().finalization.observed_total, 0U);
        BOOST_CHECK(fixture.runtime->GetFinalizedTip() == cybou::ComputeBlockId(conflicting_block));
    } else {
        BOOST_CHECK(result.error == cybou::BlockTransitionError::POA_EQUIVOCATION_DETECTED);
        BOOST_CHECK(fixture.runtime->GetFinalizedTip() == cybou::ComputeBlockId(canonical->block));
    }
    const auto current_snapshot = fixture.runtime->GetStore().GetStateSnapshot();
    BOOST_REQUIRE(current_snapshot);
    BOOST_CHECK(cybou::SerializeCybouState(*current_snapshot.state) ==
        cybou::SerializeCybouState(*fixture.runtime->GetStore().LoadState().state));
    BOOST_CHECK(previous_bytes == cybou::SerializeCybouState(*previous_snapshot.state));
    BOOST_CHECK((current_snapshot.state != previous_snapshot.state) == conflicting_wins);
    const auto status = fixture.runtime->GetStatus();
    BOOST_CHECK(!status.poa_safety_halted);
    BOOST_CHECK(status.runtime_state != cybou::NodeRuntimeState::SAFETY_HALTED);
    // A signer whose durable journal belongs to the losing history must fail closed.
    BOOST_CHECK(fixture.runtime->ProduceBlock().has_value() == !conflicting_wins);
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
    const auto digest = cybou::ComputeIdentityOperationDigest(fixture.runtime->GetNetworkBinding(), auth);
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
    BOOST_CHECK(!fixture.runtime->FindFinalizedRootPublication(cybou::Hash256::ONE));
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
        std::span<const unsigned char, 32>{fixture.runtime->GetNetworkBinding().begin(), 32},
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
    auto foreign_genesis = cybou::CreateTestGenesisState();
    ++foreign_genesis.settlement.next_period_start_utc;
    cybou::NodeRuntimeConfig config{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "foreign-observer",
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_CHECK(!observer.InitializeGenesis(foreign_genesis));
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    const auto foreign_definition = cybou::CreateTestNetworkGenesis(foreign_genesis, cybou::TestPoaFinalizerPublicKey(0xBC), cybou::TestNetworkPublicKey(0xBC));
    cybou::NodeRuntimeConfig foreign_config{
        .network_genesis = foreign_definition,
        .data_dir = fixture.directory / "foreign-producer",
        .poa_finalizer_recovery_entropy = foreign_seed,
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
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
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "peer-priority",
        .advertised_endpoint = std::make_pair("127.0.0.1", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));

    // Operator-approved validator endpoints.
    runtime.SetConfiguredPeerEndpoints({{"10.0.0.10", 8333}, {"10.0.0.11", 8333}});
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
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "peer-policy",
        .advertised_endpoint = std::make_pair("203.0.113.5", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    runtime.SetConfiguredPeerEndpoints({
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
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "private-peer-policy",
        .advertised_endpoint = std::make_pair("10.1.1.1", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
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

BOOST_AUTO_TEST_CASE(sync_completion_is_advisory_for_ordinary_peers)
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
        const bool handshake = fixture.HandshakeAsPeer(session, {.network_binding = fixture.runtime->GetNetworkBinding(),
            .finalized_height = 1, .finalized_tip = *tip,
            .nonce = 1301});
        finalizer_served = handshake;
        for (int i = 0; i < 5 && finalizer_served; ++i) finalizer_served = session.ServeNext(*fixture.runtime);
    }};

    cybou::NodeRuntimeConfig observer_config{.network_genesis = fixture.definition,
        .data_dir = fixture.directory / "finalizer-tip-observer",
        .configured_peers = {{std::make_pair(loopback.to_string(), finalizer_port)}},
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0};
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    const auto finalizer_sync = observer.SyncFromConfiguredPeer(10);
    finalizer_server.join();
    BOOST_CHECK(finalizer_served.load());
    BOOST_CHECK_EQUAL(finalizer_sync.blocks_applied, 1U);
    BOOST_CHECK(finalizer_sync.caught_up_with_known_peers);

    tcp::acceptor storage_acceptor{io, tcp::endpoint{loopback, 0}};
    const auto storage_port = storage_acceptor.local_endpoint().port();
    std::atomic_bool storage_served{false};
    std::jthread storage_server{[&] {
        tcp::socket socket{io};
        storage_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const bool handshake = session.Handshake({.network_binding = fixture.runtime->GetNetworkBinding(),
            .finalized_height = 1, .finalized_tip = *tip,
            .nonce = 1302});
        storage_served = handshake;
        for (int i = 0; i < 5 && storage_served; ++i) storage_served = session.ServeNext(*fixture.runtime);
    }};

    cybou::NodeRuntimeConfig storage_observer_config{.network_genesis = fixture.definition,
        .data_dir = fixture.directory / "provider-tip-observer",
        .configured_peers = {{std::make_pair(loopback.to_string(), storage_port)}},
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0};
    cybou::CybouNodeRuntime storage_observer{std::move(storage_observer_config)};
    BOOST_REQUIRE(storage_observer.InitializeGenesis(fixture.genesis));
    const auto storage_sync = storage_observer.SyncFromConfiguredPeer(10);
    storage_server.join();
    BOOST_CHECK(storage_served.load());
    BOOST_CHECK_EQUAL(storage_sync.blocks_applied, 1U);
    BOOST_CHECK(storage_sync.caught_up_with_known_peers);
}


BOOST_AUTO_TEST_CASE(finalized_event_index_rebuilds_and_refuses_missing_source)
{
    CybouServiceTestFixture fixture;
    const auto identity = fixture.CreateIdentity("event-index.cybou");
    const auto account = *identity->GetAccountId();
    const auto block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(block);
    const auto op = *cybou::ComputeOperationId(block->block.operations.front());
    auto& store = fixture.runtime->GetStore();
    auto& db = store.GetDatabase();
    const auto root = store.GetStateRoot();
    for (int i = 0; i < 12; ++i) BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    const auto scan = fixture.runtime->ScanFinalizedPublications(0, 13, 1);
    BOOST_REQUIRE(scan);
    BOOST_CHECK(scan->heights.empty());
    BOOST_CHECK_EQUAL(scan->scanned_height, 13U);
    db.Write(std::string{"cybou/events/op/"} + op.GetHex(), std::string{"truncated"});
    BOOST_CHECK(fixture.runtime->FindFinalizedOperation(op).status == cybou::FinalizedOperationLookupStatus::FOUND);
    const auto kem_key = std::string{"cybou/events/kem/"} + account.Value().GetHex() + "/0000000000000000";
    db.Erase(kem_key);
    BOOST_CHECK(fixture.runtime->FindIdentityKemPackage(account, 0).status == cybou::IdentityKemPackageLookupStatus::FOUND);
    // The same committed history supports a freshly opened store without its index marker.
    db.Erase(std::string{"cybou/events/head"});
    cybou::CybouStateStore reopened{db, fixture.definition};
    const auto found = reopened.FindIndexedOperation(op);
    BOOST_REQUIRE(found.available && found.location);
    BOOST_CHECK_EQUAL(found.location->height, 1U);
    BOOST_CHECK(store.GetStateRoot() == root);
    BOOST_CHECK(reopened.FindIndexedKem(account, 0).location.has_value());
    // Cache coordinates cannot make a missing source block into finalized evidence.
    db.Erase(std::string{"cybou/block/"} + cybou::ComputeBlockId(block->block).GetHex());
    BOOST_CHECK(fixture.runtime->FindFinalizedOperation(op).status == cybou::FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE);
    BOOST_CHECK(!reopened.RebuildFinalizedEventIndex());
    BOOST_CHECK(!db.Exists(std::string{"cybou/events/head"}));
    BOOST_CHECK(store.GetStateRoot() == root);
}

BOOST_AUTO_TEST_CASE(canonical_replacement_erases_losing_event_coordinates)
{
    CybouServiceTestFixture first;
    CybouServiceTestFixture second{first.validator_seed[0]};
    const auto alice = first.CreateIdentity("event-conflict-a.cybou");
    const auto bob = second.CreateIdentity("event-conflict-b.cybou");
    const auto a = first.runtime->GetBlockAtHeight(1);
    const auto b = second.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(a && b);
    const bool a_wins = cybou::ComputeBlockId(a->block) < cybou::ComputeBlockId(b->block);
    const auto& winner = a_wins ? *a : *b;
    const auto& loser = a_wins ? *b : *a;
    const auto losing_account = a_wins ? *bob->GetAccountId() : *alice->GetAccountId();
    const auto winning_op = *cybou::ComputeOperationId(winner.block.operations.front());
    const auto losing_op = *cybou::ComputeOperationId(loser.block.operations.front());
    CybouServiceTestFixture observer{first.validator_seed[0]};
    BOOST_REQUIRE(observer.runtime->CommitBlock(loser));
    BOOST_CHECK(observer.runtime->FindFinalizedOperation(losing_op).status == cybou::FinalizedOperationLookupStatus::FOUND);
    BOOST_REQUIRE(observer.runtime->CommitBlock(winner));
    auto& db = observer.runtime->GetStore().GetDatabase();
    BOOST_CHECK(!db.Exists(std::string{"cybou/events/op/"} + losing_op.GetHex()));
    BOOST_CHECK(!db.Exists(std::string{"cybou/events/kem/"} + losing_account.Value().GetHex() + "/0000000000000000"));
    BOOST_CHECK(observer.runtime->FindFinalizedOperation(losing_op).status == cybou::FinalizedOperationLookupStatus::NOT_FOUND);
    BOOST_CHECK(observer.runtime->FindFinalizedOperation(winning_op).status == cybou::FinalizedOperationLookupStatus::FOUND);
    BOOST_CHECK(observer.runtime->GetStore().RebuildFinalizedEventIndex());
    BOOST_CHECK(observer.runtime->FindFinalizedOperation(losing_op).status == cybou::FinalizedOperationLookupStatus::NOT_FOUND);
}

BOOST_AUTO_TEST_SUITE_END()
