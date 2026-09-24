// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/peer_manager.h>

#include <cybou/node_runtime.h>

#include <boost/asio/ip/address.hpp>
#include <openssl/rand.h>

#include <array>
#include <limits>
#include <optional>

namespace cybou::p2p {
namespace {

std::optional<uint64_t> RandomNonce()
{
    std::array<unsigned char, 8> bytes{};
    if (RAND_bytes(bytes.data(), bytes.size()) != 1) return std::nullopt;
    uint64_t nonce{0};
    for (int i = 0; i < 8; ++i) nonce |= uint64_t{bytes[i]} << (8 * i);
    return nonce == 0 ? std::nullopt : std::optional<uint64_t>{nonce};
}

} // namespace

PeerManager::PeerManager(CybouNodeRuntime& runtime) : m_runtime{runtime} {}

bool PeerManager::Connect(const std::string& numeric_address, const uint16_t port)
{
    if (port == 0 || m_peers.size() >= MAX_OUTBOUND_PEERS) return false;
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(numeric_address, ec);
    if (ec) return false;
    const Endpoint endpoint{address.to_string(), port};
    if (m_peers.contains(endpoint)) return false;
    const auto status = m_runtime.GetStatus();
    if (!status.is_initialized) return false;
    const auto nonce = RandomNonce();
    if (!nonce) return false;
    boost::asio::ip::tcp::socket socket{m_io};
    socket.connect({address, port}, ec);
    if (ec) return false;
    Hello local{.network_id = status.network_id, .finalized_height = status.finalized_height,
        .finalized_tip = status.finalized_tip, .capabilities = 0, .nonce = *nonce};
    auto peer = std::make_unique<PeerSession>(std::move(socket));
    if (!peer->Handshake(local)) return false;
    m_peers.emplace(endpoint, std::move(peer));
    return true;
}

size_t PeerManager::PingAll()
{
    size_t healthy{0};
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        const auto nonce = RandomNonce();
        if (!nonce || !it->second->Ping(*nonce)) {
            it = m_peers.erase(it);
        } else {
            ++healthy;
            ++it;
        }
    }
    return healthy;
}

SyncPeerResult PeerManager::SyncFromPeer(const std::string& numeric_address, uint16_t port, uint64_t max_blocks)
{
    SyncPeerResult result;
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(numeric_address, ec);
    if (ec) return result;
    const Endpoint endpoint{address.to_string(), port};
    auto it = m_peers.find(endpoint);
    if (it == m_peers.end()) return result;
    result.status = SyncPeerStatus::UP_TO_DATE;
    while (result.blocks_applied < max_blocks) {
        const auto status = m_runtime.GetStatus();
        if (!status.is_initialized || status.finalized_height == std::numeric_limits<uint64_t>::max()) {
            result.status = SyncPeerStatus::PROTOCOL_ERROR;
            break;
        }
        const uint64_t height = status.finalized_height + 1;
        const auto bytes = it->second->RequestBlock(height);
        if (!bytes) {
            result.status = SyncPeerStatus::CONNECTION_FAILED;
            m_peers.erase(it);
            break;
        }
        if (bytes->empty()) break;
        const auto block = DeserializeFinalizedBlock(*bytes);
        if (!block || block->block.height != height ||
            block->certificate.network_id != status.network_id ||
            block->certificate.block_id != ComputeBlockId(block->block) ||
            !m_runtime.CommitBlock(*block)) {
            result.status = SyncPeerStatus::PROTOCOL_ERROR;
            m_peers.erase(it);
            break;
        }
        ++result.blocks_applied;
        result.status = SyncPeerStatus::BLOCKS_APPLIED;
    }
    return result;
}

std::vector<PeerInfo> PeerManager::Peers() const
{
    std::vector<PeerInfo> peers;
    peers.reserve(m_peers.size());
    for (const auto& [endpoint, session] : m_peers) {
        if (session->Peer()) peers.push_back(PeerInfo{endpoint.first, endpoint.second, *session->Peer()});
    }
    return peers;
}

void PeerManager::DisconnectAll() { m_peers.clear(); }

} // namespace cybou::p2p
