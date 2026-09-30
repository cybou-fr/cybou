// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

// Multi-process storage soak client, driven by test/cybou_storage_soak.py.
//
// A light full node that reaches a real PoA finalizer and three real storage
// providers only over CYP2, under the Beta target (2 remote replicas). It
// walks the failure sequence that must hold before any Authority
// enforcement:
//
//   provider killed -> audit -> repair elsewhere
//   provider restarted -> same ProviderID
//   provider blob corrupted on disk -> audit drops it -> repair
//   finalizer restarted -> sync continues
//   app.db deleted -> rebuilt from history, placement from provider proofs
//   recovery phrase rotated (RecoveryBridge first) -> app.db rebuilt
//   clean restore on a fresh node with only the new phrase
//     -> every file back with exact bytes from providers
//
// The orchestrator acts on "REQUEST <n> <action> ..." lines and acknowledges
// by creating WORK_DIR/ack-<n>.

#include <cybou/application_service.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/encrypted_chunk_tree.h>
#include <cybou/identity_service.h>
#include <cybou/kv_store.h>
#include <cybou/network_definition.h>
#include <cybou/node_service.h>
#include <cybou/private_application_store.h>
#include <cybou/publication_service.h>
#include <cybou/recovery_phrase.h>
#include <cybou/storage_service.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

void Step(const std::string& text) { std::cout << text << std::endl; }

[[noreturn]] void Fail(const std::string& text)
{
    std::cout << "FAIL " << text << std::endl;
    std::exit(1);
}

void WaitFor(const std::string& what, const std::function<bool()>& done, std::chrono::seconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline) Fail("timeout: " + what);
        std::this_thread::sleep_for(250ms);
    }
}

std::string Hex(std::span<const unsigned char> bytes)
{
    static constexpr char DIGITS[]{"0123456789abcdef"};
    std::string out;
    for (const auto b : bytes) {
        out.push_back(DIGITS[b >> 4]);
        out.push_back(DIGITS[b & 0x0f]);
    }
    return out;
}

/** Asks the orchestrator to act and waits for its acknowledgement. */
class Orchestrator {
public:
    explicit Orchestrator(std::filesystem::path work) : m_work{std::move(work)} {}
    void Request(const std::string& action)
    {
        const int n = ++m_next;
        Step("REQUEST " + std::to_string(n) + " " + action);
        WaitFor("orchestrator: " + action, [&] { return std::filesystem::exists(m_work / ("ack-" + std::to_string(n))); },
            180s);
    }

private:
    std::filesystem::path m_work;
    int m_next{0};
};

std::vector<unsigned char> Pattern(std::size_t size, unsigned seed)
{
    std::vector<unsigned char> out(size);
    for (std::size_t i{0}; i < size; ++i) out[i] = static_cast<unsigned char>((i * 131 + seed) ^ (i >> 7));
    return out;
}

/** One Identity's Application DB and core services over a runtime. */
struct Services {
    cybou::CybouNodeRuntime& runtime;
    cybou::CybouKeyStore& keystore;
    std::filesystem::path dir;
    cybou::RuntimeStorageTransport transport;
    std::unique_ptr<cybou::PrivateApplicationStore> db;
    std::unique_ptr<cybou::KVStore> staging;
    std::unique_ptr<cybou::StorageService> storage;
    std::unique_ptr<cybou::PublicationService> publication;
    std::unique_ptr<cybou::ApplicationService> application;

    Services(cybou::CybouNodeRuntime& rt, cybou::CybouKeyStore& ks, std::filesystem::path identity_dir)
        : runtime{rt}, keystore{ks}, dir{std::move(identity_dir)}, transport{rt}
    {
        Open();
    }

    void Close()
    {
        application.reset();
        publication.reset();
        storage.reset();
        staging.reset();
        db.reset();
    }

    void Open()
    {
        Close();
        std::filesystem::create_directories(dir);
        try {
            db = std::make_unique<cybou::PrivateApplicationStore>(keystore, dir);
        } catch (const cybou::PrivateApplicationStoreKeyMismatch&) {
            // Encrypted under keys replaced by IdentityRotate: rebuild from history.
            std::filesystem::remove_all(dir / "app.db");
            db = std::make_unique<cybou::PrivateApplicationStore>(keystore, dir);
        }
        staging = std::make_unique<cybou::KVStore>(cybou::KVStoreOptions{.path = dir / "staging"});
        storage = std::make_unique<cybou::StorageService>(runtime, transport, *db, cybou::BETA_REMOTE_REPLICA_TARGET);
        publication = std::make_unique<cybou::PublicationService>(runtime, keystore, *db,
            runtime.GetIdentityOperationCoordinator(keystore), *staging);
        application = std::make_unique<cybou::ApplicationService>(runtime, keystore, *db, *storage);
    }

