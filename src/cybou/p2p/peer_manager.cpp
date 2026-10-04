// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Реализация single-threaded менеджера outbound-пиров CYBOU.

#include <cybou/operation_submit.h>
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

/// \brief Генерирует ненулевой nonce для `HELLO` и ping.
std::optional<uint64_t> RandomNonce()
{
    std::array<unsigned char, 8> bytes{};
    if (RAND_bytes(bytes.data(), bytes.size()) != 1) return std::nullopt;
    uint64_t nonce{0};
    for (int i = 0; i < 8; ++i) nonce |= uint64_t{bytes[i]} << (8 * i);
    return nonce == 0 ? std::nullopt : std::optional<uint64_t>{nonce};
}

using Endpoint = std::pair<std::string, uint16_t>;

/// \brief Канонизирует числовой endpoint в точную строковую форму Boost.Asio.
/// \details Это убирает дубликаты вида разных текстовых представлений одного IP.
std::optional<Endpoint> CanonicalEndpoint(const std::string_view numeric_address, const uint16_t port)
{
    if (port == 0) return std::nullopt;
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(std::string{numeric_address}, ec);
    if (ec) return std::nullopt;
    return Endpoint{address.to_string(), port};
}

} // namespace

PeerManager::PeerManager(CybouNodeRuntime& runtime) : m_runtime{runtime}
{
    SetExplicitEndpoints(m_runtime.GetConfiguredPeerEndpoints());
}

