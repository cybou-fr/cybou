// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Реализация headless CLI единственного исполняемого файла `cybou`.

#include <cybou/cli/cybou_cli.h>

#include <cybou/cli/command_line.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/poa_finalizer.h>
#include <cybou/hex.h>
#include <cybou/identity_service.h>
#include <cybou/keystore.h>
#include <cybou/network_genesis.h>
#include <cybou/network_genesis.h>
#include <cybou/node_runtime.h>
#include <cybou/node_service.h>
#include <cybou/official_networks.h>
#include <cybou/p2p/geo_database_updater.h>
#include <cybou/p2p/peer_admission.h>
#include <cybou/p2p/peer_manager.h>
#include <cybou/private_application_store.h>
#include <cybou/protocol_limits.h>
#include <cybou/secret32.h>
#include <cybou/secret_file.h>
#include <cybou/storage_service.h>

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace cybou::cli {
namespace {

using Endpoint = std::pair<std::string, uint16_t>;

std::atomic_bool stopping{false};
std::shared_ptr<EventWriter> events;
std::shared_ptr<const p2p::PeerAdmissionPolicy> peer_admission_policy;

void Stop(int) { stopping.store(true); }

const char* HELP = R"(CYBOU (headless; run without arguments for the desktop)
                [--block-interval 1000ms] [--peers FILE] [--capacity 20GiB] [--advertise IP:PORT]
                [--tls-certificate FILE --tls-key FILE] [--event-log FILE] [--event-log-mode minimal|detailed]
  node run --network devnet --data-dir DIR [--peer IP:PORT] [--listen IP:PORT] [--peers FILE]
           [--capacity 20GiB] [--poa-key-file FILE] [--block-interval 1000ms]
           [--advertise IP:PORT] [--tls-certificate FILE --tls-key FILE]
           [--event-log FILE] [--event-log-mode minimal|detailed]
           [--identity-vault FILE --identity-password-file FILE]   (sign Validation as this Identity)
  identity restore --network devnet --data-dir DIR --vault NEW_FILE --phrase-file FILE
                   --password-file FILE [--peer IP:PORT] [--timeout 600s]
  network info --network devnet          (NetworkID, binding, genesis and bootstrap locators)
  network probe --network devnet --data-dir DIR --peer IP:PORT
  network sync --network devnet --data-dir DIR --peer IP:PORT [--count 100]
  network follow --network devnet --data-dir DIR (--peer IP:PORT | --peers FILE) [--until-height N]
  operation submit --network devnet --data-dir DIR (--peer IP:PORT | --peers FILE) --operation-file FILE
  operation status --network devnet --data-dir DIR --operation-id HEX
  doctor --network devnet --data-dir DIR [--listen IP:PORT] [--peers FILE] [--key-file FILE]
  storage status --event-log FILE
  storage verify --network devnet --data-dir DIR --chunk-id HEX [--peer IP:PORT]
  storage placement --network devnet --data-dir DIR --vault FILE --password-file FILE
                    --operation-id HEX [--replicas 1|2]
MAINNET is not provisioned and cannot start. Secrets are file inputs.
Network commands require --peer-admission france (automatic DB-IP Lite country data; offline
override --geo-country-csv FILE --geo-sha256 HEX --geo-issued-month YYYY-MM). The bootstrap locator serves --tls-certificate/--tls-key matching
its compiled SPKI pin.
)";

std::vector<unsigned char> ReadFile(const std::filesystem::path& path, const size_t limit)
{
    const auto size = std::filesystem::file_size(path);
    if (size > limit) throw std::runtime_error("file exceeds size limit");
    std::vector<unsigned char> bytes(size);
    std::ifstream file(path, std::ios::binary);
    if (!file || !file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) throw std::runtime_error("cannot read file");
    return bytes;
}

uint16_t Port(std::string_view text)
{
    return static_cast<uint16_t>(Number(text, 1, 65535));
}

Endpoint ParseEndpoint(const std::string& text)
{
    const auto colon = text.rfind(':');
    if (colon == std::string::npos) throw std::invalid_argument("expected numeric IP:port");
    auto host = text.substr(0, colon);
    if (host.starts_with("[") && host.ends_with("]")) host = host.substr(1, host.size() - 2);
    return {boost::asio::ip::make_address(host).to_string(), Port(text.substr(colon + 1))};
}

std::vector<Endpoint> ReadPeerEndpoints(const std::filesystem::path& path)
{
    if (std::filesystem::file_size(path) > 4096) throw std::runtime_error("peer list exceeds 4 KiB");
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read peer list");
    std::vector<Endpoint> endpoints;
    std::set<Endpoint> seen;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::istringstream fields(line);
        std::string address_text, port_text, extra;
        if (!(fields >> address_text >> port_text) || (fields >> extra)) throw std::runtime_error("invalid peer list entry");
        boost::system::error_code ec;
        const auto address = boost::asio::ip::make_address(address_text, ec);
        if (ec) throw std::runtime_error("peer list requires numeric IP addresses");
        const Endpoint endpoint{address.to_string(), Port(port_text)};
        if (!seen.insert(endpoint).second) throw std::runtime_error("duplicate peer list entry");
        endpoints.push_back(endpoint);
        if (endpoints.size() > p2p::MAX_OUTBOUND_PEERS) throw std::runtime_error("too many peer list entries");
    }
    if (endpoints.empty()) throw std::runtime_error("peer list is empty");
    return endpoints;
}

