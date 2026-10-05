// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// \brief Full Node sessions, discovery, routing and remote storage transport.

#include <cybou/node_runtime.h>
#include <cybou/p2p/peer_manager.h>
#include <cybou/p2p/storage_session_pool.h>
#include <cybou/storage_io_scheduler.h>
#include <cybou/p2p/peer_admission.h>
#include <cybou/identity_crypto.h>
#include <boost/asio/ip/address.hpp>
#include <algorithm>

namespace cybou {

namespace {
// Discovered routing hints are untrusted. Reject address scopes this node has
// no business dialing: unspecified, multicast, and (unless the local CYBOU P2P
// listener lives in the same scope) loopback and link-local targets. Without a
// known local listener the policy stays permissive for DEV tooling.
/// \brief Определяет private/ULA адреса, которые нельзя безусловно dial'ить по чужим routing hints.
bool IsPrivateAddress(const boost::asio::ip::address& addr)
{
    const auto is_private_v4 = [](const uint32_t value) {
        return (value & 0xFF000000U) == 0x0A000000U ||
            (value & 0xFFF00000U) == 0xAC100000U ||
            (value & 0xFFFF0000U) == 0xC0A80000U;
    };
    if (addr.is_v4()) {
        return is_private_v4(addr.to_v4().to_uint());
    }
    const auto bytes = addr.to_v6().to_bytes();
    if (addr.to_v6().is_v4_mapped()) {
        const uint32_t value = (static_cast<uint32_t>(bytes[12]) << 24) |
            (static_cast<uint32_t>(bytes[13]) << 16) |
            (static_cast<uint32_t>(bytes[14]) << 8) | static_cast<uint32_t>(bytes[15]);
        return is_private_v4(value);
    }
    return (bytes[0] & 0xFEU) == 0xFCU; // Unique-local IPv6 (fc00::/7).
}

/// \brief Отфильтровывает discovered endpoints, которые выглядели бы loopback/link-local/private из чужой зоны.
bool IsConnectableDiscoveredAddress(
    const boost::asio::ip::address& addr,
    const std::optional<std::pair<std::string, uint16_t>>& advertised_endpoint)
{
    auto is_link_local = [](const boost::asio::ip::address& a) {
        if (a.is_v4()) return (a.to_v4().to_uint() & 0xFFFF0000U) == 0xA9FE0000U;
        const auto bytes = a.to_v6().to_bytes();
        return bytes[0] == 0xFEU && (bytes[1] & 0xC0U) == 0x80U;
    };
    if (addr.is_unspecified() || addr.is_multicast()) return false;
    if (IsPrivateAddress(addr) && advertised_endpoint) {
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(advertised_endpoint->first, ec);
        if (!ec && !IsPrivateAddress(local) && !local.is_loopback() && !is_link_local(local) &&
            !local.is_unspecified() && !local.is_multicast()) {
            return false;
        }
    }
    if (addr.is_loopback()) {
        if (!advertised_endpoint) return true;
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(advertised_endpoint->first, ec);
        return !ec && local.is_loopback();
    }
    if (is_link_local(addr)) {
        if (!advertised_endpoint) return true;
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(advertised_endpoint->first, ec);
        return !ec && is_link_local(local);
    }
    return true;
}

} // namespace

CybouNodeRuntime::NetworkCore::NetworkCore(const NodeRuntimeConfig& config)
    : configured_peers{config.configured_peers}, advertised_endpoint{config.advertised_endpoint}
{
}

CybouNodeRuntime::NetworkCore::~NetworkCore()
{
    // Drain jobs while routing, peer policy and runtime domains still exist.
    storage_io.reset();
    storage_sessions.reset();
    peer_manager.reset();
}

StorageIoScheduler& CybouNodeRuntime::StorageIo() { return *m_network.storage_io; }

bool CybouNodeRuntime::AdmitPeerAddress(const std::string& numeric_address) const
{
    // Allows() itself fails closed for public addresses without Geo data (DEC-285).
    return m_config.peer_admission_policy && m_config.peer_admission_policy->Allows(numeric_address);
}

std::vector<CybouNodeRuntime::StorageEndpoint> CybouNodeRuntime::StorageEndpoints() const
{
    std::vector<p2p::PeerInfo> peers;
    {
        std::lock_guard p2p_lock(m_network.mutex);
        if (!m_network.peer_manager) return {};
        peers = m_network.peer_manager->StorageEndpoints();
    }
    std::shared_ptr<const CybouState> state;
    {
        std::lock_guard lock(m_chain.mutex);
        const auto loaded = m_chain.store.GetStateSnapshot();
        if (loaded && loaded.state) state = loaded.state;
    }
    std::vector<StorageEndpoint> endpoints;
    endpoints.reserve(peers.size());
    for (const auto& peer : peers) {
        if (!peer.storage_id) continue;
        StorageEndpoint endpoint{peer.address, peer.port, *peer.storage_id};
        // Payout-аккаунт признаётся только по текущему finalized Authorization-ключу аккаунта.
        if (peer.payout_binding && state) {
            const auto* record = state->identities.Find(peer.payout_binding->payout_account);
            const auto digest = StoragePayoutBindingDigest(m_network_binding, *peer.storage_id,
                peer.payout_binding->payout_account);
            if (record && VerifyIdentityMessage(record->authorization_key, peer.payout_binding->authorization, digest)) {
                endpoint.payout_account = peer.payout_binding->payout_account;
            }
        }
        endpoints.push_back(std::move(endpoint));
    }
    return endpoints;
}

std::optional<ChunkAdmissionResult> CybouNodeRuntime::PutChunkToStorageEndpoint(const std::string& address,
    const uint16_t port, const std::array<unsigned char, 32>& storage_id, const cybou::Hash256& publication_operation_id,
    const ChunkId& chunk_id, const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    return m_network.storage_sessions->Run(address, port, storage_id, [&](p2p::PeerSession& session) {
        return session.PutAuthorizedChunk(publication_operation_id, chunk_id, stored_bytes, proof);
    });
}

std::optional<std::vector<unsigned char>> CybouNodeRuntime::GetChunkFromStorageEndpoint(const std::string& address,
    const uint16_t port, const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id)
{
    return m_network.storage_sessions->Run(address, port, storage_id, [&](p2p::PeerSession& session) {
        return session.GetChunkById(chunk_id);
    }, true);
}

std::optional<StorageAuditAnswer> CybouNodeRuntime::AuditChunkAtStorageEndpoint(const std::string& address,
    const uint16_t port, const std::array<unsigned char, 32>& storage_id, const StorageAuditChallenge& challenge)
{
    return m_network.storage_sessions->Run(address, port, storage_id, [&](p2p::PeerSession& session) {
        return session.AuditChunk(challenge);
    });
}

std::optional<ChunkAuthorizationProof> CybouNodeRuntime::GetChunkAuthorizationProofFromStorageEndpoint(
    const std::string& address, const uint16_t port, const std::array<unsigned char, 32>& storage_id,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id)
{
    return m_network.storage_sessions->Run(address, port, storage_id, [&](p2p::PeerSession& session) {
        return session.GetChunkAuthorizationProof(publication_operation_id, chunk_id);
    });
}

void CybouNodeRuntime::SchedulePeerRetry(
    const std::pair<std::string, uint16_t>& endpoint, const PeerFailureClass failure)
{
    auto& retry = m_network.peer_retry_after[endpoint];
    const auto now = std::chrono::steady_clock::now();
    const auto backoff = [](uint32_t failures, const uint32_t base_seconds, const uint32_t max_seconds) {
        uint32_t delay = base_seconds;
        while (failures > 1 && delay < max_seconds) {
            delay = std::min(max_seconds, delay * 2);
            --failures;
        }
        return std::chrono::seconds{delay};
    };

    switch (failure) {
    case PeerFailureClass::TEMPORARY:
        retry.temporary_failures = std::min<uint32_t>(retry.temporary_failures + 1, 16);
        retry.retry_after = now + backoff(retry.temporary_failures, 5, 300);
        break;
    case PeerFailureClass::PROTOCOL:
        // Protocol mismatches back off far more aggressively than transient
        // reachability to avoid hammering obviously incompatible peers.
        retry.protocol_failures = std::min<uint32_t>(retry.protocol_failures + 1, 16);
        retry.retry_after = now + backoff(retry.protocol_failures, 1800, 86400);
        break;
    case PeerFailureClass::WRONG_NETWORK:
        retry.retry_after = now + std::chrono::hours{24};
        break;
    }
}

SyncPeerResult CybouNodeRuntime::SyncFromConfiguredPeer(const uint64_t max_blocks)
{
    (void)m_provider.finalized_chunk_store->RetryPendingPurges();
    if (!m_network.peer_manager) return {};
    RetryPendingIdentityOperations();
    std::lock_guard p2p_lock(m_network.mutex);

    const auto local_status = GetStatus();
    if (local_status.runtime_state == NodeRuntimeState::NETWORK_MISMATCH) {
        return SyncPeerResult{.status = SyncPeerStatus::NETWORK_MISMATCH};
    }
    if (local_status.runtime_state == NodeRuntimeState::CORRUPT || !local_status.is_initialized) {
        return SyncPeerResult{.status = SyncPeerStatus::PROTOCOL_ERROR};
    }

    auto explicit_endpoints = GetConfiguredPeerEndpoints();
    m_network.peer_manager->SetExplicitEndpoints(explicit_endpoints);
    const auto maintenance_now = std::chrono::steady_clock::now();
    const bool have_connected_peers = m_network.peer_manager->ConnectedCount() != 0;
    if (have_connected_peers && maintenance_now >= m_network.next_peer_discovery) {
        m_network.next_peer_discovery = maintenance_now + std::chrono::seconds{60};
        m_network.peer_manager->DiscoverPeers(1);
    } else if (have_connected_peers && maintenance_now >= m_network.next_peer_ping) {
        m_network.next_peer_ping = maintenance_now + std::chrono::seconds{15};
        m_network.peer_manager->PingSome(1);
    }

    const auto targets = GetPeerEndpointsForGossip();
    const auto connected_before_dial = m_network.peer_manager->Peers();
    if (connected_before_dial.size() < p2p::MAX_OUTBOUND_PEERS) {
        const auto now = std::chrono::steady_clock::now();
        const auto candidate = std::find_if(targets.begin(), targets.end(), [&](const auto& endpoint) {
            const bool connected = std::any_of(connected_before_dial.begin(), connected_before_dial.end(),
                [&](const p2p::PeerInfo& peer) { return peer.address == endpoint.first && peer.port == endpoint.second; });
            const auto retry = m_network.peer_retry_after.find(endpoint);
            return !connected && (retry == m_network.peer_retry_after.end() || now >= retry->second.retry_after);
        });
        if (candidate != targets.end()) {
            if (m_network.peer_manager->Connect(candidate->first, candidate->second)) {
                m_network.peer_retry_after.erase(*candidate);
            } else {
                switch (m_network.peer_manager->LastConnectStatus()) {
                case p2p::PeerConnectStatus::UNAVAILABLE:
                    SchedulePeerRetry(*candidate, PeerFailureClass::TEMPORARY);
                    break;
                case p2p::PeerConnectStatus::WRONG_NETWORK:
                    SchedulePeerRetry(*candidate, PeerFailureClass::WRONG_NETWORK);
                    break;
                case p2p::PeerConnectStatus::HANDSHAKE_FAILED:
                    SchedulePeerRetry(*candidate, PeerFailureClass::PROTOCOL);
                    break;
                default:
                    break;
                }
            }
        }
    }

    auto peers = m_network.peer_manager->Peers();
    if (peers.empty()) {
        return SyncPeerResult{.status = SyncPeerStatus::CONNECTION_FAILED};
    }
    const auto route_rank = [&](const p2p::PeerInfo& peer) {
        const auto endpoint = std::make_pair(peer.address, peer.port);
        const auto found = std::find(explicit_endpoints.begin(), explicit_endpoints.end(), endpoint);
        return found == explicit_endpoints.end() ? explicit_endpoints.size() :
            static_cast<size_t>(found - explicit_endpoints.begin());
    };
    // HELLO heights are connection-time snapshots. Prefer configured routes so
    // mutually lagging discovered providers cannot eclipse the bootstrap peer.
    // An unavailable or up-to-date configured peer still falls through below.
    std::sort(peers.begin(), peers.end(), [&](const p2p::PeerInfo& left, const p2p::PeerInfo& right) {
        if (route_rank(left) != route_rank(right)) return route_rank(left) < route_rank(right);
        if (left.hello.finalized_height != right.hello.finalized_height) {
            return left.hello.finalized_height > right.hello.finalized_height;
        }
        if (left.address != right.address) return left.address < right.address;
        return left.port < right.port;
    });

    SyncPeerResult result{.status = SyncPeerStatus::CONNECTION_FAILED};
    bool any_peer_up_to_date{false};
    bool all_peers_caught_up{!peers.empty()};
    for (const auto& peer : peers) {
        const auto attempt = m_network.peer_manager->SyncFromPeer(peer.address, peer.port, max_blocks-result.blocks_applied);
        // Completion covers known reachable peers only; it conveys no consensus trust.
        if (!attempt.caught_up_with_known_peers) all_peers_caught_up = false;
        if (attempt.status == SyncPeerStatus::BLOCKS_APPLIED) {
            result.status=SyncPeerStatus::BLOCKS_APPLIED;
            result.blocks_applied+=attempt.blocks_applied;
            if (result.blocks_applied>=max_blocks) { all_peers_caught_up = false; break; }
            // Partial progress from a slow route must not hide a fresher peer.
            continue;
        }
        // HELLO height is only a snapshot from connection time. A peer that
        // reports UP_TO_DATE may have stopped advancing while another
        // connected peer has newer blocks, so keep checking the whole set.
        // Peer protocol failures are isolated to that session by PeerManager;
        // they must not become a fatal network-service result.
        if (attempt.status == SyncPeerStatus::UP_TO_DATE) {
            any_peer_up_to_date = true;
            m_network.peer_retry_after.erase({peer.address, peer.port});
        } else if (attempt.status == SyncPeerStatus::CONNECTION_FAILED) {
            SchedulePeerRetry({peer.address, peer.port}, PeerFailureClass::TEMPORARY);
        } else if (attempt.status == SyncPeerStatus::NETWORK_MISMATCH) {
            SchedulePeerRetry({peer.address, peer.port}, PeerFailureClass::WRONG_NETWORK);
        } else if (attempt.status == SyncPeerStatus::PROTOCOL_ERROR) {
            SchedulePeerRetry({peer.address, peer.port}, PeerFailureClass::PROTOCOL);
        }
    }
    result.caught_up_with_known_peers = all_peers_caught_up;
    if (result.blocks_applied==0 && any_peer_up_to_date) result.status = SyncPeerStatus::UP_TO_DATE;
    m_network.peer_manager->PollOperationRelays();
    m_network.peer_manager->PushOperationRelays();
    m_network.peer_manager->FanoutFinalizedBlocks();
    return result;
}

size_t CybouNodeRuntime::ConnectedPeerCount() const
{
    std::lock_guard p2p_lock(m_network.mutex);
    return m_network.peer_manager ? m_network.peer_manager->ConnectedCount() : 0;
}

std::optional<std::array<unsigned char, 32>> CybouNodeRuntime::PinnedSpki(
    const std::string& address, const uint16_t port) const
{
    std::lock_guard lock(m_network.routing_mutex);
    for (const auto& peer : m_network.configured_peers) {
        if (peer.endpoint.first == address && peer.endpoint.second == port) return peer.tls_spki_sha256;
    }
    return std::nullopt;
}

std::vector<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetPeerEndpointsForGossip() const
{
    std::lock_guard lock(m_network.routing_mutex);
    constexpr size_t MAX_GOSSIP_TARGETS{32};
    std::vector<std::pair<std::string, uint16_t>> result;
    result.reserve(MAX_GOSSIP_TARGETS);
    for (const auto& peer : m_network.configured_peers) {
        if (result.size() >= MAX_GOSSIP_TARGETS) break;
        if (m_network.advertised_endpoint == peer.endpoint) continue;
        if (std::find(result.begin(), result.end(), peer.endpoint) == result.end()) result.push_back(peer.endpoint);
    }
    for (const auto& ep : m_network.discovered_peer_endpoints) {
        if (result.size() >= MAX_GOSSIP_TARGETS) break;
        if (std::find(result.begin(), result.end(), ep) == result.end()) result.push_back(ep);
    }
    return result;
}

void CybouNodeRuntime::SetConfiguredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints)
{
    std::lock_guard lock(m_network.routing_mutex);
    // Retain release-pinned configured entries when replacing operator routes.
    std::erase_if(m_network.configured_peers, [](const ConfiguredPeer& peer) { return !peer.tls_spki_sha256; });
    for (const auto& [host, port] : endpoints) {
        if (port == 0 || m_network.configured_peers.size() >= 32) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        const Endpoint endpoint{addr.to_string(), port};
        if (m_network.advertised_endpoint == endpoint ||
            std::any_of(m_network.configured_peers.begin(), m_network.configured_peers.end(),
                [&](const ConfiguredPeer& peer) { return peer.endpoint == endpoint; })) continue;
        m_network.configured_peers.push_back({endpoint, std::nullopt});
    }
}

