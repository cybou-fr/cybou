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
    uint64_t caps = CAP_SERVE_BLOCKS | CAP_BLOCK_INVENTORY | CAP_BLOCK_ANNOUNCEMENTS | CAP_PEER_DISCOVERY;
    if (status.is_authority) {
        caps |= (CAP_ACCEPT_OPERATIONS | CAP_OP_INVENTORY | CAP_CONSENSUS);
    }
    Hello local{.network_id = status.network_id, .finalized_height = status.finalized_height,
        .finalized_tip = status.finalized_tip, .capabilities = caps, .nonce = *nonce};
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
    m_announced_operations.erase(endpoint);
    m_announced_blocks.erase(endpoint);
    // Seed the fanout frontier from the peer's handshake height. This
    // knowledge survives reconnects (a peer's chain only grows), so it is
    // deliberately NOT erased on disconnect or failed consensus sends.
    m_peer_finalized_heights[endpoint] = peer->Peer()->finalized_height;
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
            m_announced_operations.erase(it->first);
            m_announced_blocks.erase(it->first);
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
    std::vector<BlockAnnouncement> inventory;
    size_t inventory_cursor{0};
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
                    }
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
            }
        }
        const auto response = it->second->RequestBlock(height);
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
            }
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

size_t PeerManager::FanoutRecentOperations(size_t max_per_peer)
{
    if (max_per_peer == 0 || max_per_peer > MAX_PENDING_OPERATIONS) return 0;
    for (auto it = m_announced_operations.begin(); it != m_announced_operations.end();) {
        if (!m_peers.contains(it->first)) it = m_announced_operations.erase(it);
        else ++it;
    }
    const auto pending = m_runtime.RecentOperationsForGossip();
    std::set<uint256> live_ids;
    for (const auto& operation : pending) {
        const auto id = ComputeOperationId(operation);
        if (id) live_ids.insert(*id);
    }
    size_t delivered{0};
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        auto& announced = m_announced_operations[it->first];
        for (auto known = announced.begin(); known != announced.end();) {
            if (!live_ids.contains(*known)) known = announced.erase(known);
            else ++known;
        }
        if (!it->second->Peer() ||
            !(it->second->Peer()->capabilities & CAP_ACCEPT_OPERATIONS) ||
            !(it->second->Peer()->capabilities & CAP_OP_INVENTORY)) {
            ++it;
            continue;
        }
        bool disconnected{false};
        size_t offered{0};
        for (const auto& operation : pending) {
            if (offered >= max_per_peer) break;
            const auto id = ComputeOperationId(operation);
            if (!id || announced.contains(*id)) continue;
            ++offered;
            const auto response = it->second->AdvertiseOperation(operation);
            if (!response) {
                disconnected = true;
                break;
            }
            if (static_cast<bool>(*response)) {
                announced.insert(*id);
                ++delivered;
            }
        }
        if (disconnected) {
            m_announced_operations.erase(it->first);
            it = m_peers.erase(it);
        } else {
            ++it;
        }
    }
    return delivered;
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
            m_announced_operations.erase(it->first);
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
        if (session->Peer()) peers.push_back(PeerInfo{endpoint.first, endpoint.second, *session->Peer()});
    }
    return peers;
}

void PeerManager::DisconnectAll()
{
    m_peers.clear();
    // Deliberately keep m_announced_blocks/m_announced_operations: a peer's
    // store knowledge persists across sessions. Clearing them restarts fanout
    // from the oldest recent entries, and with the per-peer offer cap a peer
    // that is behind never receives the newer blocks it actually needs.
}

bool PeerManager::SendConsensusTo(const std::string& address, uint16_t port,
    const std::optional<BftProposalMsg>& proposal,
    const std::optional<BftPrevoteMsg>& prevote,
    const std::optional<BftPrecommitMsg>& precommit)
{
    const Endpoint endpoint{address, port};
    const auto it = m_peers.find(endpoint);
    if (it == m_peers.end() || !it->second || !it->second->Peer() ||
        !(it->second->Peer()->capabilities & CAP_CONSENSUS)) return false;
    auto& session = *it->second;
    if ((proposal && !session.SendProposal(*proposal)) ||
        (prevote && !session.SendPrevote(*prevote)) ||
        (precommit && !session.SendPrecommit(*precommit))) {
        m_peers.erase(it);
        m_announced_operations.erase(endpoint);
        m_announced_blocks.erase(endpoint);
        return false;
    }
    return true;
}

size_t PeerManager::BroadcastProposal(const BftProposalMsg& proposal)
{
    size_t count{0};
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        auto& [endpoint, session] = *it;
        if (session && session->Peer() && (session->Peer()->capabilities & CAP_CONSENSUS)) {
            if (!session->SendProposal(proposal)) {
                m_announced_operations.erase(endpoint);
                m_announced_blocks.erase(endpoint);
                it = m_peers.erase(it);
                continue;
            }
            ++count;
        }
        ++it;
    }
    return count;
}

size_t PeerManager::BroadcastPrevote(const BftPrevoteMsg& prevote)
{
    size_t count{0};
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        auto& [endpoint, session] = *it;
        if (session && session->Peer() && (session->Peer()->capabilities & CAP_CONSENSUS)) {
            if (!session->SendPrevote(prevote)) {
                m_announced_operations.erase(endpoint);
                m_announced_blocks.erase(endpoint);
                it = m_peers.erase(it);
                continue;
            }
            ++count;
        }
        ++it;
    }
    return count;
}

size_t PeerManager::BroadcastPrecommit(const BftPrecommitMsg& precommit)
{
    size_t count{0};
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        auto& [endpoint, session] = *it;
        if (session && session->Peer() && (session->Peer()->capabilities & CAP_CONSENSUS)) {
            if (!session->SendPrecommit(precommit)) {
                m_announced_operations.erase(endpoint);
                m_announced_blocks.erase(endpoint);
                it = m_peers.erase(it);
                continue;
            }
            ++count;
        }
        ++it;
    }
    return count;
}

size_t PeerManager::DiscoverPeers()
{
    size_t added{0};
    std::vector<std::pair<std::string, uint16_t>> discovered;
    for (const auto& [endpoint, session] : m_peers) {
        if (session && session->Peer() && (session->Peer()->capabilities & CAP_PEER_DISCOVERY)) {
            const auto peers = session->RequestPeers();
            discovered.insert(discovered.end(), peers.begin(), peers.end());
        }
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