std::array<unsigned char, 32> Sha256Pin(std::string_view text)
{
    if (text.size() != 64) throw std::invalid_argument("Geo data SHA-256 must contain 64 hex characters");
    std::array<unsigned char, 32> digest{};
    auto nibble = [](const char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<unsigned>(c - 'A' + 10);
        throw std::invalid_argument("Geo data SHA-256 is not hexadecimal");
    };
    for (size_t i = 0; i < digest.size(); ++i) {
        digest[i] = static_cast<unsigned char>((nibble(text[2 * i]) << 4) | nibble(text[2 * i + 1]));
    }
    return digest;
}

const std::initializer_list<std::string> ADMISSION_OPTIONS{"peer-admission", "geo-country-csv", "geo-sha256", "geo-issued-month"};

std::set<std::string> AllowedOptions(std::initializer_list<std::string> keys, const bool admission)
{
    std::set<std::string> allowed(keys);
    if (admission) allowed.insert(ADMISSION_OPTIONS.begin(), ADMISSION_OPTIONS.end());
    return allowed;
}

void ConfigurePeerAdmission(const Options& opts, const std::filesystem::path& data_directory)
{
    const auto mode = opts.Require("peer-admission");
    if (mode != "france") throw std::invalid_argument("peer admission must be france");
    const bool has_csv = opts.Has("geo-country-csv");
    const bool has_sha256 = opts.Has("geo-sha256");
    const bool has_month = opts.Has("geo-issued-month");
    if (!has_csv && !has_sha256 && !has_month) {
        // В онлайн-режиме CLI делегирует актуальность updater'у; пока готового
        // датасета нет, публичный P2P останется fail-closed по месту использования.
        auto updater = p2p::GeoDatabaseUpdater::Start(data_directory / "geo");
        peer_admission_policy = std::make_shared<const p2p::PeerAdmissionPolicy>(
            p2p::PeerAdmissionPolicy::PublicWithUpdater(std::move(updater)));
        return;
    }
    if (!has_csv || !has_sha256 || !has_month) {
        throw std::invalid_argument("offline Geo override requires --geo-country-csv, --geo-sha256, and --geo-issued-month");
    }
    const auto issued_month = p2p::FrenchIpDataset::ParseIssuedMonth(opts.Require("geo-issued-month"));
    if (!issued_month) throw std::invalid_argument("Geo issued month must use YYYY-MM");
    const auto dataset = p2p::FrenchIpDataset::LoadDbIpCountryCsv(
        opts.Require("geo-country-csv"), Sha256Pin(opts.Require("geo-sha256")), *issued_month);
    if (!dataset) throw std::invalid_argument("France Geo CSV is missing, corrupt, expired, future-dated, or has the wrong SHA-256");
    peer_admission_policy = std::make_shared<const p2p::PeerAdmissionPolicy>(p2p::PeerAdmissionPolicy::Public(dataset));
    std::cerr << "Peer Geo data: DB-IP Lite IP to Country; attribution: DB-IP.com (CC BY 4.0)\n";
}

void Allow(const Options& opts, std::initializer_list<std::string> keys, bool admission = false)
{
    opts.AllowSet(AllowedOptions(keys, admission));
}

/** Apply CLI-local policy to the shared official runtime configuration. */
NodeRuntimeConfig RuntimeConfig(const OfficialNetwork& network, const std::filesystem::path& data_dir)
{
    auto config = MakeNodeRuntimeConfig(network, data_dir);
    config.peer_admission_policy = peer_admission_policy;
    config.event_writer = events;
    return config;
}

std::unique_ptr<CybouNodeService> StartNode(const OfficialNetwork& network, NodeRuntimeConfig config)
{
    auto node = std::make_unique<CybouNodeService>(CybouNodeServiceConfig{
        .runtime = std::move(config), .genesis = network.genesis_state});
    node->Start();
    return node;
}

void StartEvents(const Options& opts, const std::string& node_type)
{
    const auto mode = opts.Get("event-log-mode", "minimal");
    if (mode != "minimal" && mode != "detailed") throw std::invalid_argument("event log mode must be minimal or detailed");
    if (opts.Has("event-log")) {
        events = std::make_shared<EventWriter>(opts.Get("event-log"),
            mode == "detailed" ? EventLogMode::DETAILED : EventLogMode::MINIMAL);
        events->Write(NodeEvent::node_started, {{"node_type", node_type}});
    }
}

std::optional<TlsServerIdentity> TlsIdentity(const Options& opts)
{
    if (opts.Has("tls-certificate") != opts.Has("tls-key")) {
        throw std::invalid_argument("--tls-certificate and --tls-key must be given together");
    }
    if (!opts.Has("tls-certificate")) return std::nullopt;
    if (!opts.Has("listen")) throw std::invalid_argument("a stable TLS identity requires --listen");
    return TlsServerIdentity{opts.Get("tls-certificate"), opts.Get("tls-key")};
}

