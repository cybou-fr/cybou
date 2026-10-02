// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/finalizer_node.h>
#include <cybou/cli/command_line.h>
#include <cybou/identity_service.h>
#include <cybou/keystore.h>
#include <cybou/private_application_store.h>
#include <cybou/storage_service.h>
#include <cybou/protocol_limits.h>
#include <cybou/secret_file.h>
#include <cybou/secret32.h>
#include <cybou/bootstrap_nodes.h>
#include <cybou/p2p/geo_database_updater.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/hex.h>
#include <cybou/network_definition.h>
#include <cybou/node_runtime.h>
#include <cybou/node_service.h>
#include <cybou/p2p/peer_manager.h>
#include <cybou/p2p/peer_admission.h>
#include <cybou/signing.h>

#include <boost/asio.hpp>

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <charconv>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
std::atomic_bool stopping{false};
std::shared_ptr<cybou::EventWriter> events;
std::vector<std::pair<std::string,uint16_t>> explicit_peers;
std::optional<std::pair<std::string,uint16_t>> advertised_endpoint;
std::shared_ptr<const cybou::p2p::PeerAdmissionPolicy> active_peer_admission_policy;
std::unique_ptr<cybou::CybouKeyStore> active_bootstrap_keystore;

void Stop(int) { stopping.store(true); }

void ConfigureBootstrapIdentity(cybou::NodeRuntimeConfig& config)
{
    if (!active_bootstrap_keystore) return;
    const auto account = active_bootstrap_keystore->GetAccountId();
    if (!account) throw std::runtime_error("cannot read bootstrap Identity account");
    config.bootstrap_identity_account = *account;
    config.bootstrap_proof_signer = [](const std::span<const unsigned char> message)
        -> std::optional<std::vector<unsigned char>> {
        if (!active_bootstrap_keystore || message.empty()) return std::nullopt;
        const auto signature = active_bootstrap_keystore->SignAuthorization(message);
        if (!signature || signature->ml_dsa.size() != 2420) return std::nullopt;
        std::vector<unsigned char> bytes(signature->ed25519.begin(), signature->ed25519.end());
        bytes.insert(bytes.end(), signature->ml_dsa.begin(), signature->ml_dsa.end());
        return bytes;
    };
}

void RequireBootstrapIdentityAuthorized(const cybou::CybouNodeRuntime& runtime)
{
    if (active_bootstrap_keystore && !runtime.LocalBootstrapAccountId()) {
        throw std::runtime_error("bootstrap Identity has no claimed grant in this network");
    }
}

std::vector<unsigned char> ReadFile(const std::filesystem::path& path, const size_t limit)
{
    const auto size = std::filesystem::file_size(path);
    if (size > limit) throw std::runtime_error("file exceeds size limit");
    std::vector<unsigned char> bytes(size);
    std::ifstream file(path, std::ios::binary);
    if (!file || !file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        throw std::runtime_error("cannot read file");
    }
    return bytes;
}

void WriteNewFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
{
    if (std::filesystem::exists(path)) throw std::runtime_error("network file already exists");
    std::ofstream file(path, std::ios::binary | std::ios::out);
    if (!file || !file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size())) {
        throw std::runtime_error("cannot write network file");
    }
}

void PutU32(std::vector<unsigned char>& out, const uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint16_t Port(const char* value)
{
    unsigned number{0};
    const std::string_view input{value};
    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() ||
        number == 0 || number > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("invalid port");
    }
    return static_cast<uint16_t>(number);
}

std::vector<std::pair<std::string, uint16_t>> ReadPeerEndpoints(const std::filesystem::path& path)
{
    if (std::filesystem::file_size(path) > 4096) throw std::runtime_error("peer list exceeds 4 KiB");
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read peer list");
    std::vector<std::pair<std::string, uint16_t>> endpoints;
    std::set<std::pair<std::string, uint16_t>> seen;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::istringstream fields(line);
        std::string address_text, port_text, extra;
        if (!(fields >> address_text >> port_text) || (fields >> extra)) {
            throw std::runtime_error("invalid peer list entry");
        }
        boost::system::error_code ec;
        const auto address = boost::asio::ip::make_address(address_text, ec);
        if (ec) throw std::runtime_error("peer list requires numeric IP addresses");
        const auto endpoint = std::make_pair(address.to_string(), Port(port_text.c_str()));
        if (!seen.insert(endpoint).second) throw std::runtime_error("duplicate peer list entry");
        endpoints.push_back(endpoint);
        if (endpoints.size() > cybou::p2p::MAX_OUTBOUND_PEERS) {
            throw std::runtime_error("too many peer list entries");
        }
    }
    if (endpoints.empty()) throw std::runtime_error("peer list is empty");
    return endpoints;
}

uint64_t PositiveCount(const char* value)
{
    const auto count = std::stoull(value);
    if (count == 0 || count > 1'000'000) throw std::runtime_error("invalid count");
    return count;
}

/** Storage capacity in bytes: at least 1 MiB, at most 1 TiB. */
uint64_t CapacityBytes(const char* value)
{
    const std::string_view text{value};
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        throw std::runtime_error("invalid storage capacity");
    }
    const auto bytes = std::stoull(std::string{text});
    if (bytes < (1ULL << 20) || bytes > (1ULL << 40)) throw std::runtime_error("invalid storage capacity");
    return bytes;
}

uint64_t TargetHeight(const char* value)
{
    uint64_t height{0};
    const std::string_view input{value};
    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), height);
    if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() || height == 0) {
        throw std::runtime_error("invalid target height");
    }
    return height;
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

