// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/peer_manager.h>

#include <cybou/node_runtime.h>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/steady_timer.hpp>
#include <openssl/rand.h>

#include <algorithm>
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

PeerManager::PeerManager(CybouNodeRuntime& runtime) : m_runtime{runtime}
{
    SetExplicitEndpoints(m_runtime.GetExplicitPeerEndpoints());
}

bool PeerManager::Connect(const std::string& numeric_address, const uint16_t port)
{
    m_last_connect_status = PeerConnectStatus::INVALID_REQUEST;
    if (port == 0) return false;
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(numeric_address, ec);
    if (ec) return false;
    const Endpoint endpoint{address.to_string(), port};
    if (m_peers.contains(endpoint)) return false;
    if (m_peers.size() >= MAX_OUTBOUND_PEERS) {
        if (!m_explicit_endpoints.contains(endpoint)) {
            // Discovered endpoints never displace existing connections.
            m_last_connect_status = PeerConnectStatus::UNAVAILABLE;
            return false;
        }
        // Explicit configured peer endpoint: evict a connected non-explicit peer
        // to make room. Frontier knowledge for the evicted endpoint is kept
        // (its chain only grows, so the knowledge stays valid on reconnect).
        const auto victim = std::find_if(m_peers.begin(), m_peers.end(), [&](const auto& entry) {
            return !m_explicit_endpoints.contains(entry.first);
        });
        if (victim == m_peers.end()) {
            m_last_connect_status = PeerConnectStatus::UNAVAILABLE;
            return false;
        }
        m_announced_blocks.erase(victim->first);
        m_peers.erase(victim);
    }
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
    uint64_t caps = CAP_SERVE_BLOCKS | CAP_BLOCK_INVENTORY | CAP_BLOCK_ANNOUNCEMENTS | CAP_PEER_DISCOVERY;
    if (m_runtime.HasStorageProvider()) caps |= CAP_STORAGE | CAP_STORAGE_PROOFS;
    // This outbound session does not accept operations from the remote peer.
    // The inbound listener advertises CAP_ACCEPT_OPERATIONS and proves the
    // genesis-bound finalizer key on sessions where it serves that role.
    Hello local{.network_id = status.network_id, .finalized_height = status.finalized_height,
        .finalized_tip = status.finalized_tip, .capabilities = caps, .nonce = *nonce};
    auto peer = std::make_unique<PeerSession>(std::move(socket), TransportRole::CLIENT);
    const auto signer = [this](std::span<const unsigned char> message) { return m_runtime.SignProviderProof(message); };
    const auto finalizer_signer = [this](std::span<const unsigned char> message) {
        return m_runtime.SignFinalizerTransportProof(message);
    };
    if (!peer->Handshake(local, signer, finalizer_signer,
            &m_runtime.GetNetworkDefinition().poa_finalizer_public_key)) {
        switch (peer->LastHandshakeStatus()) {
        case HandshakeStatus::UNAVAILABLE: m_last_connect_status = PeerConnectStatus::UNAVAILABLE; break;
        case HandshakeStatus::WRONG_NETWORK: m_last_connect_status = PeerConnectStatus::WRONG_NETWORK; break;
        default: m_last_connect_status = PeerConnectStatus::HANDSHAKE_FAILED; break;
        }
        return false;
    }
    if (!MatchesKnownFinalizedChain(m_runtime, *peer->Peer())) {
        m_last_connect_status = PeerConnectStatus::HANDSHAKE_FAILED;
        return false;
    }
    m_announced_blocks.erase(endpoint);
    // Seed the fanout frontier from the peer's handshake height. This
    // knowledge survives reconnects (a peer's chain only grows), so it is
    // deliberately NOT erased on disconnect or failed consensus sends.
    m_peer_finalized_heights[endpoint] = peer->Peer()->finalized_height;
    m_peers.emplace(endpoint, std::move(peer));
    m_last_connect_status = PeerConnectStatus::CONNECTED;
    return true;
}

void PeerManager::SetExplicitEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints)
{
    m_explicit_endpoints.clear();
    for (const auto& [host, port] : endpoints) {
        if (port == 0) continue;
        boost::system::error_code ec;
        const auto address = boost::asio::ip::make_address(host, ec);
        if (!ec) m_explicit_endpoints.emplace(address.to_string(), port);
    }
}

size_t PeerManager::PingAll()
{
    size_t healthy{0};
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        const auto nonce = RandomNonce();
        if (!nonce || !it->second->Ping(*nonce)) {
            m_announced_blocks.erase(it->first);
            it = m_peers.erase(it);
        } else {
            ++healthy;
            ++it;
        }
    }
    return healthy;
}