int PrintPeerSubmitResult(const p2p::PeerSubmitResult& result)
{
    std::cout << "status=";
    if (result) std::cout << static_cast<unsigned>(result.acknowledgment->status);
    else if (result.delivery_uncertain) std::cout << "unconfirmed";
    else if (result.acknowledgment) std::cout << static_cast<unsigned>(result.acknowledgment->status);
    else std::cout << "unavailable";
    std::cout << " operation=" << result.op_id.GetHex();
    if (result.endpoint && (result || !result.delivery_uncertain)) {
        std::cout << " peer=" << result.endpoint->first << ':' << result.endpoint->second;
    }
    std::cout << std::endl;
    return result ? 0 : 1;
}

std::optional<std::vector<Endpoint>> PeerList(const Options& opts)
{
    return opts.Has("peers") ? std::optional{ReadPeerEndpoints(opts.Get("peers"))} : std::nullopt;
}

// ---- network ----

int NetworkInfo(const Options& opts)
{
    Allow(opts, {"network"});
    const auto& network = RequireOfficialNetwork(opts.Require("network"));
    std::cout << "network=" << network.name
              << "\nnetwork_id=" << cybou::HexEncode(network.genesis.GetNetworkId())
              << "\nnetwork_binding=" << ComputeNetworkBinding(network.genesis.GetNetworkPublicKey()).GetHex()
              << "\ngenesis=" << network.genesis.GetGenesisAnchor().GetHex() << '\n';
    for (const auto& locator : network.rendezvous_locators) {
        std::cout << "bootstrap=" << locator.host << ':' << locator.port
                  << " spki_sha256=" << cybou::HexEncode(locator.tls_spki_sha256) << '\n';
    }
    return 0;
}

int NetworkProbe(const Options& opts)
{
    Allow(opts, {"network", "data-dir", "peer"}, true);
    ConfigurePeerAdmission(opts, opts.Require("data-dir"));
    const auto& network = RequireOfficialNetwork(opts.Require("network"));
    const auto peer = ParseEndpoint(opts.Require("peer"));
    auto node = StartNode(network, RuntimeConfig(network, opts.Require("data-dir")));
    p2p::PeerManager peers{node->Runtime()};
    if (!peers.Connect(peer.first, peer.second) || peers.PingAll() != 1) throw std::runtime_error("P2P handshake or ping failed");
    const auto info = peers.Peers().front();
    std::cout << "peer=" << info.address << ':' << info.port << " height=" << info.hello.finalized_height
              << std::endl;
    return 0;
}

int NetworkSync(const Options& opts)
{
    Allow(opts, {"network", "data-dir", "peer", "count"}, true);
    ConfigurePeerAdmission(opts, opts.Require("data-dir"));
    const auto& network = RequireOfficialNetwork(opts.Require("network"));
    const auto peer = ParseEndpoint(opts.Require("peer"));
    auto node = StartNode(network, RuntimeConfig(network, opts.Require("data-dir")));
    auto& runtime = node->Runtime();
    p2p::PeerManager peers{runtime};
    if (!peers.Connect(peer.first, peer.second)) throw std::runtime_error("P2P handshake failed");
    const auto result = peers.SyncFromPeer(peer.first, peer.second, Number(opts.Get("count", "100"), 1, 1'000'000));
    if (!result.IsConnected()) throw std::runtime_error("P2P block sync failed");
    std::cout << "height=" << *runtime.GetFinalizedHeight() << " applied=" << result.blocks_applied << std::endl;
    return 0;
}