    cybou::PublicationJobPhase Phase(const std::string& job)
    {
        const auto status = publication->GetJob(job);
        return status ? status->phase : cybou::PublicationJobPhase::NEEDS_ATTENTION;
    }

    void WaitProtected(const std::string& job)
    {
        const auto deadline = std::chrono::steady_clock::now() + 240s;
        while (true) {
            publication->ProcessDurability(*storage);
            const auto status = publication->GetJob(job);
            if (status && status->phase == cybou::PublicationJobPhase::PROTECTED) return;
            if (std::chrono::steady_clock::now() > deadline) {
                Fail(job + " not protected: phase=" + (status ? std::to_string(static_cast<int>(status->phase)) : "none") +
                    " error=" + (status ? status->error : std::string{}) +
                    " op_status=" + (status ? std::to_string(static_cast<int>(
                        runtime.GetOperationStatus(status->operation_id).kind)) : "-"));
            }
            std::this_thread::sleep_for(250ms);
        }
    }

    /** Audits every placement until op is PROTECTED with no replica on excluded providers. */
    void WaitRepaired(const uint256& op, const std::set<std::array<unsigned char, 32>>& excluded, const std::string& what)
    {
        WaitFor(what, [&] {
            storage->AuditNextPlacement(64);
            const auto placement = storage->DescribePlacement(op);
            const auto durability = storage->GetDurability(op);
            if (!placement || !durability || durability->state != cybou::DurabilityState::PROTECTED) return false;
            for (const auto& replicas : placement->replicas) {
                for (const auto& replica : replicas) {
                    if (excluded.contains(replica.provider_id)) return false;
                }
            }
            return true;
        }, 240s);
    }

    cybou::PrivateItemId Upload(const std::string& job, const std::string& name, const std::vector<unsigned char>& bytes)
    {
        const auto item_id = *cybou::NewPrivateItemId();
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item_id,
            cybou::FileItem{.item_id = item_id, .kind = cybou::FileItemKind::FILE, .name = name}});
        auto offset = std::make_shared<std::size_t>(0);
        std::vector<std::pair<std::size_t, cybou::NewContent>> content;
        content.emplace_back(0, cybou::NewContent{[&bytes, offset](std::span<unsigned char> out) -> std::optional<std::size_t> {
            const auto n = std::min(out.size(), bytes.size() - *offset);
            std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(*offset), n, out.begin());
            *offset += n;
            return n;
        }});
        const auto submitted = publication->PublishFiles(job, batch, std::move(content));
        if (submitted.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) Fail(job + ": " + submitted.error);
        WaitProtected(job);
        Step("PROTECTED " + job);
        return item_id;
    }

    /** Downloads through StorageService (local cache or providers) and compares exact bytes. */
    void Verify(const cybou::PrivateItemId& item_id, const std::vector<unsigned char>& expected, const std::string& what)
    {
        WaitFor(what + " indexed", [&] { application->Scan(); return application->GetFile(item_id).has_value(); }, 120s);
        const auto file = application->GetFile(item_id);
        std::vector<unsigned char> downloaded;
        std::set<cybou::ChunkId> seen;
        const auto written = cybou::FetchEncryptedChunkTree(
            std::span<const unsigned char, 32>{runtime.GetNetworkId().begin(), 32}, *file->item.content_key,
            *file->item.root_chunk_id, [&](const cybou::ChunkId& id) { return storage->Fetch(id); },
            [](std::span<const unsigned char>) { return true; },
            [&](const cybou::ChunkId& id) { return seen.insert(id).second; },
            [&](std::span<const unsigned char> data) {
                downloaded.insert(downloaded.end(), data.begin(), data.end());
                return true;
            }, expected.size());
        if (!written || downloaded != expected) Fail(what + ": bytes differ");
        Step("VERIFIED " + what + " bytes=" + std::to_string(downloaded.size()));
    }
};

std::unique_ptr<cybou::CybouNodeService> StartNode(const cybou::CybouNetworkFile& network,
    const std::filesystem::path& data_dir, const std::string& ip, std::uint16_t port)
{
    auto node = std::make_unique<cybou::CybouNodeService>(cybou::CybouNodeServiceConfig{
        .runtime = cybou::NodeRuntimeConfig{.network_definition = network.definition,
            .data_dir = data_dir, .p2p_endpoint = std::make_pair(ip, port)},
        .genesis = network.genesis,
    });
    node->Start();
    node->StartNetwork(cybou::CybouNetworkServiceConfig{.sync_interval = 500ms},
        [](const cybou::SyncPeerResult&, const cybou::NodeRuntimeStatus&, std::size_t) { return true; });
    return node;
}