std::vector<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetConfiguredPeerEndpoints() const
{
    std::lock_guard lock(m_network.routing_mutex);
    std::vector<Endpoint> result;
    result.reserve(m_network.configured_peers.size());
    for (const auto& peer : m_network.configured_peers)
        if (m_network.advertised_endpoint != peer.endpoint) result.push_back(peer.endpoint);
    return result;
}

void CybouNodeRuntime::AddDiscoveredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints)
{
    std::lock_guard lock(m_network.routing_mutex);
    constexpr size_t MAX_DISCOVERED_PEER_ENDPOINTS{256};
    for (const auto& [host, port] : endpoints) {
        if (port == 0) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        if (!IsConnectableDiscoveredAddress(addr, m_network.advertised_endpoint)) continue;
        const auto canonical = std::make_pair(addr.to_string(), port);
        if (m_network.advertised_endpoint && canonical == *m_network.advertised_endpoint) continue;
        if (std::any_of(m_network.configured_peers.begin(), m_network.configured_peers.end(),
                [&](const ConfiguredPeer& peer) { return peer.endpoint == canonical; })) continue;
        if (m_network.discovered_peer_endpoints.size() >= MAX_DISCOVERED_PEER_ENDPOINTS) break;
        m_network.discovered_peer_endpoints.emplace(canonical);
    }
}

} // namespace cybou