int NetworkFollow(const Options& opts)
{
    Allow(opts, {"network", "data-dir", "peer", "peers", "until-height"}, true);
    ConfigurePeerAdmission(opts, opts.Require("data-dir"));
    if (opts.Has("peer") == opts.Has("peers")) throw std::invalid_argument("specify exactly one of --peer or --peers");
    const auto& network = RequireOfficialNetwork(opts.Require("network"));
    const auto endpoints = opts.Has("peers") ? ReadPeerEndpoints(opts.Get("peers"))
                                              : std::vector<Endpoint>{ParseEndpoint(opts.Get("peer"))};
    const auto until_height = opts.Has("until-height")
        ? std::optional<uint64_t>{Number(opts.Get("until-height"), 1, UINT64_MAX)} : std::nullopt;
    auto node = StartNode(network, RuntimeConfig(network, opts.Require("data-dir")));
    auto& runtime = node->Runtime();
    p2p::PeerManager peers{runtime};
    std::vector<bool> rejected(endpoints.size(), false);
    std::vector<std::chrono::steady_clock::time_point> retry_after(endpoints.size());
    size_t preferred_peer{0};
    while (!stopping) {
        const auto status = runtime.GetStatus();
        if (!status.is_initialized) throw std::runtime_error("node state unavailable");
        if (until_height && status.finalized_height >= *until_height) return 0;
        bool progress{false};
        for (size_t offset = 0; offset < endpoints.size() && !stopping; ++offset) {
            const size_t i = (preferred_peer + offset) % endpoints.size();
            if (rejected[i] || std::chrono::steady_clock::now() < retry_after[i]) continue;
            const auto& [host, port] = endpoints[i];
            const auto connected = peers.Peers();
            const bool present = std::any_of(connected.begin(), connected.end(), [&](const auto& peer) {
                return peer.address == host && peer.port == port;
            });
            if (!present && !peers.Connect(host, port)) {
                if (peers.LastConnectStatus() != p2p::PeerConnectStatus::UNAVAILABLE) {
                    // Семантический отказ (wrong network, bad handshake, geo reject)
                    // фиксируем навсегда для этого запуска, чтобы не крутиться по
                    // заведомо плохому endpoint'у бесконечно.
                    rejected[i] = true;
                    std::cerr << "P2P peer rejected: " << host << ':' << port << '\n';
                } else {
                    retry_after[i] = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                }
                continue;
            }
            const auto height = runtime.GetFinalizedHeight();
            if (!height) throw std::runtime_error("node height unavailable");
            const uint64_t batch = until_height ? std::min<uint64_t>(100, *until_height - *height) : 100;
            const auto result = peers.SyncFromPeer(host, port, batch);
            if (result.blocks_applied > 0) {
                progress = true;
                std::cout << "height=" << *runtime.GetFinalizedHeight() << " peer=" << host << ':' << port << std::endl;
            }
            if (result.status == SyncPeerStatus::PROTOCOL_ERROR || result.status == SyncPeerStatus::NETWORK_MISMATCH) {
                rejected[i] = true;
                std::cerr << "P2P peer failed block verification: " << host << ':' << port << '\n';
            }
            if (result.status == SyncPeerStatus::CONNECTION_FAILED) {
                retry_after[i] = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            }
            if (until_height && *runtime.GetFinalizedHeight() >= *until_height) return 0;
            if (result.blocks_applied > 0) {
                preferred_peer = i;
                break;
            }
        }
        if (std::all_of(rejected.begin(), rejected.end(), [](bool value) { return value; })) {
            throw std::runtime_error("all configured P2P peers rejected");
        }
        if (!progress) std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return 0;
}

int NetworkCommand(const std::string& action, const Options& opts)
{
    if (action == "info") return NetworkInfo(opts);
    if (action == "probe") return NetworkProbe(opts);
    if (action == "sync") return NetworkSync(opts);
    if (action == "follow") return NetworkFollow(opts);
    throw std::invalid_argument("unknown network command; use --help");
}

// ---- operation ----

int OperationCommand(const std::string& action, const Options& opts)
{
    if (action == "status") {
        Allow(opts, {"network", "data-dir", "operation-id"});
        const auto& network = RequireOfficialNetwork(opts.Require("network"));
        const auto op_id = ParseHash256UserHex(opts.Require("operation-id"));
        if (!op_id || op_id->IsNull()) throw std::runtime_error("invalid OperationID");
        auto node = StartNode(network, RuntimeConfig(network, opts.Require("data-dir")));
        const auto result = node->Runtime().FindFinalizedOperation(*op_id);
        if (result.status == FinalizedOperationLookupStatus::FOUND) {
            std::cout << "status=finalized operation=" << op_id->GetHex() << " height=" << result.height
                      << " index=" << result.operation_index << " block=" << result.block_id.GetHex() << std::endl;
            return 0;
        }
        std::cout << "status=" << (result.status == FinalizedOperationLookupStatus::NOT_FOUND ? "not-found" : "history-unavailable")
                  << " operation=" << op_id->GetHex() << " scanned_height=" << result.scanned_height << std::endl;
        return result.status == FinalizedOperationLookupStatus::NOT_FOUND ? 1 : 2;
    }
    if (action != "submit") throw std::invalid_argument("unknown operation command; use --help");
    Allow(opts, {"network", "data-dir", "peer", "peers", "operation-file"}, true);
    ConfigurePeerAdmission(opts, opts.Require("data-dir"));
    if (opts.Has("peer") == opts.Has("peers")) throw std::invalid_argument("specify exactly one of --peer or --peers");
    const auto& network = RequireOfficialNetwork(opts.Require("network"));
    const auto endpoints = opts.Has("peers") ? ReadPeerEndpoints(opts.Get("peers"))
                                              : std::vector<Endpoint>{ParseEndpoint(opts.Get("peer"))};
    const auto operation = DeserializeProtocolOperation(ReadFile(opts.Require("operation-file"), MAX_OPERATION_PAYLOAD_BYTES));
    if (!operation) throw std::runtime_error("invalid operation file");
    auto node = StartNode(network, RuntimeConfig(network, opts.Require("data-dir")));
    p2p::PeerManager peers{node->Runtime()};
    const auto work = node->Runtime().PrepareOperationWork(*operation);
    if (!work) throw std::runtime_error("cannot compute the operation proof-of-work");
    return PrintPeerSubmitResult(peers.SubmitOperationToAny(endpoints, *operation, *work));
}

// ---- Identity ----

struct PasswordWiper {
    std::string& value;
    ~PasswordWiper() { if (!value.empty()) crypto::CleanseMemory(value.data(), value.size()); }
};

struct WordsWiper {
    RecoveryWords& words;
    ~WordsWiper() { for (auto& word : words) crypto::CleanseMemory(word.data(), word.size()); }
};

/** Reads a private password file; trailing newlines are not part of the password. */
std::string ReadPassword(const std::filesystem::path& path)
{
    auto bytes = ReadSecretFile(path, 1024);
    if (!bytes) throw std::runtime_error("password file must be a private regular file");
    std::string password(bytes->begin(), bytes->end());
    crypto::CleanseMemory(bytes->data(), bytes->size());
    while (!password.empty() && (password.back() == '\n' || password.back() == '\r')) password.pop_back();
    if (password.size() < 12) {
        crypto::CleanseMemory(password.data(), password.size());
        throw std::runtime_error("vault password must have at least 12 characters");
    }
    return password;
}

/**
 * Restores an Identity from its 24 words into a new encrypted vault. A
 * genesis allocation that names this phrase and is still unclaimed is
 * claimed by a finalized AccountCreate, so the network must be finalizing.
 */
int IdentityCommand(const std::string& action, const Options& opts)
{
    if (action != "restore") throw std::invalid_argument("unknown identity command; use --help");
    Allow(opts, {"network", "data-dir", "vault", "phrase-file", "password-file", "peer", "timeout"}, true);
    ConfigurePeerAdmission(opts, opts.Require("data-dir"));
    const auto& network = RequireOfficialNetwork(opts.Require("network"));
    const std::filesystem::path vault = opts.Require("vault");
    if (std::filesystem::exists(vault)) throw std::runtime_error("refusing to replace an existing vault");
    const auto timeout_ms = Quantity(opts.Get("timeout", "600s"), true);

    RecoveryWords words;
    WordsWiper wipe_words{words};
    {
        auto bytes = ReadSecretFile(opts.Require("phrase-file"), 1024);
        if (!bytes) throw std::runtime_error("phrase file must be a private regular file");
        std::string text(bytes->begin(), bytes->end());
        PasswordWiper wipe_text{text};
        crypto::CleanseMemory(bytes->data(), bytes->size());
        std::istringstream in{text};
        size_t count{0};
        std::string word;
        while (in >> word) {
            PasswordWiper wipe_word{word};
            if (count < words.size()) words[count] = word;
            ++count;
        }
        if (count != words.size() || !DecodeRecoveryWords(words)) throw std::runtime_error("the phrase file must hold one valid 24-word recovery phrase");
    }
    std::string password = ReadPassword(opts.Require("password-file"));
    PasswordWiper wipe_password{password};

    auto config = RuntimeConfig(network, opts.Require("data-dir"));
    if (opts.Has("peer")) config.configured_peers.push_back({ParseEndpoint(opts.Get("peer")), std::nullopt});
    auto node = StartNode(network, std::move(config));
    std::atomic_bool synced{false};
    node->StartNetwork(CybouNetworkServiceConfig{.sync_interval = std::chrono::milliseconds{250}},
        [&synced](const SyncPeerResult& sync, const NodeRuntimeStatus&, size_t) {
            if (sync.IsConnected() && sync.caught_up_with_known_peers) synced = true;
            return true;
        });
    // The phrase is resolved against verified state: catch up first.
    const auto sync_deadline = std::chrono::steady_clock::now() + std::chrono::minutes{2};
    while (!synced && !stopping && std::chrono::steady_clock::now() < sync_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{250});
    }
    if (!synced) {
        node->StopNetwork();
        throw std::runtime_error("could not synchronize with known peers");
    }
    CybouIdentityService identity{node->Runtime(), vault};
    const auto result = identity.RestoreIdentitySync(words, std::move(password),
        [](IdentityCreationPhase, const std::string& message) { std::cout << message << std::endl; },
        std::chrono::milliseconds{timeout_ms});
    node->StopNetwork();
    if (!result.success) throw std::runtime_error(result.error_message);
    std::cout << "account=" << result.account_id.Value().GetHex() << " height=" << result.creation_height << '\n';
    return 0;
}

