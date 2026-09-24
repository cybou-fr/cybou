// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/authority_node.h>
#include <cybou/block_feed.h>
#include <cybou/bootstrap_nodes.h>
#include <cybou/network_definition.h>
#include <cybou/node_runtime.h>
#include <cybou/signing.h>
#include <cybou/validator.h>
#include <dbwrapper.h>
#include <support/cleanse.h>
#include <util/strencodings.h>
#include <util/translation.h>

#include <boost/asio.hpp>

#include <atomic>
#include <array>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
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
    const auto number = std::stoul(value);
    if (number == 0 || number > std::numeric_limits<uint16_t>::max()) throw std::runtime_error("invalid port");
    return static_cast<uint16_t>(number);
}

uint64_t PositiveCount(const char* value)
{
    const auto count = std::stoull(value);
    if (count == 0 || count > 1'000'000) throw std::runtime_error("invalid count");
    return count;
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
    if (argc < 5) throw std::runtime_error("usage: cybou-node init-dev NETWORK_FILE VALIDATOR_KEY_FILE [MORE_VALIDATOR_KEY_FILES...] | bootstrap | serve NETWORK_FILE DB_DIR KEY_FILE BIND_IP PORT [BLOCK_MS] | sync NETWORK_FILE DB_DIR [PEER_HOST PORT] COUNT");
    const auto network = cybou::LoadCybouNetworkFile(argv[2]);
    if (!network) throw std::runtime_error("invalid CYBOU network file");
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
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
    if (std::string_view{argv[1]} == "serve" && (argc == 7 || argc == 8)) {
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
        const auto interval_ms = argc == 8 ? PositiveCount(argv[7]) : 1000;
        if (interval_ms > 60000) throw std::runtime_error("block interval exceeds 60 seconds");
        boost::asio::io_context io;
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
        while (!stopping) {
            boost::asio::ip::tcp::socket socket(io);
            boost::system::error_code ec;
            acceptor.accept(socket, ec);
            if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                continue;
            }
            if (ec) throw boost::system::system_error(ec);
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
