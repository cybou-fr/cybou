// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
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
#include <cybou/p2p/geo_database_updater.h>
#include <cybou/name_service.h>
#include <boost/asio/ip/address.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <memory>
#include <set>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;
namespace {
std::atomic_bool stop{false};
void Stop(int) { stop=true; }

/** cli::Quantity, but "0"/"0s" mean zero: a run that only creates Identities, no attachment. */
uint64_t ZeroOrQuantity(const std::string& text,bool duration) {
    return text=="0" || (duration && text=="0s") ? 0 : cybou::cli::Quantity(text,duration);
}

using Clock=std::chrono::steady_clock;
double Ms(Clock::duration d) { return std::chrono::duration<double,std::milli>(d).count(); }

/** Latency samples of one measured stage (milliseconds). */
struct Samples {
    std::vector<double> values;
    void Add(double ms) { values.push_back(ms); }
    std::string Json() const {
        auto sorted=values;
        std::sort(sorted.begin(),sorted.end());
        const auto at=[&](double q) { return sorted.empty() ? 0.0 : sorted[std::min(sorted.size()-1,static_cast<size_t>(q*(sorted.size()-1)+0.5))]; };
        const auto mean=sorted.empty() ? 0.0 : std::accumulate(sorted.begin(),sorted.end(),0.0)/sorted.size();
        std::ostringstream out;
        out.setf(std::ios::fixed); out.precision(1);
        out << "{\"count\":" << sorted.size() << ",\"mean_ms\":" << mean << ",\"p50_ms\":" << at(0.5)
            << ",\"p95_ms\":" << at(0.95) << ",\"max_ms\":" << (sorted.empty() ? 0.0 : sorted.back()) << '}';
        return out.str();
    }
};

/** Run-wide measurements written by --metrics. */
struct Metrics {
    Samples wallet_finality, publication_finality, publication_protected, publication_queue_wait, publication_signed_to_final;
    std::map<std::string,uint64_t> submitted, failed, busy;
    uint64_t bytes_published{0};
};
Metrics metrics;
struct Client {
    std::atomic_bool caught_up_known_peers{false};
    std::atomic<unsigned> last_sync_status{0};
    std::atomic<size_t> last_sync_peers{0};
    std::atomic<std::uint64_t> sync_passes{0};
    std::unique_ptr<cybou::CybouNodeService> node;
    std::unique_ptr<cybou::CybouIdentityService> identity;
    std::unique_ptr<cybou::PrivateApplicationStore> db;
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
    std::chrono::steady_clock::time_point next_audit{std::chrono::steady_clock::now()+30s};
    /** Submission time of each job not yet finalized / protected (for --metrics). */
    std::map<std::string,Clock::time_point> awaiting_finality, awaiting_protection, awaiting_sign, signed_at;
    Client(const cybou::OfficialNetwork& net, const std::filesystem::path& dir,
           const std::pair<std::string,uint16_t>& peer, const std::string& password, unsigned target, bool recovery) {
        std::filesystem::create_directories(dir);
        events=std::make_shared<cybou::EventWriter>(dir/"client.events.jsonl");
        auto config=cybou::MakeNodeRuntimeConfig(net,dir/"node");
        if (std::none_of(config.configured_peers.begin(),config.configured_peers.end(),
                [&](const auto& configured){return configured.endpoint==peer;}))
            config.configured_peers.push_back({peer,std::nullopt});
        config.peer_admission_policy=std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(
            cybou::p2p::PeerAdmissionPolicy::PublicWithUpdater(cybou::p2p::GeoDatabaseUpdater::Start(dir/"node"/"geo")));
        config.event_writer=events;
        node=std::make_unique<cybou::CybouNodeService>(cybou::CybouNodeServiceConfig{
            .runtime=std::move(config),.genesis=net.genesis_state});
        node->Start();
        // A synthetic client is an ordinary Full Node: it accepts peers on any free port.
        node->StartNetwork({.sync_interval=500ms,.listen_endpoint=std::make_pair(std::string{"0.0.0.0"},uint16_t{0}),.listen_optional=true},[&](const auto& result,const auto&,size_t peers){
            last_sync_status.store(static_cast<unsigned>(result.status));
            last_sync_peers.store(peers);
            sync_passes.fetch_add(1);
            // Ready once a peer confirms there is nothing newer: requiring every connected peer to
            // answer cleanly in one pass never happens in a large mesh with busy peers.
            if (result.IsConnected() && (result.caught_up_with_known_peers ||
                    result.status == cybou::SyncPeerStatus::UP_TO_DATE)) caught_up_known_peers.store(true);
            return true;
        });
        const auto ready_deadline = std::chrono::steady_clock::now()+2400s;
        auto next_report = std::chrono::steady_clock::now()+30s;
        while (!stop && !caught_up_known_peers.load()) {
            if (std::chrono::steady_clock::now()>=next_report) {
                // Initial sync diagnostics: a stuck pass shows as a frozen pass count.
                std::cerr << "initial sync: height=" << node->Runtime().GetFinalizedHeight().value_or(0)
                          << " passes=" << sync_passes.load() << " last_status=" << last_sync_status.load()
                          << " peers=" << last_sync_peers.load() << std::endl;
                next_report += 30s;
            }
            if (std::chrono::steady_clock::now()>ready_deadline) throw std::runtime_error("synthetic client initial sync timeout");
            std::this_thread::sleep_for(100ms);
        }
        if (stop) throw std::runtime_error("synthetic client startup interrupted");
        identity=std::make_unique<cybou::CybouIdentityService>(node->Runtime(),dir/"identity.vault");
        if (std::filesystem::exists(dir/"identity.vault")) {
            if (!identity->LoadVault(password)) throw std::runtime_error("cannot unlock synthetic vault");
        } else {
            if (recovery) throw std::runtime_error("recovery requires an existing synthetic vault");
            if (!identity->PrepareNewIdentity()) throw std::runtime_error("cannot prepare synthetic Identity");
        }
        if (recovery) {
            if (!identity->GetAccountId() || !node->Runtime().GetAccountState(*identity->GetAccountId()))
                throw std::runtime_error("recovery requires a finalized synthetic Identity");
        } else {
            const auto created = identity->CreateIdentitySync(password,nullptr,120s);
            if (!created.success) throw std::runtime_error("synthetic Identity creation failed: "+created.error_message);
        }
        auto& runtime=node->Runtime(); auto& keys=identity->GetKeyStore();
        auto appdir=cybou::IdentityDataDirectory(dir/"node",*identity->GetAccountId());
        db=std::make_unique<cybou::PrivateApplicationStore>(keys,appdir);
        transport=std::make_unique<cybou::RuntimeStorageTransport>(runtime);
        storage=std::make_unique<cybou::StorageService>(runtime,*transport,*db,static_cast<uint8_t>(target));
        publication=std::make_unique<cybou::PublicationService>(runtime,keys,*db,runtime.GetIdentityOperationCoordinator(keys));
        application=std::make_unique<cybou::ApplicationService>(runtime,keys,*db,*storage);
        wallet=std::make_unique<cybou::CybouWalletService>(runtime,keys);
        jobs=publication->Jobs(); // Restart resumes durable exact operations and publication intents.
        events->Write(cybou::NodeEvent::node_started,{{"network_binding",runtime.GetNetworkBinding().GetHex()}});
    }
    bool Advance() {
        publication->ProcessDurability(*storage);
        application->Scan();
        events->Observe(node->Runtime().GetDiagnostics());
        if (std::chrono::steady_clock::now()>=next_audit) {
            storage->AuditNextPlacement(1);
            next_audit=std::chrono::steady_clock::now()+30s;
        }
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
            // Queue wait ends when the job is signed and submitted (it has an operation id).
            if (!result->operation_id.IsNull()) if (auto it=awaiting_sign.find(job); it!=awaiting_sign.end()) {
                metrics.publication_queue_wait.Add(Ms(Clock::now()-it->second));
                signed_at[job]=Clock::now();
                awaiting_sign.erase(it);
            }
            if (result->finalized_height) if (auto it=signed_at.find(job); it!=signed_at.end()) {
                metrics.publication_signed_to_final.Add(Ms(Clock::now()-it->second));
                signed_at.erase(it);
            }
            if (result->finalized_height) if (auto it=awaiting_finality.find(job); it!=awaiting_finality.end()) {
                metrics.publication_finality.Add(Ms(Clock::now()-it->second));
                awaiting_finality.erase(it);
            }
            if (current==cybou::PublicationJobPhase::PROTECTED) if (auto it=awaiting_protection.find(job); it!=awaiting_protection.end()) {
                metrics.publication_protected.Add(Ms(Clock::now()-it->second));
                awaiting_protection.erase(it);
            }
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
            uint64_t fetched_chunks{0};
            bool missing_chunk{false};
            std::set<cybou::ChunkId> seen;
            const auto written = cybou::FetchEncryptedChunkTree(
                std::span<const unsigned char,32>{node->Runtime().GetNetworkBinding().begin(),32},
                *file.item.content_key, *file.item.root_chunk_id,
                [&](const cybou::ChunkId& id) {
                    auto bytes=storage->Fetch(id);
                    if (!bytes) missing_chunk=true;
                    else ++fetched_chunks;
                    return bytes;
                },
                [](std::span<const unsigned char>) { return true; },
                [&](const cybou::ChunkId& id) { return seen.insert(id).second; },
                [&](std::span<const unsigned char> bytes) {
                    for (auto byte : bytes) if (byte != static_cast<unsigned char>((offset++)*131)) return false;
                    return true;
                }, file.item.logical_size);
            if (missing_chunk) { done=false; continue; }
            if (!written || offset != file.item.logical_size) throw std::runtime_error(
                "synthetic file recovery failed: expected="+std::to_string(file.item.logical_size)+
                " decoded="+std::to_string(offset)+" fetched_chunks="+std::to_string(fetched_chunks)+
                " tree_ok="+(written ? "yes" : "no"));
            verified_files.insert(file.item.item_id);
        }
        for (const auto& id : expected_incoming_mail) {
            const auto mail = application->GetMail(id);
            if (!mail) { done=false; continue; }
            if (mail->outgoing || mail->message.subject!="Synthetic DEVNET mail" || mail->message.body!="Synthetic DEVNET payload")
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
                " [--identities 2] [--profile files|mail|root-publications|payments|system-locks|mixed|recovery]\n"
                " [--expected-incoming-mail N] [--expected-files N] (recovery submits no operations)\n"
                " [--recipient NAME.cybou --subject TEXT --body TEXT] [--attachment-size 0] (mail profile)\n"
                " [--operations-per-second 1] [--file-size 4MiB] [--duration 15m] [--replicas 2]\n"
                " [--metrics FILE] (JSON: latencies p50/p95/max, throughput, failures)\n"
                " [--funder DIR] [--fund-each N] (pay N CYBOU from the funder to each synthetic Identity)\n"
                "cybou-loadgen --profile funder --network devnet --data-dir DIR --peer IP:PORT --password-file FILE\n"
                " [--funder-name battlefunder] creates the funder Identity, claims its name and prints its balance;\n"
                " send it CYBOU once from the Central Authority desktop.\n"
                " [--pay-accounts FILE --fund-each N] then tops up every listed AccountID (hex per line) to N.\n"
                "Every load run writes DATA-DIR/accounts.txt with its synthetic AccountIDs.\n"
                "Financial profiles require pre-funded synthetic vaults; onboarding funds only System Balance.\n";
            return 0;
        }
        cybou::cli::Options opts{argc,argv,1};
        opts.Allow({"network","data-dir","peer","password-file","identities","profile","operations-per-second","file-size","duration","replicas","drain-timeout","max-operations","expected-incoming-mail","expected-files","recipient","subject","body","metrics","funder","fund-each","funder-name","pay-accounts","attachment-size"});
        const auto* net=&cybou::RequireOfficialNetwork(opts.Require("network"));
        const auto endpoint=opts.Require("peer"); const auto colon=endpoint.rfind(':');
        if (colon==std::string::npos) throw std::runtime_error("invalid peer");
        auto address=endpoint.substr(0,colon);
        if (address.starts_with('[') && address.ends_with(']')) address=address.substr(1,address.size()-2);
        const auto peer=std::pair{boost::asio::ip::make_address(address).to_string(),static_cast<uint16_t>(cybou::cli::Number(endpoint.substr(colon+1),1,65535))};
        const auto recipient_name=opts.Get("recipient", "");
        const auto count=cybou::cli::Number(opts.Get("identities","2"),recipient_name.empty() ? 2 : 1,100);
        const auto rate=cybou::cli::Number(opts.Get("operations-per-second","1"),1,1000);
        const auto maximum=cybou::cli::Number(opts.Get("max-operations","100000"),1,100000);
        const auto size=cybou::cli::Quantity(opts.Get("file-size","4MiB"));
        const auto attachment_size=ZeroOrQuantity(opts.Get("attachment-size","0"),false);
        if (attachment_size>(64ULL<<20)) throw std::runtime_error("attachment bound exceeded");
        const auto duration=ZeroOrQuantity(opts.Get("duration","15m"),true);
        if (size>(64ULL<<20) || duration>86400000) throw std::runtime_error("load bounds exceeded");
        const auto target=cybou::cli::Number(opts.Get("replicas","2"),1,2);
        auto profile=opts.Get("profile","files");
        if ((!recipient_name.empty() || opts.Has("subject") || opts.Has("body")) && profile!="mail")
            throw std::runtime_error("recipient, subject and body require the mail profile");
        if (recipient_name.empty() && (opts.Has("subject") || opts.Has("body")))
            throw std::runtime_error("custom mail content requires an external recipient");
        if (!std::set<std::string>{"files","mail","root-publications","payments","system-locks","mixed","recovery","funder"}.contains(profile)) throw std::runtime_error("unknown profile");
        const auto expected_mail=cybou::cli::Number(opts.Get("expected-incoming-mail","0"),0,100000);
        const auto expected_files=cybou::cli::Number(opts.Get("expected-files","0"),0,100000);
        if (profile=="recovery" && !expected_mail && !expected_files)
            throw std::runtime_error("recovery requires an expected content count per Identity");
        std::ifstream file{opts.Require("password-file"),std::ios::binary};
        std::string password{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
        if (!file || password.empty() || password.size()>1024) throw std::runtime_error("invalid password file");
        std::signal(SIGINT,Stop); std::signal(SIGTERM,Stop);
#ifdef _WIN32
        std::signal(SIGBREAK,Stop);
#endif
        if (profile=="funder") {
            // One funded Identity per test site, named so the desktop can pay it.
            const auto label=opts.Get("funder-name","battlefunder");
            Client funder{*net,std::filesystem::path{opts.Require("data-dir")},peer,password,static_cast<unsigned>(target),false};
            if (!funder.identity->GetFinalizedPrimaryName()) {
                cybou::CybouNameService names{funder.node->Runtime(),funder.identity->GetKeyStore(),
                    std::filesystem::path{opts.Require("data-dir")}/"identity.vault"};
                const auto claimed=names.ClaimSync(label,password,{},std::chrono::minutes{5});
                if (!claimed.success) throw std::runtime_error("funder name claim failed: "+claimed.message);
            }
            cybou::crypto::CleanseMemory(password.data(),password.size());
            if (opts.Has("pay-accounts")) {
                const auto fund=cybou::cli::Number(opts.Require("fund-each"),1,1'000'000'000);
                std::ifstream list{opts.Get("pay-accounts")};
                if (!list) throw std::runtime_error("cannot read --pay-accounts");
                auto& runtime=funder.node->Runtime();
                for (std::string line; std::getline(list,line);) {
                    while (!line.empty() && (line.back()=='\r' || line.back()==' ')) line.pop_back();
                    if (line.empty()) continue;
                    const auto hash=cybou::Hash256::FromHex(line);
                    if (!hash) throw std::runtime_error("invalid AccountID: "+line);
                    const cybou::AccountId account{*hash};
                    const auto deadline=Clock::now()+180s;
                    // The account must be finalized before it can be paid.
                    while (!stop && !runtime.GetAccountState(account)) {
                        if (Clock::now()>deadline) throw std::runtime_error("account not finalized: "+line);
                        std::this_thread::sleep_for(200ms);
                    }
                    const auto have=runtime.GetAccountState(account)->balance;
                    if (have>=fund) continue;
                    const auto result=funder.wallet->SendPayment(account,fund-have);
                    if (!result) throw std::runtime_error("funding payment failed; send CYBOU to the funder first");
                    while (!stop && runtime.GetOperationStatus(result.op_id).kind!=cybou::OperationStatusKind::FINALIZED) {
                        if (Clock::now()>deadline) throw std::runtime_error("funding finality timeout");
                        std::this_thread::sleep_for(100ms);
                    }
                    std::cout << "funded " << line << " +" << fund-have << '\n';
                }
            }
            const auto [balance,system_balance]=funder.wallet->GetBalances();
            std::cout << "funder=" << label << ".cybou account=" << funder.identity->GetAccountId()->Value().GetHex()
                      << " balance=" << balance << " system_balance=" << system_balance << '\n';
            return 0;
        }
        std::unique_ptr<Client> funder;
        if (opts.Has("funder")) funder=std::make_unique<Client>(*net,std::filesystem::path{opts.Require("funder")},peer,password,target,false);
        std::vector<std::unique_ptr<Client>> clients;
        for (uint64_t i=0;i<count && !stop;++i) clients.push_back(std::make_unique<Client>(*net,std::filesystem::path{opts.Require("data-dir")}/("identity-"+std::to_string(i)),peer,password,target,profile=="recovery"));
        cybou::crypto::CleanseMemory(password.data(),password.size());
        if (clients.size()!=count) return 1;
        {
            std::ofstream accounts{std::filesystem::path{opts.Require("data-dir")}/"accounts.txt"};
            for (const auto& client : clients) accounts << client->identity->GetAccountId()->Value().GetHex() << '\n';
        }
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
        if (const auto fund=cybou::cli::Number(opts.Get("fund-each","0"),0,1'000'000'000); fund) {
            if (!funder) throw std::runtime_error("--fund-each requires --funder");
            for (const auto& client : clients) {
                if (static_cast<uint64_t>(client->wallet->GetBalances().first)>=fund) continue;
                const auto result=funder->wallet->SendPayment(*client->identity->GetAccountId(),fund);
                if (!result) throw std::runtime_error("funding payment failed; send CYBOU to the funder first");
                const auto deadline=Clock::now()+180s;
                while (!stop && client->node->Runtime().GetOperationStatus(result.op_id).kind!=cybou::OperationStatusKind::FINALIZED &&
                       funder->node->Runtime().GetOperationStatus(result.op_id).kind!=cybou::OperationStatusKind::FINALIZED) {
                    if (Clock::now()>deadline) throw std::runtime_error("funding finality timeout");
                    std::this_thread::sleep_for(100ms);
                }
                while (!stop && static_cast<uint64_t>(client->wallet->GetBalances().first)<fund) {
                    if (Clock::now()>deadline) throw std::runtime_error("funding sync timeout");
                    std::this_thread::sleep_for(100ms);
                }
            }
        }
        if (profile=="payments" || profile=="system-locks" || profile=="mixed") {
            // A restarted client node catches up on the funding blocks before its Balance shows it.
            const auto funded_deadline=Clock::now()+120s;
            for (const auto& client : clients)
                while (!stop && client->wallet->GetBalances().first==0 && Clock::now()<funded_deadline)
                    std::this_thread::sleep_for(500ms);
        }
        if (profile=="payments" || profile=="system-locks" || profile=="mixed")
            for (const auto& client : clients) if (client->wallet->GetBalances().first==0)
                throw std::runtime_error("financial profile requires pre-funded DEVNET identities; no test funding bypass exists");
        auto start=std::chrono::steady_clock::now(); uint64_t submitted{0};
        auto next=start;
        while (!stop && profile!="recovery" && std::chrono::steady_clock::now()-start < std::chrono::milliseconds{duration}) {
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
            ++metrics.submitted[actual];
            if (actual=="payments" || actual=="system-locks") {
                const auto submitted_at=Clock::now();
                auto result=actual=="payments" ? client.wallet->SendPayment(*clients[(submitted+1)%count]->identity->GetAccountId(),1) : client.wallet->LockToSystemBalance(1);
                if (!result && result.operation_phase==cybou::IdentityOperationPhase::CONFLICT) {
                    // One Identity, one nonce: its previous operation (often a publication) is still
                    // unresolved. This is the per-account throughput limit, not a failure.
                    ++metrics.busy[actual];
                    ++submitted; next+=std::chrono::microseconds{1000000/rate};
                    while (!stop && Clock::now()<next) std::this_thread::sleep_for(20ms);
                    continue;
                }
                if (!result) { ++metrics.failed[actual]; throw std::runtime_error("wallet load operation failed: error="+std::to_string(static_cast<unsigned>(result.error))+" "+result.error_message); }
                // Do not allocate a replacement nonce while delivery is uncertain.
                auto deadline=std::chrono::steady_clock::now()+120s;
                while (!stop && client.node->Runtime().GetOperationStatus(result.op_id).kind!=cybou::OperationStatusKind::FINALIZED) {
                    client.node->Runtime().GetIdentityOperationCoordinator(client.identity->GetKeyStore()).GetStatus(result.op_id);
                    if (std::chrono::steady_clock::now()>deadline) throw std::runtime_error("wallet finality timeout");
                    std::this_thread::sleep_for(100ms);
                }
                metrics.wallet_finality.Add(Ms(Clock::now()-submitted_at));
            } else {
                const auto job="load-"+std::to_string(client.jobs.size());
                cybou::PublicationJobResult result;
                if (actual=="mail") {
                    cybou::MailMessage message;
                    message.message_id=*cybou::NewPrivateItemId();
                    if (recipient_name.empty()) message.recipient_account_id=*clients[(submitted+1)%count]->identity->GetAccountId();
                    else {
                        auto label=recipient_name;
                        if (label.ends_with(".cybou")) label.resize(label.size()-6);
                        const auto state=client.node->Runtime().GetStore().LoadState();
                        const auto* recipient=state && state.state ? state.state->names.Resolve(label) : nullptr;
                        if (!recipient) throw std::runtime_error("recipient name is not finalized: "+recipient_name);
                        message.recipient_account_id=*recipient;
                    }
                    message.client_timestamp_ms=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                    message.subject=opts.Get("subject", "Synthetic DEVNET mail");
                    message.body=opts.Get("body", "Synthetic DEVNET payload");
                    std::vector<std::pair<size_t,cybou::NewContent>> attachments;
                    if (attachment_size) {
                        // Same deterministic byte pattern as synthetic files, so the recipient can verify it.
                        message.attachments.push_back({.attachment_id=*cybou::NewPrivateItemId(),
                            .filename="synthetic-attachment.bin",.logical_size=attachment_size,
                            .media_type=std::string{"application/octet-stream"}});
                        auto offset=std::make_shared<uint64_t>(0);
                        attachments.emplace_back(0,cybou::NewContent{[offset,attachment_size](std::span<unsigned char> out)->std::optional<size_t> {
                            auto n=std::min<uint64_t>(out.size(),attachment_size-*offset);
                            for (size_t i=0;i<n;++i) out[i]=static_cast<unsigned char>((*offset+i)*131);
                            *offset+=n; return n;
                        }});
                        metrics.bytes_published+=attachment_size;
                    }
                    result=client.publication->PublishMail(job,message,std::move(attachments));
                    if (recipient_name.empty()) clients[(submitted+1)%count]->expected_incoming_mail.push_back(message.message_id);
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
                if (result.phase==cybou::PublicationJobPhase::NEEDS_ATTENTION) { ++metrics.failed[actual]; throw std::runtime_error("publication submission failed: "+result.error); }
                client.jobs.push_back(job);
                client.awaiting_finality[job]=client.awaiting_protection[job]=client.awaiting_sign[job]=Clock::now();
                if (actual=="files") metrics.bytes_published+=size;
            }
            ++submitted; next+=std::chrono::microseconds{1000000/rate};
            while (!stop && std::chrono::steady_clock::now()<next) std::this_thread::sleep_for(20ms);
        }
        auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds{cybou::cli::Quantity(opts.Get("drain-timeout","5m"),true)};
        bool done=false, drain_timeout=false;
        while (!stop && !done) {
            done=true;
            for (auto& client : clients) {
                if (!client->Advance()) done=false;
                if (profile=="recovery") {
                    uint64_t incoming{0};
                    for (const auto& mail : client->application->ListMail()) if (!mail.outgoing) {
                        if (mail.message.subject!="Synthetic DEVNET mail" || mail.message.body!="Synthetic DEVNET payload")
                            throw std::runtime_error("recovered recipient mail differs");
                        ++incoming;
                    }
                    if (incoming<expected_mail || client->verified_files.size()<expected_files) done=false;
                    if (expected_files) for (const auto& recovered : client->application->ListFiles()) {
                        if (recovered.item.kind!=cybou::FileItemKind::FILE) continue;
                        const auto durability=client->storage->GetDurability(recovered.operation_id);
                        if (!durability || durability->state!=cybou::DurabilityState::PROTECTED ||
                            durability->min_replicas<client->storage->RemoteReplicaTarget()) done=false;
                    }
                }
            }
            if (!done && std::chrono::steady_clock::now()>deadline) {
                // Keep the measurements of a partly drained run; the result says what happened.
                std::cerr << "cybou-loadgen: drain timeout: unfinished publications retained for restart\n";
                drain_timeout=true;
                break;
            }
            if (!done) std::this_thread::sleep_for(1s);
        }
        for (auto& client : clients) client->events->Write(cybou::NodeEvent::node_stopping);
        const auto elapsed_s=std::chrono::duration<double>(Clock::now()-start).count();
        // Attempts refused because the account was busy are not operations.
        for (const auto& [_,n] : metrics.busy) submitted-=std::min<uint64_t>(submitted,n);
        std::cout << "operations=" << submitted << " result=" << (done ? "PASS" : drain_timeout ? "DRAIN_TIMEOUT" : "INTERRUPTED") << '\n';
        if (opts.Has("metrics")) {
            std::ofstream out{opts.Get("metrics")};
            const auto counts=[](const std::map<std::string,uint64_t>& m) {
                std::string s{"{"};
                for (const auto& [k,v] : m) s+=(s.size()>1 ? "," : "")+std::string{"\""}+k+"\":"+std::to_string(v);
                return s+"}";
            };
            out << "{\"profile\":\"" << profile << "\",\"identities\":" << count << ",\"result\":\"" << (done ? "PASS" : drain_timeout ? "DRAIN_TIMEOUT" : "INTERRUPTED")
                << "\",\"elapsed_s\":" << elapsed_s << ",\"operations\":" << submitted
                << ",\"operations_per_s\":" << (elapsed_s>0 ? submitted/elapsed_s : 0.0)
                << ",\"bytes_published\":" << metrics.bytes_published
                << ",\"submitted\":" << counts(metrics.submitted) << ",\"failed\":" << counts(metrics.failed)
                << ",\"busy\":" << counts(metrics.busy)
                << ",\"wallet_submit_to_final\":" << metrics.wallet_finality.Json()
                << ",\"publication_submit_to_final\":" << metrics.publication_finality.Json()
                << ",\"publication_submit_to_protected\":" << metrics.publication_protected.Json()
                << ",\"publication_queue_wait\":" << metrics.publication_queue_wait.Json()
                << ",\"publication_signed_to_final\":" << metrics.publication_signed_to_final.Json() << "}\n";
            if (!out) throw std::runtime_error("cannot write metrics");
        }
        return done ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << "cybou-loadgen: " << e.what() << '\n'; return 1; }
}