// ---- ordinary Full Node ----

int RunNode(const Options& opts)
{
    Allow(opts, {"network", "data-dir", "peer", "listen", "peers", "capacity", "advertise",
        "tls-certificate", "tls-key", "event-log", "event-log-mode", "poa-key-file", "block-interval",
        "identity-vault", "identity-password-file"}, true);
    if (opts.Has("identity-vault") != opts.Has("identity-password-file")) {
        throw std::invalid_argument("--identity-vault and --identity-password-file go together");
    }
    if (opts.Has("advertise") && !opts.Has("listen")) throw std::invalid_argument("--advertise requires --listen");
    ConfigurePeerAdmission(opts, opts.Require("data-dir"));
    StartEvents(opts, "Full Node");
    const auto& network = RequireOfficialNetwork(opts.Require("network"));
    const auto listen = opts.Has("listen") ? std::optional{ParseEndpoint(opts.Get("listen"))} : std::nullopt;

    auto config = RuntimeConfig(network, opts.Require("data-dir"));
    // Official networks discover peers from their compiled rendezvous locators;
    // CLI только добавляет operator hints поверх этой конституции сети.
    if (opts.Has("peer")) config.configured_peers.push_back({ParseEndpoint(opts.Get("peer")), std::nullopt});
    else if (network.rendezvous_locators.empty() && !opts.Has("peers") && !listen)
        throw std::invalid_argument("a network without bootstrap locators requires --peer, --peers or --listen");
    if (listen) config.advertised_endpoint = opts.Has("advertise") ? ParseEndpoint(opts.Get("advertise")) : *listen;
    config.tls_server_identity = TlsIdentity(opts);
    if (opts.Has("capacity")) {
        const auto capacity = Quantity(opts.Get("capacity"));
        if (capacity == 0) throw std::invalid_argument("--capacity must be positive; omit it for automatic allocation");
        config.storage_capacity_bytes = capacity;
    }
    if (opts.Has("poa-key-file")) {
        auto bytes = ReadSecretFile(opts.Require("poa-key-file"), 32);
        if (!bytes || bytes->size() != 32) throw std::runtime_error("PoA key file must be private and contain exactly 32 raw bytes");
        std::array<unsigned char, 32> seed{};
        std::copy(bytes->begin(), bytes->end(), seed.begin());
        // Временный вектор очищается до передачи в `Secret32`, чтобы в памяти
        // не оставалось двух живых копий одного и того же PoA секрета.
        crypto::CleanseMemory(bytes->data(), bytes->size());
        config.poa_finalizer_recovery_entropy = Secret32{seed};
        crypto::CleanseMemory(seed.data(), seed.size());
    }
    const auto interval = Quantity(opts.Get("block-interval", "1000ms"), true);
    if (interval == 0 || interval > 60000) throw std::invalid_argument("invalid block interval");
    auto node = StartNode(network, std::move(config));
    // An ordinary Identity of this node's operator: it signs Validation while
    // its finalized AUTH is above the threshold. It never finalizes.
    std::unique_ptr<CybouIdentityService> identity;
    if (opts.Has("identity-vault")) {
        identity = std::make_unique<CybouIdentityService>(node->Runtime(), opts.Require("identity-vault"));
        std::string password = ReadPassword(opts.Require("identity-password-file"));
        PasswordWiper wipe_password{password};
        const bool unlocked = identity->LoadVault(password);
        if (!unlocked || !identity->GetAccountId()) throw std::runtime_error("cannot unlock the Identity vault");
        node->Runtime().SetValidationSigner(std::make_shared<CybouKeyStoreValidationSigner>(identity->GetKeyStore()));
        const auto account_state = identity->GetFinalizedAccountState();
        const bool eligible = account_state && account_state->authority > VALIDATION_AUTHORITY_THRESHOLD;
        std::cout << "identity=" << identity->GetAccountId()->Value().GetHex()
                  << " validation signer configured (eligible=" << (eligible ? "yes" : "no") << ")" << std::endl;
    }
    if (auto peers = PeerList(opts)) {
        if (opts.Has("peer")) peers->insert(peers->begin(), ParseEndpoint(opts.Get("peer")));
        node->Runtime().SetConfiguredPeerEndpoints(*peers);
    }
    std::atomic<std::uint64_t> last_height{0};
    node->StartNetwork(CybouNetworkServiceConfig{.sync_interval = std::chrono::milliseconds{250}, .block_interval_ms = interval, .listen_endpoint = listen},
        [&last_height, &node](const SyncPeerResult& sync, const NodeRuntimeStatus& status, size_t peers) {
            if (status.runtime_state == NodeRuntimeState::NETWORK_MISMATCH ||
                status.runtime_state == NodeRuntimeState::CORRUPT) {
                std::cerr << "node state unavailable" << std::endl;
                stopping = true;
                return false;
            }
            // Signing safety disables the local signer in the production worker.
            // The ordinary Full Node and its network worker remain available.
            if (status.finalized_height != last_height.exchange(status.finalized_height)) {
                std::cout << "height=" << status.finalized_height << " peers=" << peers << std::endl;
            }
            if (events) {
                const auto d = node->Runtime().GetDiagnostics();
                events->Write(sync.IsConnected() ? NodeEvent::sync_progress : NodeEvent::sync_failed,
                    {{"height", d.height}, {"error_code", std::uint64_t{static_cast<unsigned>(sync.status)}}});
                events->Observe(d);
                if (!events->Good()) { stopping = true; return false; }
            }
            return true;
        });
    while (!stopping.load()) std::this_thread::sleep_for(std::chrono::milliseconds{250});
    node->StopNetwork();
    // The signer references the vault's key store: never let it outlive it.
    node->Runtime().SetValidationSigner(nullptr);
    const auto final_status = node->Runtime().GetStatus();
    return final_status.runtime_state != NodeRuntimeState::READY || (events && !events->Good()) ? 2 : 0;
}

