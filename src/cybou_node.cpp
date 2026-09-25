// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/authority_node.h>
#include <cybou/block_feed.h>
#include <cybou/bootstrap_nodes.h>
#include <cybou/network_definition.h>
#include <cybou/node_runtime.h>
#include <cybou/p2p/inbound_server.h>
#include <cybou/p2p/peer_manager.h>
#include <cybou/signing.h>
#include <cybou/validator.h>
#include <dbwrapper.h>
#include <support/cleanse.h>
#include <util/strencodings.h>
#include <util/translation.h>

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
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

const TranslateFn G_TRANSLATION_FUN{nullptr};

namespace {
std::atomic_bool stopping{false};

void Stop(int) { stopping.store(true); }

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

int Main(const int argc, char* argv[])
{
    if (argc == 2 && std::string_view{argv[1]} == "bootstrap") {
        // DEV bootstrap seed list (doc 75). Transport metadata only — trust
        // always comes from the network definition file, never from seeds.
        for (const auto& endpoint : cybou::CYBOU_DEV_BOOTSTRAP_AUTHORITIES) {
            std::cout << endpoint.host << ':' << endpoint.port << '\n';
        }
        return 0;
    }
    if (argc >= 4 && std::string_view{argv[1]} == "init-dev") {
        std::vector<cybou::IdentityHybridPublicKey> validator_keys;
        for (int i = 3; i < argc; ++i) {
            auto key_bytes = ReadFile(argv[i], 32);
            if (key_bytes.size() != 32) throw std::runtime_error("validator key file must contain exactly 32 raw bytes");
            std::array<unsigned char, 32> key{};
            std::copy(key_bytes.begin(), key_bytes.end(), key.begin());
            memory_cleanse(key_bytes.data(), key_bytes.size());
            const auto keypair = cybou::GenerateValidatorKeyPair(key);
            memory_cleanse(key.data(), key.size());
            if (!keypair) throw std::runtime_error("cannot derive validator key pair");
            validator_keys.push_back(keypair->public_key);
        }
        const auto genesis = cybou::CreateDevGenesisState(validator_keys);
        if (!genesis) throw std::runtime_error("invalid or duplicate validator keys");
        const auto definition = cybou::CreateDevNetworkDefinition(*genesis);
        auto definition_bytes = cybou::SerializeNetworkDefinition(definition);
        auto state_bytes = cybou::SerializeCybouState(*genesis);
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
    if (argc < 5) throw std::runtime_error("usage: cybou-node init-dev NETWORK_FILE VALIDATOR_KEY_FILE [MORE_VALIDATOR_KEY_FILES...] | bootstrap | serve NETWORK_FILE DB_DIR KEY_FILE BIND_IP PORT [BLOCK_MS [P2P_PORT [PEERS_FILE]]] | sync NETWORK_FILE DB_DIR [PEER_HOST PORT] COUNT | p2p-probe NETWORK_FILE DB_DIR PEER_IP P2P_PORT | p2p-sync NETWORK_FILE DB_DIR PEER_IP P2P_PORT COUNT | p2p-follow NETWORK_FILE DB_DIR PEER_IP P2P_PORT [UNTIL_HEIGHT] | p2p-follow-peers NETWORK_FILE DB_DIR PEERS_FILE [UNTIL_HEIGHT] | p2p-submit NETWORK_FILE DB_DIR PEER_IP P2P_PORT OP_FILE | p2p-submit-peers NETWORK_FILE DB_DIR PEERS_FILE OP_FILE | operation-status NETWORK_FILE DB_DIR OP_ID");
    const auto network = cybou::LoadCybouNetworkFile(argv[2]);
    if (!network) throw std::runtime_error("invalid CYBOU network file");
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
    if (std::string_view{argv[1]} == "operation-status" && argc == 5) {
        const auto op_id = uint256::FromUserHex(argv[4]);
        if (!op_id || op_id->IsNull()) throw std::runtime_error("invalid OperationID");
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        cybou::CybouNodeRuntime runtime{std::move(config)};
        if (!runtime.GetStatus().is_initialized && !runtime.InitializeGenesis(network->genesis)) {
            throw std::runtime_error("cannot initialize genesis");
        }
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
    if (std::string_view{argv[1]} == "p2p-probe" && argc == 6) {
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        cybou::CybouNodeRuntime runtime{std::move(config)};
        if (!runtime.GetStatus().is_initialized && !runtime.InitializeGenesis(network->genesis)) {
            throw std::runtime_error("cannot initialize genesis");
        }
        cybou::p2p::PeerManager peers{runtime};
        if (!peers.Connect(argv[4], Port(argv[5])) || peers.PingAll() != 1) {
            throw std::runtime_error("P2P handshake or ping failed");
        }
        const auto peer = peers.Peers().front();
        std::cout << "peer=" << peer.address << ':' << peer.port
                  << " height=" << peer.hello.finalized_height << std::endl;
        return 0;
    }
    if (std::string_view{argv[1]} == "p2p-sync" && argc == 7) {
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        cybou::CybouNodeRuntime runtime{std::move(config)};
        if (!runtime.GetStatus().is_initialized && !runtime.InitializeGenesis(network->genesis)) {
            throw std::runtime_error("cannot initialize genesis");
        }
        cybou::p2p::PeerManager peers{runtime};
        const auto port = Port(argv[5]);
        if (!peers.Connect(argv[4], port)) throw std::runtime_error("P2P handshake failed");
        const auto result = peers.SyncFromPeer(argv[4], port, PositiveCount(argv[6]));
        if (!result.IsConnected()) throw std::runtime_error("P2P block sync failed");
        std::cout << "height=" << *runtime.GetFinalizedHeight()
                  << " applied=" << result.blocks_applied << std::endl;
        return 0;
    }
    if (std::string_view{argv[1]} == "p2p-follow" && (argc == 6 || argc == 7)) {
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        cybou::CybouNodeRuntime runtime{std::move(config)};
        if (!runtime.GetStatus().is_initialized && !runtime.InitializeGenesis(network->genesis)) {
            throw std::runtime_error("cannot initialize genesis");
        }
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
    if (std::string_view{argv[1]} == "p2p-follow-peers" && (argc == 5 || argc == 6)) {
        const auto endpoints = ReadPeerEndpoints(argv[4]);
        const auto until_height = argc == 6 ? std::optional<uint64_t>{TargetHeight(argv[5])} : std::nullopt;
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        cybou::CybouNodeRuntime runtime{std::move(config)};
        if (!runtime.GetStatus().is_initialized && !runtime.InitializeGenesis(network->genesis)) {
            throw std::runtime_error("cannot initialize genesis");
        }
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
    if (std::string_view{argv[1]} == "p2p-submit-peers" && argc == 6) {
        const auto endpoints = ReadPeerEndpoints(argv[4]);
        const auto bytes = ReadFile(argv[5], cybou::MAX_OPERATION_PAYLOAD_BYTES);
        const auto operation = cybou::DeserializeProtocolOperation(bytes);
        if (!operation) throw std::runtime_error("invalid operation file");
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        cybou::CybouNodeRuntime runtime{std::move(config)};
        if (!runtime.GetStatus().is_initialized && !runtime.InitializeGenesis(network->genesis)) {
            throw std::runtime_error("cannot initialize genesis");
        }
        cybou::p2p::PeerManager peers{runtime};
        const auto result = peers.SubmitOperationToAny(endpoints, *operation);
        return PrintPeerSubmitResult(result);
    }
    if (std::string_view{argv[1]} == "p2p-submit" && argc == 7) {
        const auto bytes = ReadFile(argv[6], cybou::MAX_OPERATION_PAYLOAD_BYTES);
        const auto operation = cybou::DeserializeProtocolOperation(bytes);
        if (!operation) throw std::runtime_error("invalid operation file");
        cybou::NodeRuntimeConfig config{.network_definition = network->definition,
            .data_dir = argv[3], .db_cache_bytes = 8 << 20};
        cybou::CybouNodeRuntime runtime{std::move(config)};
        if (!runtime.GetStatus().is_initialized && !runtime.InitializeGenesis(network->genesis)) {
            throw std::runtime_error("cannot initialize genesis");
        }
        cybou::p2p::PeerManager peers{runtime};
        const auto port = Port(argv[5]);
        return PrintPeerSubmitResult(peers.SubmitOperationToAny({{argv[4], port}}, *operation));
    }
    // Without explicit PEER_HOST PORT, sync follows the DEV bootstrap list
    // (doc 75). The bootstrap endpoint is transport metadata — every block
    // is still verified against the network definition file.
    if (std::string_view{argv[1]} == "sync" && (argc == 5 || argc == 7)) {
        const std::string sync_host =
            argc == 5 ? std::string{cybou::CYBOU_DEV_BOOTSTRAP_AUTHORITIES.front().host} : std::string{argv[4]};
        const uint16_t sync_port =
            argc == 5 ? cybou::CYBOU_DEV_BOOTSTRAP_AUTHORITIES.front().port : Port(argv[5]);
        cybou::NodeRuntimeConfig config{
            .network_definition = network->definition,
            .data_dir = argv[3],
            .db_cache_bytes = 8 << 20,
        };
        cybou::CybouNodeRuntime runtime{std::move(config)};
        if (!runtime.GetStatus().is_initialized) {
            if (!runtime.InitializeGenesis(network->genesis)) throw std::runtime_error("cannot initialize genesis");
        }
        const auto count = PositiveCount(argc == 5 ? argv[4] : argv[6]);
        uint64_t synced{0};
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!stopping && synced < count) {
            if (runtime.SyncFromPeer(sync_host, sync_port, 1).blocks_applied > 0) {
                ++synced;
                std::cout << "height=" << *runtime.GetFinalizedHeight() << std::endl;
                deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            } else {
                if (std::chrono::steady_clock::now() >= deadline) {
                    std::cerr << "sync timed out waiting for next verified block\n";
                    return 1;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
        return synced == count ? 0 : 1;
    }
    if (std::string_view{argv[1]} == "serve" && (argc == 7 || argc == 8 || argc == 9 || argc == 10)) {
        auto key_bytes = ReadFile(argv[4], 32);
        if (key_bytes.size() != 32) throw std::runtime_error("validator key file must contain exactly 32 raw bytes");
        std::array<unsigned char, 32> key{};
        std::copy(key_bytes.begin(), key_bytes.end(), key.begin());
        memory_cleanse(key_bytes.data(), key_bytes.size());

        cybou::NodeRuntimeConfig config{
            .network_definition = network->definition,
            .data_dir = argv[3],
            .validator_private_key = key,
            .db_cache_bytes = 8 << 20,
        };
        memory_cleanse(key.data(), key.size());
        cybou::CybouNodeRuntime runtime{std::move(config)};
        if (!runtime.GetStatus().is_initialized) {
            if (!runtime.InitializeGenesis(network->genesis)) throw std::runtime_error("cannot initialize genesis");
        }
        const auto val_set = runtime.GetValidatorSet();
        if (!val_set || val_set->validators.empty()) {
            throw std::runtime_error("validator set is empty");
        }

        const auto port = Port(argv[6]);
        const auto interval_ms = argc >= 8 ? PositiveCount(argv[7]) : 1000;
        if (interval_ms > 60000) throw std::runtime_error("block interval exceeds 60 seconds");
        boost::asio::io_context io;
        std::optional<cybou::p2p::InboundPeerServer> p2p_server;
        if (argc >= 9) {
            const auto p2p_port = Port(argv[8]);
            if (p2p_port == port) throw std::runtime_error("P2P port must differ from block feed port");
            p2p_server.emplace(runtime, io, boost::asio::ip::tcp::endpoint{
                boost::asio::ip::make_address(argv[5]), p2p_port});
        }
        const auto gossip_endpoints = argc == 10 ? ReadPeerEndpoints(argv[9]) :
            std::vector<std::pair<std::string, uint16_t>>{};
        if (argc == 10) {
            const auto bind_address = boost::asio::ip::make_address(argv[5]);
            const auto own_port = Port(argv[8]);
            for (const auto& [address, peer_port] : gossip_endpoints) {
                if (address == bind_address.to_string() && peer_port == own_port) {
                    throw std::runtime_error("P2P peer list contains this listener");
                }
            }
        }
        boost::asio::ip::tcp::acceptor acceptor(io, {
            boost::asio::ip::make_address(argv[5]), port,
        });
        acceptor.non_blocking(true);
        std::jthread blocks([&] {
            while (!stopping) {
                const auto block = runtime.ProduceBlock();
                if (!block) {
                    std::cerr << "block production stopped\n";
                    stopping = true;
                    break;
                }
                // std::endl, not '\n': under systemd stdout is a pipe and a
                // buffered height line never reaches the journal otherwise.
                std::cout << "height=" << block->block.height << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
            }
        });
        std::optional<std::jthread> p2p_listener;
        if (p2p_server) p2p_listener.emplace([&] { p2p_server->Run(stopping); });
        std::optional<std::jthread> gossip_worker;
        if (!gossip_endpoints.empty()) gossip_worker.emplace([&] {
            cybou::p2p::PeerManager peers{runtime};
            std::map<std::pair<std::string, uint16_t>, std::chrono::steady_clock::time_point> retry_after;
            while (!stopping) {
                for (const auto& [host, peer_port] : gossip_endpoints) {
                    if (stopping) break;
                    const auto connected = peers.Peers();
                    const bool present = std::any_of(connected.begin(), connected.end(), [&](const auto& peer) {
                        return peer.address == host && peer.port == peer_port;
                    });
                    const auto endpoint = std::make_pair(host, peer_port);
                    if (!present && std::chrono::steady_clock::now() >= retry_after[endpoint] &&
                        !peers.Connect(host, peer_port)) {
                        retry_after[endpoint] = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    }
                }
                if (!stopping) {
                    peers.FanoutRecentOperations();
                    peers.PingAll();
                }
                for (int i = 0; i < 5 && !stopping; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            }
        });
        while (!stopping) {
            boost::asio::ip::tcp::socket socket(io);
            boost::system::error_code ec;
            acceptor.accept(socket, ec);
            if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                continue;
            }
            if (ec) {
                stopping = true;
                throw boost::system::system_error(ec);
            }
            cybou::ServeCybouConnection(runtime, socket);
        }
        return 0;
    }
    throw std::runtime_error("invalid command or arguments");
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