void ConfigurePeerAdmission(const cybou::cli::Options& opts, const std::filesystem::path& data_directory)
{
    const auto mode = opts.Require("peer-admission");
    if (mode == "lab") {
        if (opts.Has("geo-country-csv") || opts.Has("geo-sha256") || opts.Has("geo-issued-month")) {
            throw std::invalid_argument("LAB admission cannot be combined with Geo data pins");
        }
        active_peer_admission_policy = std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(
            cybou::p2p::PeerAdmissionPolicy::Lab());
        return;
    }
    if (mode != "france") throw std::invalid_argument("peer admission must be france or lab");
    const bool has_csv = opts.Has("geo-country-csv");
    const bool has_sha256 = opts.Has("geo-sha256");
    const bool has_month = opts.Has("geo-issued-month");
    if (!has_csv && !has_sha256 && !has_month) {
        auto updater = cybou::p2p::GeoDatabaseUpdater::Start(data_directory / "geo");
        active_peer_admission_policy = std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(
            cybou::p2p::PeerAdmissionPolicy::PublicWithUpdater(std::move(updater)));
        return;
    }
    if (!has_csv || !has_sha256 || !has_month) {
        throw std::invalid_argument("offline Geo override requires --geo-country-csv, --geo-sha256, and --geo-issued-month");
    }
    const auto issued_month = cybou::p2p::FrenchIpDataset::ParseIssuedMonth(opts.Require("geo-issued-month"));
    if (!issued_month) throw std::invalid_argument("Geo issued month must use YYYY-MM");
    const auto dataset = cybou::p2p::FrenchIpDataset::LoadDbIpCountryCsv(
        opts.Require("geo-country-csv"), Sha256Pin(opts.Require("geo-sha256")), *issued_month);
    if (!dataset) throw std::invalid_argument("France Geo CSV is missing, corrupt, expired, future-dated, or has the wrong SHA-256");
    active_peer_admission_policy = std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(
        cybou::p2p::PeerAdmissionPolicy::Public(dataset));
    std::cerr << "Peer Geo data: DB-IP Lite IP to Country; attribution: DB-IP.com (CC BY 4.0)\n";
}

int PrintPeerSubmitResult(const cybou::p2p::PeerSubmitResult& result)
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