// ---- doctor ----

int Doctor(const Options& opts)
{
    Allow(opts, {"network", "data-dir", "listen", "peers", "key-file"});
    const auto& network = RequireOfficialNetwork(opts.Require("network"));
    std::cout << "Network OK " << network.name << "\nNetwork binding "
              << ComputeNetworkBinding(network.genesis.GetNetworkPublicKey()).GetHex() << '\n';
    auto dir = std::filesystem::absolute(opts.Require("data-dir"));
    auto parent = dir;
    while (!std::filesystem::exists(parent)) parent = parent.parent_path();
    const auto perms = std::filesystem::status(parent).permissions();
    if ((perms & (std::filesystem::perms::owner_write | std::filesystem::perms::group_write |
                  std::filesystem::perms::others_write)) == std::filesystem::perms::none) {
        throw std::runtime_error("doctor: directory not writable");
    }
    std::cout << "Data dir OK\nDisk free " << std::filesystem::space(parent).available << " bytes\n";
    if (opts.Has("listen")) {
        const auto endpoint = ParseEndpoint(opts.Get("listen"));
        boost::asio::io_context io;
        boost::asio::ip::tcp::acceptor port{io};
        const auto address = boost::asio::ip::make_address(endpoint.first);
        port.open(address.is_v4() ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6());
        port.bind({address, endpoint.second});
        std::cout << "Listen OK\n";
    }
    if (opts.Has("peers")) { ReadPeerEndpoints(opts.Get("peers")); std::cout << "Peers OK\n"; }
    if (opts.Has("key-file")) {
        auto bytes = ReadFile(opts.Get("key-file"), 32);
        if (bytes.size() != 32) throw std::runtime_error("doctor: invalid finalizer seed size");
#ifndef _WIN32
        const auto permissions = std::filesystem::status(opts.Get("key-file")).permissions();
        if ((permissions & (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) != std::filesystem::perms::none) {
            throw std::runtime_error("doctor: finalizer seed must be private (0600)");
        }
#endif
        std::array<unsigned char, 32> seed{};
        std::copy(bytes.begin(), bytes.end(), seed.begin());
        crypto::CleanseMemory(bytes.data(), bytes.size());
        auto key = DeriveIdentityPublicKey(seed, IdentityKeyPurpose::POA_FINALIZER);
        crypto::CleanseMemory(seed.data(), seed.size());
        if (!key || *key != network.genesis.GetPoaPublicKey()) throw std::runtime_error("doctor: wrong PoA key");
        std::cout << "PoA key OK\n";
    }
    // LevelDB has no read-only open. Inspect a stable private COPY, never recover
    // or lock the operator's DB. Refuse snapshots that change during copying.
    if (std::filesystem::exists(dir / "CURRENT")) {
        auto temp = std::filesystem::temp_directory_path() / ("cybou-doctor-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(temp)) throw std::runtime_error("doctor: cannot create snapshot");
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{temp};
        std::map<std::filesystem::path, std::pair<uintmax_t, std::filesystem::file_time_type>> before;
        std::uint64_t total{0};
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_symlink()) throw std::runtime_error("doctor: DB symlink rejected");
            if (!entry.is_regular_file() || entry.path().filename() == "LOCK") continue;
            const auto size = entry.file_size();
            if (size > (2ULL << 30) - total) throw std::runtime_error("doctor: snapshot exceeds 2 GiB");
            total += size;
            before[entry.path()] = {size, entry.last_write_time()};
            std::filesystem::copy_file(entry.path(), temp / entry.path().filename());
        }
        for (const auto& [path, stamp] : before) {
            if (std::filesystem::file_size(path) != stamp.first || std::filesystem::last_write_time(path) != stamp.second) {
                throw std::runtime_error("doctor: DB changed during read-only snapshot; retry while stopped");
            }
        }
        CybouNodeRuntime copy{{.network_genesis = network.genesis, .data_dir = temp}};
        const auto status = copy.GetStatus();
        if (!status.is_initialized || status.poa_safety_halted) throw std::runtime_error("doctor: foreign/corrupt/halted DB");
        std::cout << "State OK height=" << status.finalized_height << '\n';
    } else if (std::filesystem::exists(dir) && !std::filesystem::is_empty(dir)) {
        throw std::runtime_error("doctor: non-empty directory without canonical DB");
    } else {
        std::cout << "State EMPTY\n";
    }
    std::cout << "Result READY\n";
    return 0;
}

