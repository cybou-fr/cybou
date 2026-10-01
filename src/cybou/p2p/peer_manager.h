// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_PEER_MANAGER_H
#define CYBOU_P2P_PEER_MANAGER_H

#include <cybou/p2p/session.h>
#include <cybou/sync_result.h>

#include <boost/asio/io_context.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cybou {
class CybouNodeRuntime;
}

namespace cybou::p2p {

inline constexpr size_t MAX_OUTBOUND_PEERS{8};

enum class PeerConnectStatus : uint8_t {
    CONNECTED,
    UNAVAILABLE,
    HANDSHAKE_FAILED,
    WRONG_NETWORK,
    ADMISSION_REJECTED,
    INVALID_REQUEST,
    LOCAL_FAILURE,
};

struct PeerInfo {
    std::string address;
    uint16_t port{0};
    Hello hello;
    /** Proven ProviderID for storage peers. */
    std::optional<ProviderId> provider_id;
    /** Session-only proof of the genesis PoA key; cleared when the session drops. */
    bool finalizer_authenticated{false};
    /** Session-only proof of a genesis-granted bootstrap AccountID. */
    std::optional<AccountId> bootstrap_account_id;
};

struct PeerSubmitResult {
    uint256 op_id;
    std::optional<OperationSubmitResult> acknowledgment;
    std::optional<std::pair<std::string, uint16_t>> endpoint;
    bool delivery_uncertain{false};

    explicit operator bool() const { return acknowledgment && static_cast<bool>(*acknowledgment); }
};

// Single-threaded outbound peer set. Callers schedule connection attempts and
// health checks; this class never supplies consensus trust. Dynamic discovery
// only collects untrusted routing hints — explicit operator-configured peer endpoints keep
// gossip priority, and discovered peers never define consensus connectivity.
class PeerManager {
public:
    explicit PeerManager(CybouNodeRuntime& runtime);
    bool Connect(const std::string& numeric_address, uint16_t port);
    PeerConnectStatus LastConnectStatus() const { return m_last_connect_status; }
    /**
     * Mark endpoints as explicit operator-configured peers. At capacity, connecting an
     * explicit endpoint evicts a connected non-explicit peer so operator-
     * configured peers can never be crowded out by discovered hints.
     */
    void SetExplicitEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints);
    size_t PingAll();
    /** Ping at most max_peers, rotating the starting peer on each call. */
    size_t PingSome(size_t max_peers);
    SyncPeerResult SyncFromPeer(const std::string& numeric_address, uint16_t port, uint64_t max_blocks);
    OperationSubmitResult SubmitOperation(const std::string& numeric_address, uint16_t port,
        const ProtocolOperation& operation);
    PeerSubmitResult SubmitOperationToAny(
        const std::vector<std::pair<std::string, uint16_t>>& endpoints,
        const ProtocolOperation& operation);
    size_t FanoutRecentBlocks(size_t max_per_peer = 16);
    size_t ConnectedCount() const { return m_peers.size(); }
    std::vector<PeerInfo> Peers() const;
    /** Live finalizer-authenticated sessions; endpoints are transient routes only. */
    std::vector<PeerInfo> AuthenticatedFinalizerSessions() const;
    /** Live genesis-granted bootstrap sessions; endpoint data is transient only. */
    std::vector<PeerInfo> AuthenticatedBootstrapSessions() const;
    /** Connected peers that advertised the optional ciphertext storage service. */
    std::vector<PeerInfo> StoragePeers() const;
    std::optional<ChunkAdmissionResult> PutAuthorizedChunk(
        const std::string& address, uint16_t port, const ProviderId& provider_id,
        const uint256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof);
    std::optional<std::vector<unsigned char>> GetChunkById(
        const std::string& address, uint16_t port, const ProviderId& provider_id, const ChunkId& chunk_id);
    std::optional<ChunkAuthorizationProof> GetChunkAuthorizationProof(
        const std::string& address, uint16_t port, const ProviderId& provider_id,
        const uint256& publication_operation_id, const ChunkId& chunk_id);
    void DisconnectAll();

    /** Dynamic peer auto-discovery */
    size_t DiscoverPeers(size_t max_sessions = MAX_OUTBOUND_PEERS);
    std::vector<std::pair<std::string, uint16_t>> KnownEndpoints() const;

private:
    using Endpoint = std::pair<std::string, uint16_t>;
    PeerSession* FindStorageSession(const std::string& address, uint16_t port,
        const std::optional<ProviderId>& provider_id, Endpoint* endpoint = nullptr);
    CybouNodeRuntime& m_runtime;
    boost::asio::io_context m_io;
    std::map<Endpoint, std::unique_ptr<PeerSession>> m_peers;
    std::optional<Endpoint> m_ping_cursor;
    std::optional<Endpoint> m_discovery_cursor;
    std::map<Endpoint, std::set<uint256>> m_announced_blocks;
    // Last finalized height each peer reported in a BLOCK_RESULT ack, so the
    // fanout can skip (and mark announced) heads the peer already finalized
    // without spending the per-cycle offer budget on ancient history.
    std::map<Endpoint, uint64_t> m_peer_finalized_heights;
    // Operator-configured peer endpoints (canonical address:port form).
    std::set<Endpoint> m_explicit_endpoints;
    PeerConnectStatus m_last_connect_status{PeerConnectStatus::INVALID_REQUEST};
};

} // namespace cybou::p2p
#endif
