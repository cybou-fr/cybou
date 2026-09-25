// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/peer_manager.h>

#include <cybou/node_runtime.h>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/steady_timer.hpp>
#include <openssl/rand.h>

#include <array>
#include <chrono>
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
    m_last_connect_status = PeerConnectStatus::INVALID_REQUEST;
    if (port == 0 || m_peers.size() >= MAX_OUTBOUND_PEERS) return false;
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(numeric_address, ec);
    if (ec) return false;
    const Endpoint endpoint{address.to_string(), port};
    if (m_peers.contains(endpoint)) return false;
    const auto status = m_runtime.GetStatus();
    if (!status.is_initialized) return false;
    const auto nonce = RandomNonce();
    if (!nonce) { m_last_connect_status = PeerConnectStatus::LOCAL_FAILURE; return false; }
    boost::asio::ip::tcp::socket socket{m_io};
    boost::asio::steady_timer timer{m_io};
    timer.expires_after(std::chrono::seconds(5));
    std::optional<boost::system::error_code> connect_result;
    socket.async_connect({address, port}, [&](const boost::system::error_code& result) {
        connect_result = result;
        timer.cancel();
    });
    timer.async_wait([&](const boost::system::error_code& result) {
        if (!result) {
            boost::system::error_code ignored;
            socket.close(ignored);
        }
    });
    m_io.restart();
    m_io.run();
    if (!connect_result || *connect_result) {
        m_last_connect_status = PeerConnectStatus::UNAVAILABLE;
        return false;
    }
    Hello local{.network_id = status.network_id, .finalized_height = status.finalized_height,
        .finalized_tip = status.finalized_tip, .capabilities = 0, .nonce = *nonce};
    auto peer = std::make_unique<PeerSession>(std::move(socket));
    if (!peer->Handshake(local)) {
        m_last_connect_status = peer->LastHandshakeStatus() == HandshakeStatus::UNAVAILABLE ?
            PeerConnectStatus::UNAVAILABLE : PeerConnectStatus::HANDSHAKE_FAILED;
        return false;
    }
    if (!MatchesKnownFinalizedChain(m_runtime, *peer->Peer())) {
        m_last_connect_status = PeerConnectStatus::HANDSHAKE_FAILED;
        return false;
    }
    m_peers.emplace(endpoint, std::move(peer));
    m_last_connect_status = PeerConnectStatus::CONNECTED;
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
    if (!it->second->Peer() || !(it->second->Peer()->capabilities & CAP_SERVE_BLOCKS)) {
        result.status = SyncPeerStatus::PROTOCOL_ERROR;
        m_peers.erase(it);
        return result;
    }
    result.status = SyncPeerStatus::UP_TO_DATE;
    while (result.blocks_applied < max_blocks) {
        const auto status = m_runtime.GetStatus();
        if (!status.is_initialized || status.finalized_height == std::numeric_limits<uint64_t>::max()) {
            result.status = SyncPeerStatus::PROTOCOL_ERROR;
            break;
        }
        const uint64_t height = status.finalized_height + 1;
        const auto response = it->second->RequestBlock(height);
        if (response.status != BlockRequestStatus::OK && response.status != BlockRequestStatus::NOT_FOUND) {
            result.status = response.status == BlockRequestStatus::UNAVAILABLE ?
                SyncPeerStatus::CONNECTION_FAILED : SyncPeerStatus::PROTOCOL_ERROR;
            m_peers.erase(it);
            break;
        }
        if (response.status == BlockRequestStatus::NOT_FOUND) {
            if (height <= it->second->Peer()->finalized_height) {
                result.status = SyncPeerStatus::CONNECTION_FAILED;
                m_peers.erase(it);
            }
            break;
        }
        const auto block = DeserializeFinalizedBlock(response.bytes);
        const auto& announced = *it->second->Peer();
        if (!block || block->block.height != height ||
            block->certificate.network_id != status.network_id ||
            block->certificate.block_id != ComputeBlockId(block->block) ||
            (height == announced.finalized_height && block->certificate.block_id != announced.finalized_tip) ||
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

OperationSubmitResult PeerManager::SubmitOperation(const std::string& numeric_address, uint16_t port,
    const ProtocolOperation& operation)
{
    const auto op_id = ComputeOperationId(operation).value_or(uint256{});
    const OperationSubmitResult failure{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(numeric_address, ec);
    if (ec) return failure;
    const Endpoint endpoint{address.to_string(), port};
    auto it = m_peers.find(endpoint);
    if (it == m_peers.end()) return failure;
    const auto result = (it->second->Peer() &&
        (it->second->Peer()->capabilities & CAP_OP_INVENTORY)) ?
        it->second->AdvertiseOperation(operation) : it->second->SubmitOperation(operation);
    if (!result) {
        m_peers.erase(it);
        return failure;
    }
    return *result;
}

PeerSubmitResult PeerManager::SubmitOperationToAny(
    const std::vector<std::pair<std::string, uint16_t>>& endpoints,
    const ProtocolOperation& operation)
{
    const auto op_id = ComputeOperationId(operation).value_or(uint256{});
    PeerSubmitResult result{.op_id = op_id, .acknowledgment = std::nullopt,
        .endpoint = std::nullopt, .delivery_uncertain = false};
    if (endpoints.empty() || endpoints.size() > MAX_OUTBOUND_PEERS) return result;
    const auto bytes = SerializeProtocolOperation(operation);
    if (!bytes || bytes->empty() || bytes->size() > MAX_OPERATION_PAYLOAD_BYTES || op_id.IsNull()) return result;
    for (const auto& [host, port] : endpoints) {
        boost::system::error_code ec;
        const auto address = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        const Endpoint endpoint{address.to_string(), port};
        auto it = m_peers.find(endpoint);
        if (it == m_peers.end()) {
            if (!Connect(host, port)) continue;
            it = m_peers.find(endpoint);
        }
        if (it == m_peers.end() || !it->second->Peer() ||
            !(it->second->Peer()->capabilities & CAP_ACCEPT_OPERATIONS)) continue;
        const auto acknowledgment = (it->second->Peer()->capabilities & CAP_OP_INVENTORY) ?
            it->second->AdvertiseOperation(operation) : it->second->SubmitOperation(operation);
        if (!acknowledgment) {
            result.delivery_uncertain = true;
            m_peers.erase(it);
            continue;
        }
        result.acknowledgment = *acknowledgment;
        result.endpoint = endpoint;
        if (*acknowledgment) return result;
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
