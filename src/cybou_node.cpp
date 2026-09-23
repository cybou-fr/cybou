// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/authority_node.h>
#include <cybou/block_feed.h>
#include <cybou/network_definition.h>
#include <cybou/signing.h>
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

uint32_t GetU32(const std::vector<unsigned char>& bytes, const size_t offset)
{
    uint32_t value{0};
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t{bytes[offset + i]} << (8 * i);
    return value;
}

struct NetworkFile {
    cybou::CybouNetworkDefinitionV1 definition;
    cybou::CybouState genesis;
};

NetworkFile LoadNetworkFile(const std::filesystem::path& path)
{
    const auto bytes = ReadFile(path, 16 * 1024 * 1024);
    if (bytes.size() < 12 || !std::equal(bytes.begin(), bytes.begin() + 4, "CYN1")) {
        throw std::runtime_error("invalid network file magic");
    }
    const uint32_t definition_size = GetU32(bytes, 4);
    if (definition_size > bytes.size() - 12) throw std::runtime_error("invalid network definition length");
    const size_t state_length_offset = 8 + definition_size;
    const uint32_t state_size = GetU32(bytes, state_length_offset);
    if (state_size != bytes.size() - state_length_offset - 4) throw std::runtime_error("invalid genesis length");
    const auto definition = cybou::DeserializeNetworkDefinition(
        std::span<const unsigned char>{bytes.data() + 8, definition_size});
    const auto genesis = cybou::DeserializeCybouState(
        std::span<const unsigned char>{bytes.data() + state_length_offset + 4, state_size});
    if (!definition || !genesis || cybou::CybouStateHash(*genesis) != definition->genesis_state_root ||
        cybou::ComputeValidatorSetCommitment(genesis->validator_set) != definition->initial_validator_set_commitment) {
        throw std::runtime_error("invalid genesis or network definition");
    }
    return {*definition, *genesis};
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
    if (argc == 3 && std::string_view{argv[1]} == "pubkey") {
        auto key_bytes = ReadFile(argv[2], 32);
        if (key_bytes.size() != 32) throw std::runtime_error("validator key file must contain exactly 32 raw bytes");
        std::array<unsigned char, 32> key{};
        std::copy(key_bytes.begin(), key_bytes.end(), key.begin());
        memory_cleanse(key_bytes.data(), key_bytes.size());
        const auto public_key = cybou::DeriveEd25519PublicKey(key);
        memory_cleanse(key.data(), key.size());
        if (!public_key) throw std::runtime_error("cannot derive validator public key");
        std::cout << public_key->GetHex() << '\n';
        return 0;
    }
    if (argc == 4 && std::string_view{argv[1]} == "init-dev") {
        const auto public_key = uint256::FromUserHex(argv[3]);
        if (!public_key || public_key->IsNull()) throw std::runtime_error("invalid validator public key");
        cybou::CybouState genesis{
            .onboarding_pool = 10'000'000,
            .security_reward_pool = 0,
            .pending_fee_pool = 0,
            .accounts = {},
            .validator_set = {.version = cybou::VALIDATOR_SET_VERSION,
                              .validators = {{.validator_id = *public_key,
                                              .consensus_public_key = *public_key, .weight = 1}}},
        };
        const cybou::CybouNetworkDefinitionV1 definition{
            .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
            .genesis_block_id = cybou::CybouStateHash(genesis),
            .genesis_state_root = cybou::CybouStateHash(genesis),
            .protocol_parameters = cybou::DevProtocolParameters(),
            .initial_validator_set_commitment = cybou::ComputeValidatorSetCommitment(genesis.validator_set),
        };
        auto definition_bytes = cybou::SerializeNetworkDefinition(definition);
        auto state_bytes = cybou::SerializeCybouState(genesis);
        std::vector<unsigned char> out{'C', 'Y', 'N', '1'};
        PutU32(out, definition_bytes.size());
        out.insert(out.end(), definition_bytes.begin(), definition_bytes.end());
        PutU32(out, state_bytes.size());
        out.insert(out.end(), state_bytes.begin(), state_bytes.end());
        WriteNewFile(argv[2], out);
        std::cout << "network=" << cybou::NetworkId(definition).GetHex() << '\n';
        return 0;
    }
    if (argc < 5) throw std::runtime_error("usage: cybou-node pubkey KEY_FILE | init-dev NETWORK_FILE VALIDATOR_PUBKEY_HEX | serve NETWORK_FILE DB_DIR KEY_FILE BIND_IP PORT [BLOCK_MS] | sync NETWORK_FILE DB_DIR PEER_HOST PORT COUNT");
    const auto network = LoadNetworkFile(argv[2]);
    CDBWrapper db{{.path = argv[3], .cache_bytes = 8 << 20, .memory_only = false,
                   .wipe_data = false, .obfuscate = false}};
    cybou::CybouStateStore store{db, network.definition};
    if (!store.GetFinalizedHead()) {
        if (!store.InitializeGenesis(network.genesis)) throw std::runtime_error("cannot initialize genesis");
    }
    if (!store.LoadState() || store.GetStoredNetworkId() != store.GetNetworkId()) {
        throw std::runtime_error("database state or network mismatch");
    }
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
    if (std::string_view{argv[1]} == "sync" && argc == 7) {
        const auto port = Port(argv[5]);
        const auto count = PositiveCount(argv[6]);
        uint64_t synced{0};
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!stopping && synced < count) {
            if (cybou::SyncNextFinalizedBlock(store, argv[4], port)) {
                ++synced;
                std::cout << "height=" << *store.GetFinalizedHeight() << '\n';
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
        const auto expected_key = store.GetValidatorSet()->validators.at(0).consensus_public_key;
        if (cybou::DeriveEd25519PublicKey(key) != expected_key) {
            memory_cleanse(key.data(), key.size());
            throw std::runtime_error("validator key does not match network definition");
        }
        cybou::CybouAuthorityNode producer{store, key};
        memory_cleanse(key.data(), key.size());
        const auto port = Port(argv[6]);
        const auto interval_ms = argc == 8 ? PositiveCount(argv[7]) : 1000;
        if (interval_ms > 60000) throw std::runtime_error("block interval exceeds 60 seconds");
        boost::asio::io_context io;
        boost::asio::ip::tcp::acceptor acceptor(io, {
            boost::asio::ip::make_address(argv[5]), port,
        });
        acceptor.non_blocking(true);
        std::mutex store_mutex;
        std::jthread blocks([&] {
            while (!stopping) {
                {
                    std::lock_guard lock(store_mutex);
                    const auto result = producer.ProduceNextBlock();
                    if (!result) {
                        std::cerr << "block production stopped, error=" << static_cast<int>(result.error) << '\n';
                        stopping = true;
                        break;
                    }
                    std::cout << "height=" << result.finalized_block->block.height << '\n';
                }
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
            std::lock_guard lock(store_mutex);
            cybou::ServeFinalizedBlockRequest(store, socket);
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