// ---- storage ----

int StorageCommand(const std::string& action, const Options& opts)
{
    if (action == "status") {
        Allow(opts, {"event-log"});
        std::ifstream file{opts.Require("event-log"), std::ios::binary};
        if (!file) throw std::runtime_error("cannot open output-only event log");
        file.seekg(0, std::ios::end);
        const auto size = file.tellg();
        file.seekg(std::max<std::streamoff>(0, static_cast<std::streamoff>(size) - 65536));
        std::string line, last;
        while (std::getline(file, line)) {
            if (line.find("\"event\":\"node_status\"") != std::string::npos && line.ends_with('}')) last = line;
        }
        if (last.empty()) throw std::runtime_error("no complete recent node_status sample");
        std::cout << last << '\n';
        return 0;
    }
    Allow(opts, {"network", "data-dir", "peer", "chunk-id", "vault", "password-file", "operation-id", "replicas"}, true);
    if (opts.Has("peer")) ConfigurePeerAdmission(opts, opts.Require("data-dir"));
    const auto& network = RequireOfficialNetwork(opts.Require("network"));
    auto config = RuntimeConfig(network, opts.Require("data-dir"));
    if (opts.Has("peer")) config.configured_peers.push_back({ParseEndpoint(opts.Get("peer")), std::nullopt});
    auto node = StartNode(network, std::move(config));
    auto& runtime = node->Runtime();
    if (action == "verify") {
        // ChunkID uses displayed raw BLAKE3 bytes, not cybou::Hash256 display order.
        const auto text = opts.Require("chunk-id");
        if (text.size() != 64) throw std::runtime_error("ChunkID must be 64 raw hex characters");
        ChunkId chunk{};
        const auto digit = [](char c) -> unsigned {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            throw std::runtime_error("invalid hex");
        };
        for (size_t i = 0; i < 32; ++i) chunk[i] = static_cast<unsigned char>((digit(text[2 * i]) << 4) | digit(text[2 * i + 1]));
        auto bytes = runtime.GetChunkBlobStore().Get(chunk);
        if (!bytes && opts.Has("peer")) {
            runtime.SyncFromConfiguredPeer(100);
            for (const auto& peer : runtime.StorageEndpoints()) {
                bytes = runtime.GetChunkFromStorageEndpoint(peer.address, peer.port, peer.storage_id, chunk);
                if (bytes) break;
            }
        }
        if (!bytes || ComputeChunkId(*bytes) != chunk) throw std::runtime_error("chunk absent or BLAKE3 verification failed");
        std::cout << "verified chunk=" << text << " bytes=" << bytes->size() << '\n';
        return 0;
    }
    if (action != "placement") throw std::invalid_argument("unknown storage command");
    CybouIdentityService identity{runtime, opts.Require("vault")};
    auto password_bytes = ReadSecretFile(opts.Require("password-file"), 1024);
    if (!password_bytes) throw std::runtime_error("password file must be a private regular file");
    std::string password(password_bytes->begin(), password_bytes->end());
    crypto::CleanseMemory(password_bytes->data(), password_bytes->size());
    const bool unlocked = identity.LoadVault(password);
    crypto::CleanseMemory(password.data(), password.size());
    if (!unlocked || !identity.GetAccountId()) throw std::runtime_error("cannot unlock Identity vault");
    const auto operation = ParseHash256UserHex(opts.Require("operation-id"));
    if (!operation) throw std::runtime_error("invalid OperationID");
    PrivateApplicationStore db{identity.GetKeyStore(), IdentityDataDirectory(opts.Require("data-dir"), *identity.GetAccountId())};
    RuntimeStorageTransport transport{runtime};
    StorageService storage{runtime, transport, db, static_cast<uint8_t>(Number(opts.Get("replicas", "1"), 1, 2))};
    const auto placement = storage.DescribePlacement(*operation);
    const auto durability = storage.GetDurability(*operation);
    if (!placement || !durability) throw std::runtime_error("unknown private placement");
    std::cout << "operation=" << operation->GetHex() << " chunks=" << placement->leaves.size()
              << " min_remote_replicas=" << durability->min_replicas << " target=" << unsigned(storage.RemoteReplicaTarget()) << '\n';
    return 0;
}

