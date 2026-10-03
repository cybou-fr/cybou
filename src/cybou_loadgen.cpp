// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license.
// Synthetic client: all operations go through production services and journals.
#include <cybou/official_networks.h>
#include <cybou/cli/command_line.h>
#include <cybou/node_service.h>
#include <cybou/identity_service.h>
#include <cybou/wallet_service.h>
#include <cybou/application_service.h>
#include <cybou/publication_service.h>
#include <cybou/storage_service.h>
#include <cybou/encrypted_chunk_tree.h>
#include <cybou/crypto/cleanse.h>
#include <boost/asio/ip/address.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <thread>

using namespace std::chrono_literals;
namespace {
std::atomic_bool stop{false};
void Stop(int) { stop=true; }
struct Client {
    std::unique_ptr<cybou::CybouNodeService> node;
    std::unique_ptr<cybou::CybouIdentityService> identity;
    std::unique_ptr<cybou::PrivateApplicationStore> db;
    std::unique_ptr<cybou::KVStore> staging;
    std::unique_ptr<cybou::RuntimeStorageTransport> transport;
    std::unique_ptr<cybou::StorageService> storage;
    std::unique_ptr<cybou::PublicationService> publication;
    std::unique_ptr<cybou::ApplicationService> application;
    std::unique_ptr<cybou::CybouWalletService> wallet;
    std::vector<std::string> jobs;
    std::map<std::string,cybou::PublicationJobPhase> phases;
    std::set<cybou::PrivateItemId> verified_files;
    std::vector<cybou::PrivateItemId> expected_incoming_mail;
    std::shared_ptr<cybou::EventWriter> events;
    Client(const cybou::OfficialNetwork& net, const std::filesystem::path& dir,
           const std::pair<std::string,uint16_t>& peer, const std::string& password, unsigned target) {
        std::filesystem::create_directories(dir);
        events=std::make_shared<cybou::EventWriter>(dir/"client.events.jsonl");
        node=std::make_unique<cybou::CybouNodeService>(cybou::CybouNodeServiceConfig{
            .runtime={.network_definition=net.network_definition,.data_dir=dir/"node",.configured_peers={{peer}},.event_writer=events},.genesis=net.genesis_state});
        node->Start();
        node->StartNetwork({.sync_interval=500ms},[](const auto&,const auto&,size_t){return true;});
        const auto ready_deadline = std::chrono::steady_clock::now()+120s;
        while (!stop && (!node->Runtime().ConnectedPeerCount() || !node->Runtime().GetFinalizedHeight().value_or(0))) {
            if (std::chrono::steady_clock::now()>ready_deadline) throw std::runtime_error("synthetic client initial sync timeout");
            std::this_thread::sleep_for(100ms);
        }
        // Height > 0 is not synced: AccountCreate work is stamped from the local
        // height, so wait until sync only follows new blocks (<=2 per 3 s).
        for (auto last=node->Runtime().GetFinalizedHeight().value_or(0); !stop;) {
            if (std::chrono::steady_clock::now()>ready_deadline+600s) throw std::runtime_error("synthetic client initial sync timeout");
            std::this_thread::sleep_for(3s);
            const auto now=node->Runtime().GetFinalizedHeight().value_or(0);
            if (now>0 && now-last<=2) break;
            last=now;
        }
        if (stop) throw std::runtime_error("synthetic client startup interrupted");
        identity=std::make_unique<cybou::CybouIdentityService>(node->Runtime(),dir/"identity.vault");
        if (std::filesystem::exists(dir/"identity.vault")) {
            if (!identity->LoadVault(password)) throw std::runtime_error("cannot unlock synthetic vault");
        } else {
            if (!identity->PrepareNewIdentity()) throw std::runtime_error("cannot prepare synthetic Identity");
        }
        {
            const auto created = identity->CreateIdentitySync(password,nullptr,120s);
            if (!created.success) throw std::runtime_error("synthetic Identity creation failed: "+created.error_message);
        }
        auto& runtime=node->Runtime(); auto& keys=identity->GetKeyStore();
        auto appdir=cybou::IdentityDataDirectory(dir/"node",*identity->GetAccountId());
        db=std::make_unique<cybou::PrivateApplicationStore>(keys,appdir);
        staging=std::make_unique<cybou::KVStore>(cybou::KVStoreOptions{.path=dir/"staging"});
        transport=std::make_unique<cybou::RuntimeStorageTransport>(runtime);
        storage=std::make_unique<cybou::StorageService>(runtime,*transport,*db,static_cast<uint8_t>(target));
        publication=std::make_unique<cybou::PublicationService>(runtime,keys,*db,runtime.GetIdentityOperationCoordinator(keys),*staging);
        application=std::make_unique<cybou::ApplicationService>(runtime,keys,*db,*storage);
        wallet=std::make_unique<cybou::CybouWalletService>(runtime,keys);
        jobs=publication->Jobs(); // Restart resumes durable exact operations and publication intents.
        events->Write(cybou::NodeEvent::node_started,{{"role",std::string{"loadgen"}},{"network_binding",runtime.GetNetworkBinding().GetHex()}});
    }
    bool Advance() {
        publication->ProcessDurability(*storage);
        application->Scan();
        events->Observe(node->Runtime().GetDiagnostics());
        storage->AuditNextPlacement(16);
        bool done=true;
        for (const auto& job : jobs) {
            auto result=publication->GetJob(job);
            if (!result) throw std::runtime_error("missing durable publication job");
            if (result->phase==cybou::PublicationJobPhase::NEEDS_ATTENTION) throw std::runtime_error("publication needs attention");

            auto durability=storage->GetDurability(result->operation_id);
            const auto replicas=durability ? durability->min_replicas : 0;
            const auto current = result->phase == cybou::PublicationJobPhase::PROTECTED &&
                durability && durability->state == cybou::DurabilityState::PROTECTED && replicas >= storage->RemoteReplicaTarget()
                ? cybou::PublicationJobPhase::PROTECTED : result->finalized_height ? cybou::PublicationJobPhase::SECURING : result->phase;
            if (current != cybou::PublicationJobPhase::PROTECTED) done=false;
            if (!phases.contains(job) || phases[job]!=current) {
                events->Write(current==cybou::PublicationJobPhase::PROTECTED ? cybou::NodeEvent::content_protected : cybou::NodeEvent::content_securing,
                    {{"operation_id",result->operation_id.GetHex()},{"replicas",uint64_t{replicas}},{"target",uint64_t{storage->RemoteReplicaTarget()}}});
                phases[job]=current;
            }
        }
        if (done) for (const auto& file : application->ListFiles()) {
            if (file.item.kind != cybou::FileItemKind::FILE || verified_files.contains(file.item.item_id)) continue;
            if (!file.item.content_key || !file.item.root_chunk_id) throw std::runtime_error("synthetic file content missing");
            uint64_t offset{0};
            std::set<cybou::ChunkId> seen;
            const auto written = cybou::FetchEncryptedChunkTree(
                std::span<const unsigned char,32>{node->Runtime().GetNetworkBinding().begin(),32},
                *file.item.content_key, *file.item.root_chunk_id,
                [&](const cybou::ChunkId& id) { return storage->Fetch(id); },
                [](std::span<const unsigned char>) { return true; },
                [&](const cybou::ChunkId& id) { return seen.insert(id).second; },
                [&](std::span<const unsigned char> bytes) {
                    for (auto byte : bytes) if (byte != static_cast<unsigned char>((offset++)*131)) return false;
                    return true;
                }, file.item.logical_size);
            if (!written || offset != file.item.logical_size) throw std::runtime_error("synthetic file download bytes differ");
            verified_files.insert(file.item.item_id);
        }
        for (const auto& id : expected_incoming_mail) {
            const auto mail = application->GetMail(id);
            if (!mail) { done=false; continue; }
            if (mail->outgoing || mail->message.subject!="Synthetic LAB mail" || mail->message.body!="Synthetic LAB payload")
                throw std::runtime_error("synthetic recipient mail differs");
        }
        if (!events->Good()) throw std::runtime_error("event output failed");
        return done;
    }
};
}
int main(int argc,char* argv[]) {
    try {
        if (argc==1 || (argc==2 && std::string_view{argv[1]}=="--help")) {
            std::cout << "cybou-loadgen --network devnet --data-dir DIR --peer IP:PORT --password-file FILE\n"
                " [--identities 2] [--profile files|mail|root-publications|payments|system-locks|mixed]\n"
                " [--operations-per-second 1] [--file-size 4MiB] [--duration 15m] [--replicas 2]\n"
                "Financial profiles require pre-funded synthetic vaults; onboarding funds only System Balance.\n";
            return 0;
        }
        cybou::cli::Options opts{argc,argv,1};
        opts.Allow({"network","data-dir","peer","password-file","identities","profile","operations-per-second","file-size","duration","replicas","drain-timeout","max-operations"});
        const auto* net=&cybou::RequireOfficialNetwork(opts.Require("network"));
        const auto endpoint=opts.Require("peer"); const auto colon=endpoint.rfind(':');
        if (colon==std::string::npos) throw std::runtime_error("invalid peer");
        auto address=endpoint.substr(0,colon);
        if (address.starts_with('[') && address.ends_with(']')) address=address.substr(1,address.size()-2);
        const auto peer=std::pair{boost::asio::ip::make_address(address).to_string(),static_cast<uint16_t>(cybou::cli::Number(endpoint.substr(colon+1),1,65535))};
        const auto count=cybou::cli::Number(opts.Get("identities","2"),2,100);
        const auto rate=cybou::cli::Number(opts.Get("operations-per-second","1"),1,1000);
        const auto maximum=cybou::cli::Number(opts.Get("max-operations","100000"),1,100000);
        const auto size=cybou::cli::Quantity(opts.Get("file-size","4MiB"));
        const auto duration=cybou::cli::Quantity(opts.Get("duration","15m"),true);
        if (size>(64ULL<<20) || duration>86400000) throw std::runtime_error("load bounds exceeded");
        const auto target=cybou::cli::Number(opts.Get("replicas","2"),1,2);
        auto profile=opts.Get("profile","files");
        if (!std::set<std::string>{"files","mail","root-publications","payments","system-locks","mixed"}.contains(profile)) throw std::runtime_error("unknown profile");
        std::ifstream file{opts.Require("password-file"),std::ios::binary};
        std::string password{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
        if (!file || password.empty() || password.size()>1024) throw std::runtime_error("invalid password file");
        std::signal(SIGINT,Stop); std::signal(SIGTERM,Stop);
#ifdef _WIN32
        std::signal(SIGBREAK,Stop);
#endif
        std::vector<std::unique_ptr<Client>> clients;
        for (uint64_t i=0;i<count && !stop;++i) clients.push_back(std::make_unique<Client>(*net,std::filesystem::path{opts.Require("data-dir")}/("identity-"+std::to_string(i)),peer,password,target));
        cybou::crypto::CleanseMemory(password.data(),password.size());
        if (clients.size()!=count) return 1;
        const auto accounts_deadline=std::chrono::steady_clock::now()+120s;
        bool accounts_ready=false;
        while (!stop && !accounts_ready) {
            accounts_ready=true;
            for (const auto& client : clients) for (const auto& other : clients)
                if (!client->node->Runtime().GetAccountState(*other->identity->GetAccountId())) accounts_ready=false;
            if (!accounts_ready && std::chrono::steady_clock::now()>accounts_deadline)
                throw std::runtime_error("synthetic recipient Identity sync timeout");
            if (!accounts_ready) std::this_thread::sleep_for(100ms);
        }
        if (stop) return 1;
        if (profile=="payments" || profile=="system-locks" || profile=="mixed")
            for (const auto& client : clients) if (client->wallet->GetBalances().first==0)
                throw std::runtime_error("financial profile requires pre-funded LAB identities; no test funding bypass exists");
        auto start=std::chrono::steady_clock::now(); uint64_t submitted{0};
        auto next=start;
        while (!stop && std::chrono::steady_clock::now()-start < std::chrono::milliseconds{duration}) {
            if (submitted>=maximum) {
                for (auto& active : clients) active->Advance();
                std::this_thread::sleep_for(1s);
                continue;
            }
            auto& client=*clients[submitted%count];
            client.Advance();
            // Persistent unique job IDs survive restarts; bounded staged queue.
            if (client.jobs.size()>=100000) throw std::runtime_error("publication job ceiling reached");
            auto actual=profile;
            if (actual=="mixed") actual=submitted%10<5 ? "payments" : submitted%10<7 ? "mail" : submitted%10<9 ? "files" : "system-locks";
            if (actual=="payments" || actual=="system-locks") {
                auto result=actual=="payments" ? client.wallet->SendPayment(*clients[(submitted+1)%count]->identity->GetAccountId(),1) : client.wallet->LockToSystemBalance(1);
                if (!result) throw std::runtime_error("wallet load operation failed");
                // Do not allocate a replacement nonce while delivery is uncertain.
                auto deadline=std::chrono::steady_clock::now()+120s;
                while (!stop && client.node->Runtime().GetOperationStatus(result.op_id).kind!=cybou::OperationStatusKind::FINALIZED) {
                    client.node->Runtime().GetIdentityOperationCoordinator(client.identity->GetKeyStore()).GetStatus(result.op_id);
                    if (std::chrono::steady_clock::now()>deadline) throw std::runtime_error("wallet finality timeout");
                    std::this_thread::sleep_for(100ms);
                }
            } else {
                const auto job="load-"+std::to_string(client.jobs.size());
                cybou::PublicationJobResult result;
                if (actual=="mail") {
                    cybou::MailMessage message;
                    message.message_id=*cybou::NewPrivateItemId();
                    message.recipient_account_id=*clients[(submitted+1)%count]->identity->GetAccountId();
                    message.client_timestamp_ms=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                    message.subject="Synthetic LAB mail"; message.body="Synthetic LAB payload";
                    result=client.publication->PublishMail(job,message);
                    clients[(submitted+1)%count]->expected_incoming_mail.push_back(message.message_id);
                } else {
                    cybou::FilesMutationBatch batch;
                    auto id=*cybou::NewPrivateItemId();
                    batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM,id,
                        cybou::FileItem{.item_id=id,.kind=cybou::FileItemKind::FILE,.name="synthetic.bin"}});
                    auto offset=std::make_shared<uint64_t>(0);
                    cybou::NewContent content{[offset,size](std::span<unsigned char> out)->std::optional<size_t> {
                        auto n=std::min<uint64_t>(out.size(),size-*offset);
                        for (size_t i=0;i<n;++i) out[i]=static_cast<unsigned char>((*offset+i)*131);
                        *offset+=n; return n;
                    }};
                    std::vector<std::pair<size_t,cybou::NewContent>> contents;
                    contents.emplace_back(0,std::move(content));
                    result=client.publication->PublishFiles(job,batch,std::move(contents));
                }
                if (result.phase==cybou::PublicationJobPhase::NEEDS_ATTENTION) throw std::runtime_error("publication submission failed: "+result.error);
                client.jobs.push_back(job);
            }
            ++submitted; next+=std::chrono::microseconds{1000000/rate};
            while (!stop && std::chrono::steady_clock::now()<next) std::this_thread::sleep_for(20ms);
        }
        auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds{cybou::cli::Quantity(opts.Get("drain-timeout","5m"),true)};
        bool done=false;
        while (!stop && !done) {
            done=true; for (auto& client : clients) if (!client->Advance()) done=false;
            if (!done && std::chrono::steady_clock::now()>deadline) throw std::runtime_error("drain timeout: unfinished publications retained for restart");
            if (!done) std::this_thread::sleep_for(100ms);
        }
        for (auto& client : clients) client->events->Write(cybou::NodeEvent::node_stopping,{{"role",std::string{"loadgen"}}});
        std::cout << "operations=" << submitted << " result=" << (done ? "PASS" : "INTERRUPTED") << '\n';
        return done ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << "cybou-loadgen: " << e.what() << '\n'; return 1; }
}