bool PeerManager::Connect(const std::string& numeric_address, const uint16_t port)
{
    m_last_connect_status = PeerConnectStatus::INVALID_REQUEST;
    const auto endpoint = CanonicalEndpoint(numeric_address, port);
    if (!endpoint) return false;
    if (!m_runtime.AdmitPeerAddress(endpoint->first)) {
        m_last_connect_status = PeerConnectStatus::ADMISSION_REJECTED;
        return false;
    }
    if (m_peers.contains(*endpoint)) return false;
    if (m_peers.size() >= MAX_OUTBOUND_PEERS) {
        if (!(m_explicit_endpoints.contains(*endpoint) || m_runtime.PinnedSpki(endpoint->first, endpoint->second).has_value())) {
            // Discovered endpoints never displace existing connections.
            m_last_connect_status = PeerConnectStatus::UNAVAILABLE;
            return false;
        }
        // Operator-configured or compiled rendezvous endpoint: evict an unprotected peer
        // to make room. A new session obtains its frontier from HELLO.
        const auto victim = std::find_if(m_peers.begin(), m_peers.end(), [&](const auto& entry) {
            return !m_explicit_endpoints.contains(entry.first) &&
                !m_runtime.PinnedSpki(entry.first.first, entry.first.second).has_value();
        });
        if (victim == m_peers.end()) {
            m_last_connect_status = PeerConnectStatus::UNAVAILABLE;
            return false;
        }
        m_peer_finalized_heights.erase(victim->first);
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
    const auto address = boost::asio::ip::make_address(endpoint->first);
    socket.async_connect({address, endpoint->second}, [&](const boost::system::error_code& result) {
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
    Hello local{.network_binding = status.network_binding, .finalized_height = status.finalized_height,
        .finalized_tip = status.finalized_tip, .nonce = *nonce};
    TlsSessionConfig tls;
    tls.expected_server_spki_sha256 = m_runtime.PinnedSpki(endpoint->first, endpoint->second);
    auto peer = std::make_unique<PeerSession>(std::move(socket), TransportRole::CLIENT, std::move(tls));
    if (!peer->Handshake(local)) {
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
    // Reconnect obtains a fresh frontier: an endpoint may now serve a different node.
    m_peer_finalized_heights[*endpoint] = peer->Peer()->finalized_height;
    m_peers.emplace(*endpoint, std::move(peer));
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
            m_peer_finalized_heights.erase(it->first);
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
            m_peer_finalized_heights.erase(endpoint);
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
    const auto endpoint = CanonicalEndpoint(numeric_address, port);
    if (!endpoint) return result;
    auto it = m_peers.find(*endpoint);
    if (it == m_peers.end()) return result;
    if (!it->second->Peer()) {
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
        const auto remaining = static_cast<uint8_t>(std::min<uint64_t>(max_blocks - result.blocks_applied, MAX_BLOCK_BATCH));
        const auto batch = it->second->RequestBlocks(height, remaining,
            [&](uint64_t expected_height, std::span<const unsigned char> bytes) {
                const auto block = DeserializeFinalizedBlock(bytes);
                const auto& announced = *it->second->Peer();
                // Даже после валидного batch wire-ответа commit остается локальным и
                // независимым: peer не может продвинуть канон одним только объявлением.
                if (!block || block->block.height != expected_height ||
                    block->certificate.network_binding != status.network_binding ||
                    block->certificate.block_id != ComputeBlockId(block->block) ||
                    (expected_height == announced.finalized_height && block->certificate.block_id != announced.finalized_tip) ||
                    !m_runtime.CommitBlock(*block)) return false;
                ++result.blocks_applied;
                result.status = SyncPeerStatus::BLOCKS_APPLIED;
                return true;
            });
        if (batch.status == BlockRequestStatus::NOT_FOUND) {
            if (height <= it->second->Peer()->finalized_height) {
                result.status = SyncPeerStatus::CONNECTION_FAILED;
                m_peers.erase(it);
            } else result.caught_up_with_known_peers = true;
            break;
        }
        if (batch.status != BlockRequestStatus::OK) {
            result.status = batch.status == BlockRequestStatus::UNAVAILABLE ?
                SyncPeerStatus::CONNECTION_FAILED : SyncPeerStatus::PROTOCOL_ERROR;
            m_peers.erase(it);
            break;
        }
    }
    return result;
}


OperationSubmitResult PeerManager::SubmitOperation(const std::string& numeric_address, uint16_t port,
    const ProtocolOperation& operation, const uint64_t work_nonce)
{
    const auto op_id = ComputeOperationId(operation).value_or(cybou::Hash256{});
    const OperationSubmitResult failure{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
    const auto endpoint = CanonicalEndpoint(numeric_address, port);
    if (!endpoint) return failure;
    auto it = m_peers.find(*endpoint);
    if (it == m_peers.end()) return failure;
    const auto result = it->second->SubmitOperation(operation, work_nonce);
    if (!result) {
        m_peers.erase(it);
        return failure;
    }
    return *result;
}

PeerSubmitResult PeerManager::SubmitOperationToAny(
    const std::vector<std::pair<std::string, uint16_t>>& endpoints,
    const ProtocolOperation& operation, const uint64_t work_nonce)
{
    const auto op_id = ComputeOperationId(operation).value_or(cybou::Hash256{});
    PeerSubmitResult result{.op_id = op_id, .acknowledgment = std::nullopt,
        .endpoint = std::nullopt, .delivery_uncertain = false};
    if (endpoints.empty() || endpoints.size() > MAX_OUTBOUND_PEERS) return result;
    const auto bytes = SerializeProtocolOperation(operation);
    if (!bytes || bytes->empty() || bytes->size() > MAX_OPERATION_PAYLOAD_BYTES || op_id.IsNull()) return result;
    for (const auto& [host, port] : endpoints) {
        const auto endpoint = CanonicalEndpoint(host, port);
        if (!endpoint) continue;
        auto it = m_peers.find(*endpoint);
        if (it == m_peers.end()) {
            if (!Connect(host, port)) continue;
            it = m_peers.find(*endpoint);
        }
        if (it == m_peers.end() || !it->second->Peer()) continue;
        const auto acknowledgment = it->second->SubmitOperation(operation, work_nonce);
        if (!acknowledgment) {
            result.delivery_uncertain = true;
            m_peers.erase(it);
            continue;
        }
        result.acknowledgment = *acknowledgment;
        result.endpoint = *endpoint;
        if (*acknowledgment) {
            result.delivery_uncertain = false;
            return result;
        }
    }
    return result;
}

size_t PeerManager::FanoutFinalizedBlocks(size_t max_per_peer)
{
    if (max_per_peer == 0 || max_per_peer > 32) return 0;
    // Session heights are scheduling hints, never evidence of canonical truth.
    std::erase_if(m_peer_finalized_heights, [&](const auto& entry) {
        return !m_peers.contains(entry.first);
    });
    const auto tip = m_runtime.GetFinalizedHeight();
    if (!tip) return 0;
    size_t delivered{0};
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        if (!it->second->Peer()) {
            ++it;
            continue;
        }
        auto& frontier = m_peer_finalized_heights[it->first];
        bool disconnected{false};
        for (size_t offered{0}; offered < max_per_peer && frontier < *tip; ++offered) {
            // frontier < tip also prevents overflow at UINT64_MAX.
            const uint64_t height = frontier + 1;
            const auto block = m_runtime.GetBlockAtHeight(height);
            if (!block) break;
            uint64_t peer_height{0};
            const auto response = it->second->AdvertiseBlock(
                {height, ComputeBlockId(block->block)}, *block, peer_height);
            if (!response) {
                disconnected = true;
                break;
            }
            frontier = peer_height;
            if (*response == BlockAnnounceResult::GAP) break;
            // A success without advancement must not consume repeated payloads.
            if (frontier < height) {
                disconnected = true;
                break;
            }
            ++delivered;
        }
        if (disconnected) {
            m_peer_finalized_heights.erase(it->first);
            it = m_peers.erase(it);
        } else {
            ++it;
        }
    }
    return delivered;
}

size_t PeerManager::PollOperationRelays()
{
    size_t delivered{0};
    constexpr size_t MAX_RELAY_OPERATIONS_PER_PEER{4};
    for (auto& [endpoint, session] : m_peers) {
        (void)endpoint;
        if (!session->Peer()) continue;
        for (size_t i = 0; i < MAX_RELAY_OPERATIONS_PER_PEER; ++i) {
            if (!session->PollOperationRelay(m_runtime)) break;
            ++delivered;
        }
    }
    return delivered;
}

size_t PeerManager::PollValidationAttestations()
{
    size_t received{0};
    constexpr size_t MAX_ATTESTATIONS_PER_PEER{8};
    for (auto& [endpoint, session] : m_peers) {
        (void)endpoint;
        if (!session->Peer()) continue;
        for (size_t i = 0; i < MAX_ATTESTATIONS_PER_PEER; ++i) {
            if (!session->PollValidationAttestation(m_runtime)) break;
            ++received;
        }
    }
    return received;
}

std::vector<PeerInfo> PeerManager::Peers() const
{
    std::vector<PeerInfo> peers;
    peers.reserve(m_peers.size());
    for (const auto& [endpoint, session] : m_peers) {
        if (session->Peer()) {
            peers.push_back(PeerInfo{endpoint.first, endpoint.second, *session->Peer(), session->PeerStorageId()});
        }
    }
    return peers;
}

std::vector<PeerInfo> PeerManager::StorageEndpoints()
{
    std::vector<PeerInfo> peers;
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        auto& session = it->second;
        if (!session->Peer()) {
            m_peer_finalized_heights.erase(it->first);
            it = m_peers.erase(it);
            continue;
        }
        if (session->ProveStorageIdentity()) {
            peers.push_back(PeerInfo{it->first.first, it->first.second, *session->Peer(), session->PeerStorageId()});
        }
        ++it;
    }
    return peers;
}

std::optional<ChunkAdmissionResult> PeerManager::PutAuthorizedChunk(
    const std::string& address, const uint16_t port, const StorageId& storage_id,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id, const std::span<const unsigned char> stored_bytes,
    const ChunkAuthorizationProof& proof)
{
    Endpoint endpoint;
    auto* session = FindStorageSession(address, port, storage_id, &endpoint);
    if (!session) return std::nullopt;
    auto result = session->PutAuthorizedChunk(publication_operation_id, chunk_id, stored_bytes, proof);
    if (!result) {
        m_peers.erase(endpoint);
        m_peer_finalized_heights.erase(endpoint);
    }
    return result;
}

std::optional<std::vector<unsigned char>> PeerManager::GetChunkById(
    const std::string& address, const uint16_t port, const StorageId& storage_id, const ChunkId& chunk_id)
{
    Endpoint endpoint;
    auto* session = FindStorageSession(address, port, storage_id, &endpoint);
    if (!session) return std::nullopt;
    auto result = session->GetChunkById(chunk_id);
    if (!session->Peer()) {
        m_peers.erase(endpoint);
        m_peer_finalized_heights.erase(endpoint);
    }
    return result;
}

std::optional<ChunkAuthorizationProof> PeerManager::GetChunkAuthorizationProof(
    const std::string& address, const uint16_t port, const StorageId& storage_id,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id)
{
    Endpoint endpoint;
    auto* session = FindStorageSession(address, port, storage_id, &endpoint);
    if (!session) return std::nullopt;
    auto result = session->GetChunkAuthorizationProof(publication_operation_id, chunk_id);
    if (!session->Peer()) {
        m_peers.erase(endpoint);
        m_peer_finalized_heights.erase(endpoint);
    }
    return result;
}

PeerSession* PeerManager::FindStorageSession(
    const std::string& address, const uint16_t port, const std::optional<StorageId>& storage_id, Endpoint* endpoint)
{
    const auto key = CanonicalEndpoint(address, port);
    if (!key) return nullptr;
    const auto it = m_peers.find(*key);
    if (it == m_peers.end()) return nullptr;
    if (!it->second->Peer()) {
        m_peer_finalized_heights.erase(*key);
        m_peers.erase(it);
        return nullptr;
    }
    if (!it->second->ProveStorageIdentity()) {
        return nullptr;
    }
    // Иной provider на том же endpoint недопустим: реплика идентифицируется
    // доказанным `StorageId`, а не только адресом и портом.
    if (storage_id && *it->second->PeerStorageId() != *storage_id) return nullptr;
    if (endpoint) *endpoint = *key;
    return it->second.get();
}

void PeerManager::DisconnectAll()
{
    m_peers.clear();
    m_peer_finalized_heights.clear();
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
        if (peer) targets.push_back(endpoint);
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