int Execute(const int argc, char* argv[])
{
    if (argc == 2 && std::string_view{argv[1]} == "network-bootstrap") {
        // Optional CYP2 peer hints. The bootstrap protocol has separate
        // locators and this legacy DEV deployment is not a CYP2 peer.
        for (const auto& endpoint : cybou::CYBOU_DEV_BOOTSTRAP_NODES) {
            std::cout << endpoint.host << ':' << endpoint.p2p_port << '\n';
        }
        return 0;
    }
    if ((argc == 4 || argc == 6) && std::string_view{argv[1]} == "network-init") {
        auto poa_seed_bytes = ReadFile(argv[3], 32);
        if (poa_seed_bytes.size() != 32) throw std::runtime_error("PoA finalizer key file must contain exactly 32 raw bytes");
        std::array<unsigned char, 32> poa_seed{};
        std::copy(poa_seed_bytes.begin(), poa_seed_bytes.end(), poa_seed.begin());
        cybou::crypto::CleanseMemory(poa_seed_bytes.data(), poa_seed_bytes.size());
        const auto poa_finalizer_key = cybou::DeriveIdentityPublicKey(poa_seed, cybou::IdentityKeyPurpose::POA_FINALIZER);
        // The operator recovery phrase is also the authority Identity: its
        // AccountCreate later claims the genesis allocation (Balance + name).
        const auto authority_recovery_key = cybou::DeriveIdentityPublicKey(poa_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
        cybou::crypto::CleanseMemory(poa_seed.data(), poa_seed.size());
        if (!poa_finalizer_key) throw std::runtime_error("cannot derive PoA finalizer public key");

        auto genesis = cybou::CreateDevGenesisState();
        if (argc == 6) {
            const auto recovery_id = authority_recovery_key ? cybou::ComputeRecoveryKeyId(*authority_recovery_key) : std::nullopt;
            if (!recovery_id) throw std::runtime_error("cannot derive authority recovery key");
            const auto balance = std::stoull(argv[4]);
            genesis.genesis_allocations.emplace(*recovery_id, cybou::GenesisAllocation{.balance = balance, .label = argv[5]});
            if (cybou::ValidateCybouState(genesis) != cybou::StateValidationError::NONE) {
                throw std::runtime_error("invalid authority genesis allocation (balance or name)");
            }
        }
        const auto definition = cybou::CreateDevNetworkDefinition(genesis, *poa_finalizer_key);
        auto definition_bytes = cybou::SerializeNetworkDefinition(definition);
        auto state_bytes = cybou::SerializeCybouState(genesis);
        if (!state_bytes) throw std::runtime_error("cannot serialize genesis state");
        std::vector<unsigned char> out{'C', 'Y', 'N', '1'};
        PutU32(out, definition_bytes.size());
        out.insert(out.end(), definition_bytes.begin(), definition_bytes.end());
        PutU32(out, state_bytes->size());
        out.insert(out.end(), state_bytes->begin(), state_bytes->end());
        WriteNewFile(argv[2], out);
        std::cout << "network=" << cybou::NetworkId(definition).GetHex() << '\n';
        return 0;
    }
    if (argc < 5) throw std::runtime_error("invalid command arguments; use --help");
    const auto network = cybou::LoadCybouNetworkFile(argv[2]);
    if (!network) throw std::runtime_error("invalid CYBOU network file");
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
#ifdef _WIN32
    std::signal(SIGBREAK, Stop);
#endif
    if (std::string_view{argv[1]} == "operation-status" && argc == 5) {
        const auto op_id = cybou::ParseUint256UserHex(argv[4]);
        if (!op_id || op_id->IsNull()) throw std::runtime_error("invalid OperationID");
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        config.peer_admission_policy = active_peer_admission_policy;
        config.event_writer = events;
        cybou::CybouNodeService node_service{{.runtime = std::move(config), .genesis = network->genesis}};
        node_service.Start();
        auto& runtime = node_service.Runtime();
        const auto result = runtime.FindFinalizedOperation(*op_id);
        if (result.status == cybou::FinalizedOperationLookupStatus::FOUND) {
            std::cout << "status=finalized operation=" << op_id->GetHex()
                      << " height=" << result.height
                      << " index=" << result.operation_index
                      << " block=" << result.block_id.GetHex() << std::endl;
            return 0;
        }
        std::cout << "status=" << (result.status == cybou::FinalizedOperationLookupStatus::NOT_FOUND ?
            "not-found" : "history-unavailable") << " operation=" << op_id->GetHex()
                  << " scanned_height=" << result.scanned_height << std::endl;
        return result.status == cybou::FinalizedOperationLookupStatus::NOT_FOUND ? 1 : 2;
    }
    if (std::string_view{argv[1]} == "network-probe" && argc == 6) {
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        config.peer_admission_policy = active_peer_admission_policy;
        config.event_writer = events;
        cybou::CybouNodeService node_service{{.runtime = std::move(config), .genesis = network->genesis}};
        node_service.Start();
        auto& runtime = node_service.Runtime();
        cybou::p2p::PeerManager peers{runtime};
        if (!peers.Connect(argv[4], Port(argv[5])) || peers.PingAll() != 1) {
            throw std::runtime_error("P2P handshake or ping failed");
        }
        const auto peer = peers.Peers().front();
        std::cout << "peer=" << peer.address << ':' << peer.port
                  << " height=" << peer.hello.finalized_height
                  << " capabilities=" << peer.hello.capabilities << std::endl;
        return 0;
    }
    if (std::string_view{argv[1]} == "network-sync" && argc == 7) {
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        config.peer_admission_policy = active_peer_admission_policy;
        config.event_writer = events;
        cybou::CybouNodeService node_service{{.runtime = std::move(config), .genesis = network->genesis}};
        node_service.Start();
        auto& runtime = node_service.Runtime();
        cybou::p2p::PeerManager peers{runtime};
        const auto port = Port(argv[5]);
        if (!peers.Connect(argv[4], port)) throw std::runtime_error("P2P handshake failed");
        const auto result = peers.SyncFromPeer(argv[4], port, PositiveCount(argv[6]));
        if (!result.IsConnected()) throw std::runtime_error("P2P block sync failed");
        std::cout << "height=" << *runtime.GetFinalizedHeight()
                  << " applied=" << result.blocks_applied << std::endl;
        return 0;
    }
    if (std::string_view{argv[1]} == "network-follow" && (argc == 6 || argc == 7)) {
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        config.peer_admission_policy = active_peer_admission_policy;
        config.event_writer = events;
        cybou::CybouNodeService node_service{{.runtime = std::move(config), .genesis = network->genesis}};
        node_service.Start();
        auto& runtime = node_service.Runtime();
        const auto until_height = argc == 7 ? std::optional<uint64_t>{TargetHeight(argv[6])} : std::nullopt;
        const std::string host{argv[4]};
        const auto port = Port(argv[5]);
        cybou::p2p::PeerManager peers{runtime};
        while (!stopping) {
            const auto status = runtime.GetStatus();
            if (!status.is_initialized) throw std::runtime_error("observer state unavailable");
            if (until_height && status.finalized_height >= *until_height) return 0;
            if (peers.ConnectedCount() == 0 && !peers.Connect(host, port)) {
                if (peers.LastConnectStatus() != cybou::p2p::PeerConnectStatus::UNAVAILABLE) {
                    throw std::runtime_error("P2P peer handshake or local setup failed");
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            const uint64_t batch = until_height ? std::min<uint64_t>(100, *until_height - status.finalized_height) : 100;
            const auto result = peers.SyncFromPeer(host, port, batch);
            if (result.blocks_applied > 0) {
                std::cout << "height=" << *runtime.GetFinalizedHeight() << std::endl;
            }
            if (result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR ||
                result.status == cybou::SyncPeerStatus::NETWORK_MISMATCH) {
                throw std::runtime_error("P2P block verification failed");
            }
            if (result.status == cybou::SyncPeerStatus::CONNECTION_FAILED) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } else if (result.status == cybou::SyncPeerStatus::UP_TO_DATE) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
        return 0;
    }
    if (std::string_view{argv[1]} == "network-follow-peers" && (argc == 5 || argc == 6)) {
        const auto endpoints = ReadPeerEndpoints(argv[4]);
        const auto until_height = argc == 6 ? std::optional<uint64_t>{TargetHeight(argv[5])} : std::nullopt;
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        config.peer_admission_policy = active_peer_admission_policy;
        config.event_writer = events;
        cybou::CybouNodeService node_service{{.runtime = std::move(config), .genesis = network->genesis}};
        node_service.Start();
        auto& runtime = node_service.Runtime();
        cybou::p2p::PeerManager peers{runtime};
        std::vector<bool> rejected(endpoints.size(), false);
        std::vector<std::chrono::steady_clock::time_point> retry_after(endpoints.size());
        size_t preferred_peer{0};
        while (!stopping) {
            const auto status = runtime.GetStatus();
            if (!status.is_initialized) throw std::runtime_error("observer state unavailable");
            if (until_height && status.finalized_height >= *until_height) return 0;
            bool progress{false};
            for (size_t offset = 0; offset < endpoints.size() && !stopping; ++offset) {
                const size_t i = (preferred_peer + offset) % endpoints.size();
                if (rejected[i]) continue;
                if (std::chrono::steady_clock::now() < retry_after[i]) continue;
                const auto& [host, port] = endpoints[i];
                const auto connected = peers.Peers();
                const bool present = std::any_of(connected.begin(), connected.end(), [&](const auto& peer) {
                    return peer.address == host && peer.port == port;
                });
                if (!present && !peers.Connect(host, port)) {
                    if (peers.LastConnectStatus() != cybou::p2p::PeerConnectStatus::UNAVAILABLE) {
                        rejected[i] = true;
                        std::cerr << "P2P peer rejected: " << host << ':' << port << '\n';
                    } else {
                        retry_after[i] = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    }
                    continue;
                }
                const auto height = runtime.GetFinalizedHeight();
                if (!height) throw std::runtime_error("observer height unavailable");
                const uint64_t batch = until_height ? std::min<uint64_t>(100, *until_height - *height) : 100;
                const auto result = peers.SyncFromPeer(host, port, batch);
                if (result.blocks_applied > 0) {
                    progress = true;
                    std::cout << "height=" << *runtime.GetFinalizedHeight()
                              << " peer=" << host << ':' << port << std::endl;
                }
                if (result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR ||
                    result.status == cybou::SyncPeerStatus::NETWORK_MISMATCH) {
                    rejected[i] = true;
                    std::cerr << "P2P peer failed block verification: " << host << ':' << port << '\n';
                }
                if (result.status == cybou::SyncPeerStatus::CONNECTION_FAILED) {
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
    if (std::string_view{argv[1]} == "operation-submit-peers" && argc == 6) {
        const auto endpoints = ReadPeerEndpoints(argv[4]);
        const auto bytes = ReadFile(argv[5], cybou::MAX_OPERATION_PAYLOAD_BYTES);
        const auto operation = cybou::DeserializeProtocolOperation(bytes);
        if (!operation) throw std::runtime_error("invalid operation file");
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        config.peer_admission_policy = active_peer_admission_policy;
        config.event_writer = events;
        cybou::CybouNodeService node_service{{.runtime = std::move(config), .genesis = network->genesis}};
        node_service.Start();
        auto& runtime = node_service.Runtime();
        cybou::p2p::PeerManager peers{runtime};
        const auto result = peers.SubmitOperationToAny(endpoints, *operation);
        return PrintPeerSubmitResult(result);
    }
    if (std::string_view{argv[1]} == "operation-submit" && argc == 7) {
        const auto bytes = ReadFile(argv[6], cybou::MAX_OPERATION_PAYLOAD_BYTES);
        const auto operation = cybou::DeserializeProtocolOperation(bytes);
        if (!operation) throw std::runtime_error("invalid operation file");
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        config.peer_admission_policy = active_peer_admission_policy;
        config.event_writer = events;
        cybou::CybouNodeService node_service{{.runtime = std::move(config), .genesis = network->genesis}};
        node_service.Start();
        auto& runtime = node_service.Runtime();
        cybou::p2p::PeerManager peers{runtime};
        const auto port = Port(argv[5]);
        return PrintPeerSubmitResult(peers.SubmitOperationToAny({{argv[4], port}}, *operation));
    }
    if (std::string_view{argv[1]} == "finalizer-run" && argc >= 7 && argc <= 10) {
        auto key_file = cybou::ReadSecretFile(argv[4], 32);
        if (!key_file || key_file->size() != 32) throw std::runtime_error("PoA finalizer key file must be private and contain exactly 32 raw bytes");
        std::array<unsigned char, 32> key{};
        std::copy(key_file->begin(), key_file->end(), key.begin());
        cybou::crypto::CleanseMemory(key_file->data(), key_file->size());
        cybou::Secret32 finalizer_key{key};
        cybou::crypto::CleanseMemory(key.data(), key.size());

        const auto p2p_port = Port(argv[6]);
        const auto interval_ms = argc >= 8 ? PositiveCount(argv[7]) : 1000;
        if (interval_ms > 60000) throw std::runtime_error("block interval exceeds 60 seconds");
        const auto bind_address = boost::asio::ip::make_address(argv[5]);

        cybou::NodeRuntimeConfig config{
            .network_definition = network->definition,
            .data_dir = argv[3],
            .poa_finalizer_recovery_entropy = std::move(finalizer_key),
            .db_cache_bytes = 8 << 20,
        };
        ConfigureBootstrapIdentity(config);
        if (argc == 10) {
            config.storage_enabled = true;
            config.storage_capacity_bytes = CapacityBytes(argv[9]);
        }
        config.local_p2p_endpoint = advertised_endpoint.value_or(std::make_pair(bind_address.to_string(), p2p_port));
        config.peer_admission_policy = active_peer_admission_policy;
        config.event_writer = events;
        cybou::CybouNodeService node_service{{
            .runtime = std::move(config),
            .genesis = network->genesis,
        }};
        node_service.Start();
        const auto gossip_endpoints = argc >= 9 ? ReadPeerEndpoints(argv[8]) :
            std::vector<std::pair<std::string, uint16_t>>{};
        std::jthread monitor;
        if (events) monitor = std::jthread{[&](std::stop_token stop) {
            while (!stop.stop_requested() && !stopping) {
                const auto d = node_service.Runtime().GetDiagnostics();
                events->Observe(d);
                if (!events->Good()) stopping = true;
                for (int i=0;i<20 && !stop.stop_requested() && !stopping;++i) std::this_thread::sleep_for(std::chrono::milliseconds{100});
            }
        }};
        RequireBootstrapIdentityAuthorized(node_service.Runtime());
        const auto result = node_service.RunFinalizer(cybou::CybouFinalizerServiceConfig{
            .bind_address = bind_address.to_string(), .p2p_port = p2p_port,
            .block_interval_ms = interval_ms, .peers = gossip_endpoints,
        }, stopping);
        return events && !events->Good() ? 2 : result;
    }
    if ((std::string_view{argv[1]} == "provider-run" || std::string_view{argv[1]} == "observer-run") && argc == 9) {
        // Non-finalizer full node that verifies every finalized block from its
        // peer and retains authorized encrypted chunks for other Identities.
        const std::string peer_host{argv[4]};
        const auto peer_port = Port(argv[5]);
        const auto bind_address = boost::asio::ip::make_address(argv[6]);
        const auto listen_port = std::string_view{argv[7]} == "0" ? uint16_t{0} : Port(argv[7]);
        const bool provider = std::string_view{argv[1]} == "provider-run";
        const auto capacity = provider ? CapacityBytes(argv[8]) : 0;
        const auto listen = std::make_pair(bind_address.to_string(), listen_port);
        cybou::NodeRuntimeConfig config{
            .network_definition = network->definition,
            .data_dir = argv[3],
            .p2p_endpoint = std::make_pair(peer_host, peer_port),
            .local_p2p_endpoint = advertised_endpoint.value_or(listen),
            .db_cache_bytes = 8 << 20,
            .storage_enabled = provider,
            .storage_capacity_bytes = capacity,
        };
        ConfigureBootstrapIdentity(config);
        config.peer_admission_policy = active_peer_admission_policy;
        config.event_writer = events;
        cybou::CybouNodeService node_service{{
            .runtime = std::move(config),
            .genesis = network->genesis,
        }};
        node_service.Start();
        RequireBootstrapIdentityAuthorized(node_service.Runtime());
        if (argc == 9 && explicit_peers.size()) node_service.Runtime().SetExplicitPeerEndpoints(explicit_peers);
        std::atomic<std::uint64_t> last_height{0};
        node_service.StartNetwork(cybou::CybouNetworkServiceConfig{.sync_interval = std::chrono::milliseconds{1000}, .listen_endpoint = listen_port ? std::optional{listen} : std::nullopt},
            [&last_height, &node_service](const cybou::SyncPeerResult& sync, const cybou::NodeRuntimeStatus& status, size_t peers) {
                if (status.runtime_state == cybou::NodeRuntimeState::NETWORK_MISMATCH ||
                    status.runtime_state == cybou::NodeRuntimeState::CORRUPT ||
                    status.runtime_state == cybou::NodeRuntimeState::SAFETY_HALTED) {
                    std::cerr << "provider state unavailable" << std::endl;
                    stopping = true;
                    return false;
                }
                if (status.finalized_height != last_height.exchange(status.finalized_height)) {
                    std::cout << "height=" << status.finalized_height << " peers=" << peers << std::endl;
                }
                if (events) {
                    const auto d = node_service.Runtime().GetDiagnostics();
                    events->Write(sync.IsConnected() ? cybou::NodeEvent::sync_progress : cybou::NodeEvent::sync_failed,
                        {{"height",d.height},{"error_code",std::uint64_t{static_cast<unsigned>(sync.status)}}});
                    events->Observe(d);
                    if (!events->Good()) { stopping = true; return false; }
                }
                return true;
            });
        while (!stopping.load()) std::this_thread::sleep_for(std::chrono::milliseconds{250});
        node_service.StopNetwork();
        const auto final_status = node_service.Runtime().GetStatus();
        return final_status.runtime_state != cybou::NodeRuntimeState::READY || (events && !events->Good()) ? 2 : 0;
    }
    throw std::runtime_error("invalid command or arguments");
}

std::pair<std::string,uint16_t> Endpoint(const std::string& text)
{
    const auto colon = text.rfind(':');
    if (colon == std::string::npos) throw std::invalid_argument("expected numeric IP:port");
    auto host = text.substr(0,colon);
    if (host.starts_with("[") && host.ends_with("]")) host = host.substr(1,host.size()-2);
    host = boost::asio::ip::make_address(host).to_string();
    return {host,Port(text.substr(colon+1).c_str())};
}
const char* Help = R"(CYBOU operator CLI (CYP2 only)
  finalizer run --network FILE --data-dir DIR --key-file FILE --listen IP:PORT
                [--block-interval 1000ms] [--peers FILE] [--event-log FILE] [--event-log-mode minimal|lab]
  provider run  --network FILE --data-dir DIR --peer IP:PORT --listen IP:PORT
                --capacity 20GiB [--peers FILE] [--event-log FILE] [--event-log-mode minimal|lab]
  observer run  --network FILE --data-dir DIR --peer IP:PORT
                [--listen IP:PORT] [--peers FILE] [--event-log FILE] [--event-log-mode minimal|lab]
  Any run command may enable the genesis-granted bootstrap capability with
                --bootstrap-vault FILE --bootstrap-password-file FILE
  network info --network FILE
  network init-dev --network FILE --key-file FILE
         [--authority-balance CYBOU --authority-name LABEL]
  network bootstrap
  network probe --network FILE --data-dir DIR --peer IP:PORT
  network sync --network FILE --data-dir DIR --peer IP:PORT [--count 100]
  network follow --network FILE --data-dir DIR (--peer IP:PORT | --peers FILE)
                 [--until-height N]
  operation submit --network FILE --data-dir DIR (--peer IP:PORT | --peers FILE)
                   --operation-file FILE
  operation status --network FILE --data-dir DIR --operation-id HEX
  doctor --network FILE --data-dir DIR [--listen IP:PORT] [--peers FILE]
         [--key-file FILE]
  storage status --event-log FILE (last complete output-only status sample)
  storage verify --network FILE --data-dir DIR --chunk-id HEX [--peer IP:PORT]
  storage placement --network FILE --data-dir DIR --vault FILE --password-file FILE
                    --operation-id HEX [--replicas 1|2] (offline Identity projection)
No positional arguments or legacy command aliases. Secrets are file inputs.
Event JSONL is output only. Peer advertised heights are not canonical evidence.
Network-facing commands require --peer-admission france; CYBOU downloads and validates the current DB-IP Lite
country database automatically. Optional offline overrides: --geo-country-csv FILE --geo-sha256 HEX
--geo-issued-month YYYY-MM. Use --peer-admission lab only for loopback/private LAB peers.
)";
int Doctor(const cybou::cli::Options& opts)
{
    opts.Allow({"network","data-dir","listen","peers","key-file"});
    const auto net = cybou::LoadCybouNetworkFile(opts.Require("network"));
    if (!net) throw std::runtime_error("doctor: invalid network/genesis");
    std::cout << "Network OK\nNetwork ID " << cybou::NetworkId(net->definition).GetHex() << '\n';
    auto dir = std::filesystem::absolute(opts.Require("data-dir"));
    auto parent = dir;
    while (!std::filesystem::exists(parent)) parent = parent.parent_path();
    const auto perms = std::filesystem::status(parent).permissions();
    if ((perms & (std::filesystem::perms::owner_write | std::filesystem::perms::group_write |
                  std::filesystem::perms::others_write)) == std::filesystem::perms::none)
        throw std::runtime_error("doctor: directory not writable");
    std::cout << "Data dir OK\nDisk free " << std::filesystem::space(parent).available << " bytes\n";
    if (opts.Has("listen")) {
        const auto endpoint = Endpoint(opts.Get("listen"));
        boost::asio::io_context io;
        boost::asio::ip::tcp::acceptor port{io};
        port.open(boost::asio::ip::make_address(endpoint.first).is_v4() ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6());
        port.bind({boost::asio::ip::make_address(endpoint.first),endpoint.second});
        std::cout << "Listen OK\n";
    }
    if (opts.Has("peers")) { ReadPeerEndpoints(opts.Get("peers")); std::cout << "Peers OK\n"; }
    if (opts.Has("key-file")) {
        auto bytes = ReadFile(opts.Get("key-file"),32);
        if (bytes.size()!=32) throw std::runtime_error("doctor: invalid finalizer seed size");
#ifndef _WIN32
        const auto permissions = std::filesystem::status(opts.Get("key-file")).permissions();
        if ((permissions & (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) != std::filesystem::perms::none)
            throw std::runtime_error("doctor: finalizer seed must be private (0600)");
#endif
        std::array<unsigned char,32> seed{};
        std::copy(bytes.begin(),bytes.end(),seed.begin());
        cybou::crypto::CleanseMemory(bytes.data(),bytes.size());
        auto key = cybou::DeriveIdentityPublicKey(seed,cybou::IdentityKeyPurpose::POA_FINALIZER);
        cybou::crypto::CleanseMemory(seed.data(),seed.size());
        if (!key || *key != net->definition.poa_finalizer_public_key) throw std::runtime_error("doctor: wrong PoA key");
        std::cout << "PoA key OK\n";
    }
    // LevelDB has no read-only open. Inspect a stable private COPY, never recover
    // or lock the operator's DB. Refuse snapshots that change during copying.
    if (std::filesystem::exists(dir / "CURRENT")) {
        auto temp = std::filesystem::temp_directory_path() / ("cybou-doctor-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(temp)) throw std::runtime_error("doctor: cannot create snapshot");
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path,ec); } } cleanup{temp};
        std::map<std::filesystem::path,std::pair<uintmax_t,std::filesystem::file_time_type>> before;
        std::uint64_t total{0};
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_symlink()) throw std::runtime_error("doctor: DB symlink rejected");
            if (!entry.is_regular_file() || entry.path().filename()=="LOCK") continue;
            const auto size = entry.file_size();
            if (size > (2ULL<<30) - total) throw std::runtime_error("doctor: snapshot exceeds 2 GiB");
            total += size;
            before[entry.path()]={size,entry.last_write_time()};
            std::filesystem::copy_file(entry.path(),temp/entry.path().filename());
        }
        for (const auto& [path,stamp] : before)
            if (std::filesystem::file_size(path)!=stamp.first || std::filesystem::last_write_time(path)!=stamp.second)
                throw std::runtime_error("doctor: DB changed during read-only snapshot; retry while stopped");
        cybou::CybouNodeRuntime copy{{.network_definition=net->definition,.data_dir=temp}};
        const auto status = copy.GetStatus();
        if (!status.is_initialized || status.poa_safety_halted) throw std::runtime_error("doctor: foreign/corrupt/halted DB");
        std::cout << "State OK height=" << status.finalized_height << '\n';
    } else if (std::filesystem::exists(dir) && !std::filesystem::is_empty(dir)) {
        throw std::runtime_error("doctor: non-empty directory without canonical DB");
    } else std::cout << "State EMPTY\n";
    std::cout << "Result READY\n";
    return 0;
}

int StorageCommand(const std::string& action, const cybou::cli::Options& opts)
{
    if (action=="status") {
        opts.Allow({"event-log"});
        std::ifstream file{opts.Require("event-log"),std::ios::binary};
        if (!file) throw std::runtime_error("cannot open output-only event log");
        file.seekg(0,std::ios::end); const auto size=file.tellg();
        file.seekg(std::max<std::streamoff>(0,static_cast<std::streamoff>(size)-65536));
        std::string line,last;
        while (std::getline(file,line)) if (line.find("\"event\":\"node_status\"")!=std::string::npos && line.ends_with('}')) last=line;
        if (last.empty()) throw std::runtime_error("no complete recent node_status sample");
        std::cout << last << '\n'; return 0;
    }
    opts.Allow({"network","data-dir","peer","chunk-id","vault","password-file","operation-id","replicas",
        "peer-admission","geo-country-csv","geo-sha256","geo-issued-month"});
    if (opts.Has("peer")) ConfigurePeerAdmission(opts, opts.Require("data-dir"));
    auto net=cybou::LoadCybouNetworkFile(opts.Require("network"));
    if (!net) throw std::runtime_error("invalid network");
    cybou::NodeRuntimeConfig config{.network_definition=net->definition,.data_dir=opts.Require("data-dir")};
    config.peer_admission_policy = active_peer_admission_policy;
    if (opts.Has("peer")) config.p2p_endpoint=Endpoint(opts.Get("peer"));
    cybou::CybouNodeService node{{.runtime=std::move(config),.genesis=net->genesis}};
    node.Start(); auto& runtime=node.Runtime();
    if (action=="verify") {
        auto hex=cybou::ParseUint256UserHex(opts.Require("chunk-id"));
        if (!hex) throw std::runtime_error("invalid ChunkID");
        // ChunkID uses displayed raw BLAKE3 bytes, not uint256 display order.
        const auto text=opts.Get("chunk-id");
        if (text.size()!=64) throw std::runtime_error("ChunkID must be 64 raw hex characters");
        cybou::ChunkId chunk{};
        for (size_t i=0;i<32;++i) {
            auto digit=[](char c)->unsigned { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; throw std::runtime_error("invalid hex"); };
            chunk[i]=static_cast<unsigned char>((digit(text[2*i])<<4)|digit(text[2*i+1]));
        }
        auto bytes=runtime.GetChunkBlobStore().Get(chunk);
        if (!bytes && opts.Has("peer")) {
            runtime.SyncFromConfiguredPeer(100);
            for (const auto& peer : runtime.StoragePeerEndpoints()) {
                bytes=runtime.GetChunkFromStoragePeer(peer.address,peer.port,peer.provider_id,chunk);
                if(bytes)break;
            }
        }
        if (!bytes || cybou::ComputeChunkId(*bytes)!=chunk) throw std::runtime_error("chunk absent or BLAKE3 verification failed");
        std::cout << "verified chunk=" << text << " bytes=" << bytes->size() << '\n'; return 0;
    }
    if (action!="placement") throw std::invalid_argument("unknown storage command");
    cybou::CybouIdentityService identity{runtime,opts.Require("vault")};
    auto password_bytes=cybou::ReadSecretFile(opts.Require("password-file"),1024);
    if (!password_bytes) throw std::runtime_error("password file must be a private regular file");
    std::string password(password_bytes->begin(),password_bytes->end());
    cybou::crypto::CleanseMemory(password_bytes->data(),password_bytes->size());
    const bool unlocked=identity.LoadVault(password);
    cybou::crypto::CleanseMemory(password.data(),password.size());
    if (!unlocked || !identity.GetAccountId()) throw std::runtime_error("cannot unlock Identity vault");
    auto operation=cybou::ParseUint256UserHex(opts.Require("operation-id"));
    if (!operation) throw std::runtime_error("invalid OperationID");
    cybou::PrivateApplicationStore db{identity.GetKeyStore(),cybou::IdentityDataDirectory(opts.Require("data-dir"),*identity.GetAccountId())};
    cybou::RuntimeStorageTransport transport{runtime};
    cybou::StorageService storage{runtime,transport,db,static_cast<uint8_t>(cybou::cli::Number(opts.Get("replicas","1"),1,2))};
    const auto placement=storage.DescribePlacement(*operation);
    const auto durability=storage.GetDurability(*operation);
    if (!placement || !durability) throw std::runtime_error("unknown private placement");
    std::cout << "operation=" << operation->GetHex() << " chunks=" << placement->leaves.size()
        << " min_remote_replicas=" << durability->min_replicas << " target=" << unsigned(storage.RemoteReplicaTarget()) << '\n';
    return 0;
}
int Main(int argc, char* argv[])
{
    active_peer_admission_policy.reset();
    active_bootstrap_keystore.reset();
    if (argc == 1 || std::any_of(argv+1,argv+argc,[](const char* arg){ return std::string_view{arg}=="--help"; })) {
        std::cout << Help; return 0;
    }
    const std::string group = argv[1];
    if (group=="doctor") return Doctor(cybou::cli::Options{argc,argv,2});
    if (argc<3) throw std::invalid_argument("missing command; use --help");
    const std::string action = argv[2];
    const cybou::cli::Options opts{argc,argv,3};
    if (group=="storage") return StorageCommand(action,opts);
    std::string event_path;
    std::vector<std::string> args{"cybou-node"};
    if (group=="network" && action=="bootstrap") { opts.Allow({}); args.push_back("network-bootstrap"); }
    else if (group=="network" && action=="info") {
        opts.Allow({"network"}); auto net=cybou::LoadCybouNetworkFile(opts.Require("network"));
        if (!net) throw std::runtime_error("invalid network/genesis");
        std::cout << "network_id=" << cybou::NetworkId(net->definition).GetHex()
            << " genesis=" << net->definition.genesis_block_id.GetHex() << '\n'; return 0;
    } else if (group=="network" && action=="init-dev") {
        opts.Allow({"network","key-file","authority-balance","authority-name"});
        args.insert(args.end(),{"network-init",opts.Require("network"),opts.Require("key-file")});
        if (opts.Has("authority-balance") || opts.Has("authority-name")) {
            args.insert(args.end(),{opts.Require("authority-balance"),opts.Require("authority-name")});
        }
    } else if ((group=="finalizer" || group=="provider" || group=="observer") && action=="run") {
        if (group=="finalizer") opts.Allow({"network","data-dir","key-file","listen","block-interval","peers","event-log","event-log-mode","capacity","advertise","bootstrap-vault","bootstrap-password-file",
            "peer-admission","geo-country-csv","geo-sha256","geo-issued-month"});
        else opts.Allow({"network","data-dir","peer","listen","peers","capacity","event-log","event-log-mode","advertise","bootstrap-vault","bootstrap-password-file",
            "peer-admission","geo-country-csv","geo-sha256","geo-issued-month"});
        if (opts.Has("bootstrap-vault") != opts.Has("bootstrap-password-file"))
            throw std::invalid_argument("bootstrap capability requires both --bootstrap-vault and --bootstrap-password-file");
        if (opts.Has("bootstrap-vault")) {
            auto password_bytes = cybou::ReadSecretFile(opts.Get("bootstrap-password-file"), 1024);
            if (!password_bytes) throw std::runtime_error("bootstrap password file must be a private regular file");
            std::string password(password_bytes->begin(), password_bytes->end());
            cybou::crypto::CleanseMemory(password_bytes->data(), password_bytes->size());
            active_bootstrap_keystore = std::make_unique<cybou::CybouKeyStore>();
            const bool unlocked = active_bootstrap_keystore->LoadFromFile(opts.Get("bootstrap-vault"), password);
            cybou::crypto::CleanseMemory(password.data(), password.size());
            if (!unlocked) throw std::runtime_error("cannot unlock bootstrap Identity vault");
        }
        ConfigurePeerAdmission(opts, opts.Require("data-dir"));
        if (opts.Has("event-log-mode") && opts.Get("event-log-mode")!="minimal" && opts.Get("event-log-mode")!="lab")
            throw std::invalid_argument("event log mode must be minimal or lab");
        if (group=="observer" && opts.Has("capacity")) throw std::invalid_argument("observer has no storage role");
        if (opts.Has("event-log")) event_path = opts.Get("event-log");
        args.insert(args.end(),{group+"-run",opts.Require("network"),opts.Require("data-dir")});
        auto listen = opts.Has("listen") ? Endpoint(opts.Get("listen")) : std::pair<std::string,uint16_t>{"0.0.0.0",0};
        if (group!="observer" && !listen.second) throw std::invalid_argument("missing --listen");
        if (opts.Has("advertise")) {
            if (!listen.second) throw std::invalid_argument("--advertise requires --listen");
            advertised_endpoint = Endpoint(opts.Get("advertise"));
        }
        if (opts.Has("peers")) explicit_peers = ReadPeerEndpoints(opts.Get("peers"));
        if (group=="finalizer") {
            const auto interval = cybou::cli::Quantity(opts.Get("block-interval","1000ms"),true);
            if (interval>60000) throw std::invalid_argument("block interval exceeds 60s");
            args.insert(args.end(),{opts.Require("key-file"),listen.first,std::to_string(listen.second),std::to_string(interval)});
            if (opts.Has("peers")) args.push_back(opts.Get("peers"));
            if (opts.Has("capacity")) {
                if (!opts.Has("peers")) throw std::invalid_argument("finalizer storage requires explicit --peers");
                args.push_back(std::to_string(cybou::cli::Quantity(opts.Get("capacity"))));
            }
        } else {
            const auto peer = Endpoint(opts.Require("peer"));
            args.insert(args.end(),{peer.first,std::to_string(peer.second),listen.first,std::to_string(listen.second),
                group=="provider" ? std::to_string(cybou::cli::Quantity(opts.Require("capacity"))) : "0"});
        }
    } else if (group=="operation" && action=="status") {
        opts.Allow({"network","data-dir","operation-id"});
        args.insert(args.end(),{"operation-status",opts.Require("network"),opts.Require("data-dir"),opts.Require("operation-id")});
    } else if ((group=="network" && (action=="probe" || action=="sync" || action=="follow")) || (group=="operation" && action=="submit")) {
        if (action=="probe") opts.Allow({"network","data-dir","peer","peer-admission","geo-country-csv","geo-sha256","geo-issued-month"});
        else if (action=="sync") opts.Allow({"network","data-dir","peer","count","peer-admission","geo-country-csv","geo-sha256","geo-issued-month"});
        else if (action=="follow") opts.Allow({"network","data-dir","peer","peers","until-height","peer-admission","geo-country-csv","geo-sha256","geo-issued-month"});
        else opts.Allow({"network","data-dir","peer","peers","operation-file","peer-admission","geo-country-csv","geo-sha256","geo-issued-month"});
        ConfigurePeerAdmission(opts, opts.Require("data-dir"));
        if (opts.Has("peer") == opts.Has("peers")) throw std::invalid_argument("specify exactly one of --peer or --peers");
        const bool peer_list = opts.Has("peers");
        if (peer_list && (action=="probe" || action=="sync")) throw std::invalid_argument("command requires --peer");
        args.insert(args.end(),{group+"-"+action+(peer_list ? "-peers" : ""),opts.Require("network"),opts.Require("data-dir")});
        if (peer_list) args.push_back(opts.Get("peers"));
        else { const auto peer=Endpoint(opts.Require("peer")); args.insert(args.end(),{peer.first,std::to_string(peer.second)}); }
        if (action=="sync") args.push_back(std::to_string(cybou::cli::Number(opts.Get("count","100"),1,1000000)));
        if (action=="follow" && opts.Has("until-height")) args.push_back(opts.Get("until-height"));
        if (action=="submit") args.push_back(opts.Require("operation-file"));
    } else throw std::invalid_argument("unknown command; use --help");
    if (!event_path.empty()) events = std::make_shared<cybou::EventWriter>(event_path,
        opts.Get("event-log-mode","minimal")=="lab" ? cybou::EventLogMode::LAB : cybou::EventLogMode::MINIMAL);
    std::vector<char*> pointers; for (auto& arg : args) pointers.push_back(arg.data());
    if (events) events->Write(cybou::NodeEvent::node_started,{{"role",group}});
    const auto result=Execute(static_cast<int>(pointers.size()),pointers.data());
    if (events) events->Write(cybou::NodeEvent::node_stopping,{{"role",group}});
    return result;
}
} // namespace

int main(int argc, char* argv[])
{
    try {
        return Main(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "cybou-node: " << e.what() << '\n';
        return 1;
    }
}