std::set<std::array<unsigned char, 32>> ProviderIds(cybou::CybouNodeRuntime& runtime)
{
    std::set<std::array<unsigned char, 32>> ids;
    for (const auto& peer : runtime.StoragePeerEndpoints()) ids.insert(peer.provider_id);
    return ids;
}

std::optional<std::uint16_t> PortOf(cybou::CybouNodeRuntime& runtime, const std::array<unsigned char, 32>& id)
{
    for (const auto& peer : runtime.StoragePeerEndpoints()) {
        if (peer.provider_id == id) return peer.port;
    }
    return std::nullopt;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 5) {
        std::cerr << "usage: cybou-storage-soak NETWORK_FILE WORK_DIR FINALIZER_IP FINALIZER_P2P_PORT\n";
        return 2;
    }
    try {
        const std::filesystem::path work{argv[2]};
        std::filesystem::create_directories(work);
        const auto network = cybou::LoadCybouNetworkFile(argv[1]);
        if (!network) Fail("invalid network file");
        const std::string ip{argv[3]};
        const auto port = static_cast<std::uint16_t>(std::stoul(argv[4]));
        Orchestrator orchestrator{work};

        auto node = StartNode(*network, work / "node-a", ip, port);
        auto& runtime = node->Runtime();
        WaitFor("verified sync", [&] { return runtime.GetFinalizedHeight().value_or(0) > 0; }, 60s);
        cybou::CybouIdentityService identity{runtime, work / "identity.vault"};
        const std::string password{"soak test vault password"};
        if (!identity.PrepareNewIdentity()) Fail("cannot prepare identity");
        const auto created = identity.CreateIdentitySync(password, nullptr, 120s);
        if (!created.success) Fail("identity creation: " + created.error_message);
        Step("IDENTITY finalized");
        WaitFor("three storage providers", [&] { return ProviderIds(runtime).size() >= 3; }, 120s);
        Step("PROVIDERS " + std::to_string(ProviderIds(runtime).size()));

        const auto account = *identity.GetAccountId();
        Services services{runtime, identity.GetKeyStore(), cybou::IdentityDataDirectory(work / "node-a", account)};
        const auto bytes_a = Pattern(900 * 1024, 7);
        const auto file_a = services.Upload("soak-a", "a.bin", bytes_a);
        const auto op_a = services.publication->GetJob("soak-a")->operation_id;

        // 1. Kill a replica holder: audit detects, repairs on another provider.
        auto placement = services.storage->DescribePlacement(op_a);
        const auto victim = placement->replicas.front().front().provider_id;
        const auto victim_port = PortOf(runtime, victim);
        if (!victim_port) Fail("holder is not connected");
        orchestrator.Request("KILL " + std::to_string(*victim_port));
        services.WaitRepaired(op_a, {victim}, "repair after provider loss");
        Step("REPAIRED after kill");

        // 2. Restart it: it comes back under the same ProviderID (persisted provider key).
        orchestrator.Request("START " + std::to_string(*victim_port));
        WaitFor("restarted provider reconnects with its ProviderID", [&] {
            return ProviderIds(runtime).contains(victim) && PortOf(runtime, victim) == victim_port;
        }, 180s);
        Step("RESTARTED same ProviderID");

        // 3. Corrupt one replica on a provider's disk: audit drops it and repairs.
        services.Close();
        services.Open();
        placement = services.storage->DescribePlacement(op_a);
        const auto corrupt_leaf = placement->leaves.back();
        const auto corrupt_holder = placement->replicas.back().front().provider_id;
        orchestrator.Request("CORRUPT " + std::to_string(*PortOf(runtime, corrupt_holder)) + " " + Hex(corrupt_leaf));
        // The damaged copy no longer verifies...
        const auto holder_endpoint = [&] {
            for (const auto& r : placement->replicas.back()) {
                if (r.provider_id == corrupt_holder) return r;
            }
            Fail("corrupted holder not in placement");
        }();
        const auto valid_from = [&](const cybou::StorageEndpoint& provider) {
            const auto bytes = services.transport.Get(provider, corrupt_leaf);
            return bytes && cybou::ComputeChunkId(*bytes) == corrupt_leaf;
        };
        if (valid_from(holder_endpoint)) Fail("corruption did not take effect");
        // ...the audit drops it and repairs (elsewhere, or by a valid PUT that
        // heals the damaged blob): every recorded replica serves valid bytes.
        WaitFor("audit repair after corruption", [&] {
            services.storage->AuditNextPlacement(1024);
            const auto now = services.storage->DescribePlacement(op_a);
            return services.storage->GetDurability(op_a)->state == cybou::DurabilityState::PROTECTED &&
                std::all_of(now->replicas.back().begin(), now->replicas.back().end(), valid_from);
        }, 240s);
        Step("REPAIRED after corruption");

        // 4. Restart the finalizer: finality keeps advancing for this node.
        const auto before_restart = runtime.GetFinalizedHeight().value_or(0);
        orchestrator.Request("RESTART_FINALIZER");
        WaitFor("finality after finalizer restart", [&] {
            return runtime.GetFinalizedHeight().value_or(0) > before_restart + 3;
        }, 180s);
        Step("FINALIZER restarted");

        // 5. Delete app.db: the projection and placements are rebuilt.
        services.Close();
        std::filesystem::remove_all(services.dir / "app.db");
        services.Open();
        WaitFor("rebuild after app.db loss", [&] {
            return services.application->Scan().Complete() && services.application->GetFile(file_a).has_value() &&
                services.storage->GetDurability(op_a) &&
                services.storage->GetDurability(op_a)->state == cybou::DurabilityState::PROTECTED;
        }, 240s);
        Step("REBUILT app.db");

        // 6. Rotate the recovery phrase, bridge first, then rebuild under the new keys.
        auto next_entropy = cybou::GenerateRecoveryEntropy();
        if (!next_entropy) Fail("rng");
        const auto next_words = cybou::EncodeRecoveryWords(*next_entropy);
        const std::span<const unsigned char, 32> next{next_entropy->data(), 32};
        const auto bridge = services.publication->PublishRecoveryBridge("soak-bridge", next);
        if (bridge.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) Fail("bridge: " + bridge.error);
        services.WaitProtected("soak-bridge");
        if (!services.publication->VerifyRecoveryBridge("soak-bridge", next, *services.storage)) Fail("bridge not verified");
        auto rotated = identity.RotateIdentitySync(next_words, password);
        WaitFor("rotation finality", [&] {
            if (rotated.phase == cybou::IdentityOperationPhase::FINALIZED) return true;
            if (!rotated) Fail("rotation: " + rotated.error);
            rotated = identity.ResumeIdentityRotationSync(password);
            return rotated.phase == cybou::IdentityOperationPhase::FINALIZED;
        }, 180s);
        Step("ROTATED");
        services.Open(); // key mismatch -> rebuilt from history + RecoveryBridge
        services.Verify(file_a, bytes_a, "pre-rotation file after rotation");
        const auto bytes_b = Pattern(300 * 1024, 91);
        const auto file_b = services.Upload("soak-b", "b.bin", bytes_b);
        services.Close();
        const auto last_height = runtime.GetFinalizedHeight().value_or(0);
        node->StopNetwork();
        node.reset();

        // 7. Clean restore: a fresh node and data directory, only the new phrase.
        auto fresh = StartNode(*network, work / "node-b", ip, port);
        auto& fresh_runtime = fresh->Runtime();
        WaitFor("fresh node sync", [&] {
            return fresh_runtime.GetFinalizedHeight().value_or(0) >= last_height;
        }, 180s);
        cybou::CybouIdentityService restored{fresh_runtime, work / "restored.vault"};
        const auto recovered = restored.RestoreIdentitySync(next_words, "another soak vault password", nullptr, 120s);
        cybou::crypto::CleanseMemory(next_entropy->data(), next_entropy->size());
        if (!recovered.success) Fail("restore: " + recovered.error_message);
        if (*restored.GetAccountId() != account) Fail("restored a different AccountID");
        WaitFor("providers on the fresh node", [&] { return ProviderIds(fresh_runtime).size() >= 3; }, 120s);
        Services clean{fresh_runtime, restored.GetKeyStore(), cybou::IdentityDataDirectory(work / "node-b", account)};
        clean.Verify(file_a, bytes_a, "pre-rotation file after clean restore");
        clean.Verify(file_b, bytes_b, "post-rotation file after clean restore");
        clean.Close();
        fresh->StopNetwork();
        Step("OK");
        return 0;
    } catch (const std::exception& e) {
        Fail(std::string{"exception: "} + e.what());
    }
}
