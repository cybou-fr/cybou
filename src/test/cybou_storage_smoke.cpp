// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

// Multi-process storage smoke client, driven by test/cybou_storage_smoke.py.
//
// A light full node that talks to a real PoA finalizer and real storage
// providers only over CYP2. It creates an Identity, publishes an encrypted
// file, waits for finality and remote durability, reports the provider that
// holds the replica, waits until that provider is killed, detects the loss by
// audit, repairs to another provider and verifies exact bytes from remote GET.

#include <cybou/official_networks.h>
#include <cybou/application_service.h>
#include <cybou/encrypted_chunk_tree.h>
#include <cybou/identity_service.h>
#include <test/cybou_service_test_fixture.h>
#include <cybou/kv_store.h>
#include <cybou/network_definition.h>
#include <cybou/node_service.h>
#include <cybou/private_application_store.h>
#include <cybou/publication_service.h>
#include <cybou/storage_service.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
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

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 5) {
        std::cerr << "usage: cybou-storage-smoke devnet WORK_DIR FINALIZER_IP FINALIZER_P2P_PORT\n";
        return 2;
    }
    try {
        const std::filesystem::path work{argv[2]};
        std::filesystem::create_directories(work);
        const auto* network = &cybou::RequireOfficialNetwork(argv[1]);
        const std::string finalizer_ip{argv[3]};
        const auto finalizer_port = static_cast<std::uint16_t>(std::stoul(argv[4]));

        cybou::CybouNodeService node{{
            .runtime = cybou::NodeRuntimeConfig{.network_definition = network->network_definition,
                .data_dir = work / "client-db", .p2p_endpoint = std::make_pair(finalizer_ip, finalizer_port), .peer_admission_policy = TestLabAdmissionPolicy()},
            .genesis = network->genesis_state,
        }};
        node.Start();
        node.StartNetwork(cybou::CybouNetworkServiceConfig{.sync_interval = 500ms},
            [](const cybou::SyncPeerResult&, const cybou::NodeRuntimeStatus&, std::size_t) { return true; });
        auto& runtime = node.Runtime();
        WaitFor("verified sync from the finalizer",
            [&] { return runtime.GetFinalizedHeight().value_or(0) > 0; }, 60s);
        Step("SYNCED height=" + std::to_string(runtime.GetFinalizedHeight().value_or(0)));

        cybou::CybouIdentityService identity{runtime, work / "identity.vault"};
        const std::string password{"smoke test vault password"};
        if (!identity.PrepareNewIdentity()) Fail("cannot prepare identity");
        const auto created = identity.CreateIdentitySync(password, nullptr, 120s);
        if (!created.success) Fail("identity creation: " + created.error_message);
        Step("IDENTITY finalized");

        auto& keystore = identity.GetKeyStore();
        cybou::PrivateApplicationStore db{keystore, work / "app"};
        cybou::KVStore staging{cybou::KVStoreOptions{.path = work / "staging"}};
        cybou::RuntimeStorageTransport transport{runtime};
        cybou::StorageService storage{runtime, transport, db};
        cybou::PublicationService publication{runtime, keystore, db,
            runtime.GetIdentityOperationCoordinator(keystore), staging};
        cybou::ApplicationService application{runtime, keystore, db, storage};

        WaitFor("two storage providers discovered through the finalizer",
            [&] { return runtime.StoragePeerEndpoints().size() >= 2; }, 120s);
        Step("PROVIDERS " + std::to_string(runtime.StoragePeerEndpoints().size()));

        std::vector<unsigned char> original(700 * 1024);
        for (std::size_t i{0}; i < original.size(); ++i) original[i] = static_cast<unsigned char>((i * 131) ^ (i >> 7));
        const auto item_id = *cybou::NewPrivateItemId();
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item_id,
            cybou::FileItem{.item_id = item_id, .kind = cybou::FileItemKind::FILE, .name = "smoke.bin"}});
        std::size_t offset{0};
        std::vector<std::pair<std::size_t, cybou::NewContent>> content;
        content.emplace_back(0, cybou::NewContent{[&](std::span<unsigned char> out) -> std::optional<std::size_t> {
            const auto n = std::min(out.size(), original.size() - offset);
            std::copy_n(original.begin() + static_cast<std::ptrdiff_t>(offset), n, out.begin());
            offset += n;
            return n;
        }});
        const auto submitted = publication.PublishFiles("smoke-upload", batch, std::move(content));
        if (submitted.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) Fail("publish: " + submitted.error);
        Step("PUBLISHED");

        const auto job_phase = [&] { return publication.GetJob("smoke-upload")->phase; };
        WaitFor("finality and remote durability", [&] {
            publication.ProcessDurability(storage);
            return job_phase() == cybou::PublicationJobPhase::PROTECTED;
        }, 180s);
        const auto operation = publication.GetJob("smoke-upload")->operation_id;
        Step("PROTECTED op=" + operation.GetHex());

        // Report the provider holding the file's root replica and wait until it is killed.
        auto placement = storage.DescribePlacement(operation);
        if (!placement || placement->replicas.empty() || placement->replicas.front().empty()) Fail("no placement");
        const auto holder = placement->replicas.front().front();
        Step("HOLDER " + holder.address + " " + std::to_string(holder.port));
        WaitFor("the orchestrator to kill the holder", [&] { return std::filesystem::exists(work / "killed"); }, 120s);

        const auto first_audit = storage.AuditNextPlacement(64);
        if (!first_audit || first_audit->first != operation ||
            first_audit->second.state != cybou::DurabilityState::PROTECTED) {
            Fail("first audit did not repair the lost replica: " +
                (first_audit ? first_audit->second.error : std::string{"placement not found"}));
        }

        // StorageService audits every known placement, detects the lost copy and repairs elsewhere.
        WaitFor("audit to detect the lost replica and repair it", [&] {
            storage.AuditNextPlacement(64);
            publication.ProcessDurability(storage);
            const auto now = storage.DescribePlacement(operation);
            if (!now) return false;
            for (const auto& replicas : now->replicas) {
                for (const auto& endpoint : replicas) {
                    if (cybou::SameProvider(endpoint, holder)) return false;
                }
            }
            return job_phase() == cybou::PublicationJobPhase::PROTECTED;
        }, 180s);
        Step("REPAIRED");

        // Evict every local encrypted chunk: the file must come back from a provider, exactly.
        placement = storage.DescribePlacement(operation);
        for (const auto& leaf : placement->leaves) runtime.GetChunkBlobStore().Remove(leaf);
        WaitFor("the file to be indexed", [&] { application.Scan(); return application.GetFile(item_id).has_value(); }, 60s);
        const auto file = application.GetFile(item_id);
        std::vector<unsigned char> downloaded;
        std::set<cybou::ChunkId> seen;
        const auto written = cybou::FetchEncryptedChunkTree(
            std::span<const unsigned char, 32>{runtime.GetNetworkId().begin(), 32}, *file->item.content_key,
            *file->item.root_chunk_id, [&](const cybou::ChunkId& id) { return storage.Fetch(id); },
            [](std::span<const unsigned char>) { return true; },
            [&](const cybou::ChunkId& id) { return seen.insert(id).second; },
            [&](std::span<const unsigned char> data) {
                downloaded.insert(downloaded.end(), data.begin(), data.end());
                return true;
            }, original.size());
        if (!written || downloaded != original) Fail("remote download does not match the original bytes");
        Step("VERIFIED bytes=" + std::to_string(downloaded.size()));
        node.StopNetwork();
        Step("OK");
        return 0;
    } catch (const std::exception& e) {
        Fail(std::string{"exception: "} + e.what());
    }
}