int Dispatch(int argc, char* argv[])
{
    const std::string group = argv[1];
    if (group == "--help" || group == "help") { std::cout << HELP; return 0; }
    if (group == "doctor") return Doctor(Options{argc, argv, 2});
    if (argc < 3) throw std::invalid_argument("missing command; use --help");
    const std::string action = argv[2];
    const Options opts{argc, argv, 3};
    if (group == "network") return NetworkCommand(action, opts);
    if (group == "operation") return OperationCommand(action, opts);
    if (group == "storage") return StorageCommand(action, opts);
    if (group == "identity") return IdentityCommand(action, opts);
    if (action != "run") throw std::invalid_argument("unknown command; use --help");
    int result{0};
    if (group == "node") result = RunNode(opts);
    else throw std::invalid_argument("unknown command; use --help");
    if (events) events->Write(NodeEvent::node_stopping, {{"node_type", std::string{"Full Node"}}});
    return result;
}

} // namespace

bool IsCommand(const std::string_view first)
{
    return first == "node" || first == "network" || first == "identity" ||
        first == "operation" || first == "doctor" || first == "storage" || first == "--help" || first == "help";
}

int Run(int argc, char* argv[])
{
    peer_admission_policy.reset();
    events.reset();
    stopping = false;
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
#ifdef _WIN32
    std::signal(SIGBREAK, Stop);
#endif
    try {
        if (argc < 2 || !IsCommand(argv[1])) throw std::invalid_argument("unknown command; use --help");
        return Dispatch(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "cybou: " << e.what() << '\n';
        return 1;
    }
}

} // namespace cybou::cli