size_t PeerManager::PingSome(const size_t max_peers)
{
    if (max_peers == 0 || m_peers.empty()) return 0;
    std::vector<Endpoint> targets;
    auto it = m_ping_cursor ? m_peers.upper_bound(*m_ping_cursor) : m_peers.begin();
    if (it == m_peers.end()) it = m_peers.begin();
    const size_t limit = std::min(max_peers, m_peers.size());
    for (size_t scanned = 0; scanned < m_peers.size() && targets.size() < limit; ++scanned) {
        if (it == m_peers.end()) it = m_peers.begin();
        const auto endpoint = it->first;
        ++it;
        m_ping_cursor = endpoint;
        targets.push_back(endpoint);
    }

    size_t healthy{0};
    for (const auto& endpoint : targets) {
        const auto peer = m_peers.find(endpoint);
        if (peer == m_peers.end()) continue;
        const auto nonce = RandomNonce();
        if (!nonce || !peer->second->Ping(*nonce)) {
            m_announced_blocks.erase(endpoint);
            m_peers.erase(peer);
        } else {
            ++healthy;
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
    std::vector<BlockAnnouncement> inventory;
    size_t inventory_cursor{0};
    // Responses already requested (pipelined) for inventory entries from the cursor on.
    size_t in_flight{0};
    while (result.blocks_applied < max_blocks) {
        const auto status = m_runtime.GetStatus();
        if (!status.is_initialized || status.finalized_height == std::numeric_limits<uint64_t>::max()) {
            result.status = SyncPeerStatus::PROTOCOL_ERROR;
            break;
        }
        const uint64_t height = status.finalized_height + 1;
        const bool use_inventory = (it->second->Peer()->capabilities & CAP_BLOCK_INVENTORY) != 0;
        if (use_inventory) {
            if (inventory_cursor < inventory.size() && inventory[inventory_cursor].height != height) {
                // Our height moved underneath the batch (e.g. a gossiped block): pipelined
                // responses no longer line up, so drop the session and resync cleanly.
                if (in_flight > 0) {
                    result.status = SyncPeerStatus::CONNECTION_FAILED;
                    m_peers.erase(it);
                    break;
                }
                inventory.clear();
                inventory_cursor = 0;
            }
            if (inventory_cursor == inventory.size()) {
                const auto remaining = std::min<uint64_t>(max_blocks - result.blocks_applied, MAX_BLOCK_INVENTORY);
                const auto announced = it->second->RequestBlockInventory(height, static_cast<uint8_t>(remaining));
                if (announced.status == BlockRequestStatus::NOT_FOUND) {
                    if (height <= it->second->Peer()->finalized_height) {
                        result.status = SyncPeerStatus::CONNECTION_FAILED;
                        m_peers.erase(it);
                    } else result.reached_peer_tip = true;
                    break;
                }
                if (announced.status != BlockRequestStatus::OK) {
                    result.status = announced.status == BlockRequestStatus::UNAVAILABLE ?
                        SyncPeerStatus::CONNECTION_FAILED : SyncPeerStatus::PROTOCOL_ERROR;
                    m_peers.erase(it);
                    break;
                }
                inventory = announced.blocks;
                inventory_cursor = 0;
                // Ask for the whole announced batch in one round trip.
                for (const auto& entry : inventory) {
                    if (!it->second->SendBlockRequest(entry.height)) {
                        result.status = SyncPeerStatus::CONNECTION_FAILED;
                        m_peers.erase(it);
                        return result;
                    }
                }
                in_flight = inventory.size();
            }
        }
        const auto response = in_flight > 0 ? it->second->ReadBlockResponse() : it->second->RequestBlock(height);
        if (in_flight > 0) --in_flight;
        if (response.status != BlockRequestStatus::OK && response.status != BlockRequestStatus::NOT_FOUND) {
            result.status = response.status == BlockRequestStatus::UNAVAILABLE ?
                SyncPeerStatus::CONNECTION_FAILED : SyncPeerStatus::PROTOCOL_ERROR;
            m_peers.erase(it);
            break;
        }
        if (response.status == BlockRequestStatus::NOT_FOUND) {
            if (use_inventory || height <= it->second->Peer()->finalized_height) {
                result.status = SyncPeerStatus::CONNECTION_FAILED;
                m_peers.erase(it);
            } else result.reached_peer_tip = true;
            break;
        }
        const auto block = DeserializeFinalizedBlock(response.bytes);
        const auto& announced = *it->second->Peer();
        if (!block || block->block.height != height ||
            block->certificate.network_id != status.network_id ||
            block->certificate.block_id != ComputeBlockId(block->block) ||
            (use_inventory && block->certificate.block_id != inventory[inventory_cursor].block_id) ||
            (height == announced.finalized_height && block->certificate.block_id != announced.finalized_tip) ||
            !m_runtime.CommitBlock(*block)) {
            result.status = SyncPeerStatus::PROTOCOL_ERROR;
            m_peers.erase(it);
            break;
        }
        if (use_inventory) ++inventory_cursor;
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
    const auto result = it->second->SubmitOperation(operation);
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
        const auto acknowledgment = it->second->SubmitOperation(operation);
        if (!acknowledgment) {
            result.delivery_uncertain = true;
            m_peers.erase(it);
            continue;
        }
        result.acknowledgment = *acknowledgment;
        result.endpoint = endpoint;
        if (*acknowledgment) {
            result.delivery_uncertain = false;
            return result;
        }
    }
    return result;
}

size_t PeerManager::FanoutRecentBlocks(size_t max_per_peer)
{
    if (max_per_peer == 0 || max_per_peer > 32) return 0;
    for (auto it = m_announced_blocks.begin(); it != m_announced_blocks.end();) {
        if (!m_peers.contains(it->first)) it = m_announced_blocks.erase(it);
        else ++it;
    }
    const auto recent = m_runtime.RecentFinalizedBlocksForGossip();
    std::set<uint256> live_ids;
    for (const auto& head : recent) live_ids.insert(head.block_id);
    size_t delivered{0};
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        auto& announced = m_announced_blocks[it->first];
        for (auto known = announced.begin(); known != announced.end();) {
            if (!live_ids.contains(*known)) known = announced.erase(known);
            else ++known;
        }
        if (!it->second->Peer() || !(it->second->Peer()->capabilities & CAP_BLOCK_ANNOUNCEMENTS)) {
            ++it;
            continue;
        }
        // Heads at or below the peer's reported finalized height are marked
        // announced without spending the per-cycle offer budget, so the walk
        // reaches the peer's actual frontier instead of stalling on ancient
        // blocks after every announced-set reset.
        const uint64_t peer_frontier = m_peer_finalized_heights[it->first];
        bool disconnected{false};
        size_t offered{0};
        for (const auto& head : recent) {
            if (offered >= max_per_peer) break;
            if (announced.contains(head.block_id)) continue;
            if (head.height <= peer_frontier) {
                announced.insert(head.block_id);
                continue;
            }
            const auto block = m_runtime.GetBlockAtHeight(head.height);
            if (!block || ComputeBlockId(block->block) != head.block_id) continue;
            ++offered;
            uint64_t peer_height{0};
            const auto response = it->second->AdvertiseBlock({head.height, head.block_id}, *block, peer_height);
            if (!response) {
                disconnected = true;
                break;
            }
            if (peer_height > 0) m_peer_finalized_heights[it->first] = peer_height;
            if (*response != BlockAnnounceResult::GAP) {
                announced.insert(head.block_id);
                ++delivered;
            } else {
                break;
            }
        }
        if (disconnected) {
            m_announced_blocks.erase(it->first);
            it = m_peers.erase(it);
        } else {
            ++it;
        }
    }
    return delivered;
}

std::vector<PeerInfo> PeerManager::Peers() const
{
    std::vector<PeerInfo> peers;
    peers.reserve(m_peers.size());
    for (const auto& [endpoint, session] : m_peers) {
        if (session->Peer()) {
            peers.push_back(PeerInfo{endpoint.first, endpoint.second, *session->Peer(), session->PeerProviderId(),
                session->PeerFinalizerAuthenticated()});
        }
    }
    return peers;
}

std::vector<PeerInfo> PeerManager::AuthenticatedFinalizerSessions() const
{
    std::vector<PeerInfo> sessions;
    for (const auto& [endpoint, session] : m_peers) {
        if (session->Peer() && session->PeerFinalizerAuthenticated()) {
            sessions.push_back(PeerInfo{endpoint.first, endpoint.second, *session->Peer(),
                session->PeerProviderId(), true});
        }
    }
    return sessions;
}

std::vector<PeerInfo> PeerManager::StoragePeers() const
{
    std::vector<PeerInfo> peers;
    for (const auto& [endpoint, session] : m_peers) {
        if (session->Peer() && (session->Peer()->capabilities & CAP_STORAGE) && session->PeerProviderId()) {
            peers.push_back(PeerInfo{endpoint.first, endpoint.second, *session->Peer(), session->PeerProviderId(),
                session->PeerFinalizerAuthenticated()});
        }
    }
    return peers;
}

std::optional<ChunkAdmissionResult> PeerManager::PutAuthorizedChunk(
    const std::string& address, const uint16_t port, const ProviderId& provider_id,
    const uint256& publication_operation_id, const ChunkId& chunk_id, const std::span<const unsigned char> stored_bytes,
    const ChunkAuthorizationProof& proof)
{
    Endpoint endpoint;
    auto* session = FindStorageSession(address, port, provider_id, &endpoint);
    if (!session) return std::nullopt;
    auto result = session->PutAuthorizedChunk(publication_operation_id, chunk_id, stored_bytes, proof);
    if (!result) {
        m_peers.erase(endpoint);
        m_announced_blocks.erase(endpoint);
    }
    return result;
}

std::optional<std::vector<unsigned char>> PeerManager::GetChunkById(
    const std::string& address, const uint16_t port, const ProviderId& provider_id, const ChunkId& chunk_id)
{
    Endpoint endpoint;
    auto* session = FindStorageSession(address, port, provider_id, &endpoint);
    if (!session) return std::nullopt;
    auto result = session->GetChunkById(chunk_id);
    if (!session->Peer()) {
        m_peers.erase(endpoint);
        m_announced_blocks.erase(endpoint);
    }
    return result;
}

std::optional<ChunkAuthorizationProof> PeerManager::GetChunkAuthorizationProof(
    const std::string& address, const uint16_t port, const ProviderId& provider_id,
    const uint256& publication_operation_id, const ChunkId& chunk_id)
{
    Endpoint endpoint;
    auto* session = FindStorageSession(address, port, provider_id, &endpoint);
    if (!session) return std::nullopt;
    auto result = session->GetChunkAuthorizationProof(publication_operation_id, chunk_id);
    if (!session->Peer()) {
        m_peers.erase(endpoint);
        m_announced_blocks.erase(endpoint);
    }
    return result;
}

PeerSession* PeerManager::FindStorageSession(
    const std::string& address, const uint16_t port, const std::optional<ProviderId>& provider_id, Endpoint* endpoint)
{
    if (port == 0) return nullptr;
    boost::system::error_code ec;
    const auto parsed = boost::asio::ip::make_address(address, ec);
    if (ec) return nullptr;
    const Endpoint key{parsed.to_string(), port};
    const auto it = m_peers.find(key);
    if (it == m_peers.end() || !it->second->Peer() ||
        !(it->second->Peer()->capabilities & CAP_STORAGE) || !it->second->PeerProviderId()) return nullptr;
    // A different provider now answering at this endpoint is not the recorded replica.
    if (provider_id && *it->second->PeerProviderId() != *provider_id) return nullptr;
    if (endpoint) *endpoint = key;
    return it->second.get();
}

void PeerManager::DisconnectAll()
{
    m_peers.clear();
    // Deliberately keep m_announced_blocks: a peer's
    // finalized-chain knowledge persists across sessions. Clearing it restarts fanout
    // from the oldest recent entries, and with the per-peer offer cap a peer
    // that is behind never receives the newer blocks it actually needs.
}

size_t PeerManager::DiscoverPeers(const size_t max_sessions)
{
    if (max_sessions == 0 || m_peers.empty()) return 0;
    size_t added{0};
    std::vector<std::pair<std::string, uint16_t>> discovered;
    std::vector<Endpoint> targets;
    auto it = m_discovery_cursor ? m_peers.upper_bound(*m_discovery_cursor) : m_peers.begin();
    if (it == m_peers.end()) it = m_peers.begin();
    size_t scanned{0};
    while (scanned < m_peers.size() && targets.size() < max_sessions) {
        if (it == m_peers.end()) it = m_peers.begin();
        const auto endpoint = it->first;
        const auto peer = it->second ? it->second->Peer() : std::nullopt;
        ++it;
        ++scanned;
        m_discovery_cursor = endpoint;
        if (peer && (peer->capabilities & CAP_PEER_DISCOVERY)) targets.push_back(endpoint);
    }
    for (const auto& endpoint : targets) {
        const auto session = m_peers.find(endpoint);
        if (session == m_peers.end() || !session->second) continue;
        const auto peers = session->second->RequestPeers();
        discovered.insert(discovered.end(), peers.begin(), peers.end());
    }
    if (!discovered.empty()) {
        const auto before = m_runtime.GetPeerEndpointsForGossip().size();
        m_runtime.AddDiscoveredPeerEndpoints(discovered);
        const auto after = m_runtime.GetPeerEndpointsForGossip().size();
        if (after > before) added = after - before;
    }
    return added;
}

std::vector<std::pair<std::string, uint16_t>> PeerManager::KnownEndpoints() const
{
    return m_runtime.GetPeerEndpointsForGossip();
}

} // namespace cybou::p2p
