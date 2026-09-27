// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_PEER_MANAGER_H
#define CYBOU_P2P_PEER_MANAGER_H

#include <cybou/p2p/session.h>
#include <cybou/block_feed.h>

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
    INVALID_REQUEST,
    LOCAL_FAILURE,
};

struct PeerInfo {
    std::string address;
    uint16_t port{0};
    Hello hello;
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
// only collects untrusted routing hints — explicit validator endpoints keep
// gossip priority, and discovered peers never define consensus connectivity.
class PeerManager {
public:
    explicit PeerManager(CybouNodeRuntime& runtime);
    bool Connect(const std::string& numeric_address, uint16_t port);
    PeerConnectStatus LastConnectStatus() const { return m_last_connect_status; }
    /**
     * Mark endpoints as explicit validator peers. At capacity, connecting an
     * explicit endpoint evicts a connected non-explicit peer so operator-
     * approved validators can never be crowded out by discovered hints.
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
    /** Offer each recent admitted OperationID once per connected peer. */
    size_t FanoutRecentOperations(size_t max_per_peer = 16);
    size_t FanoutRecentBlocks(size_t max_per_peer = 16);
    size_t ConnectedCount() const { return m_peers.size(); }
    std::vector<PeerInfo> Peers() const;
    /** Connected peers that advertised the optional ciphertext storage service. */
    std::vector<PeerInfo> StoragePeers() const;
    /** Storage calls use an existing session and must run on this manager's owner thread. */
    std::optional<StorageWriteResult> PutStorageChunk(
        const std::string& address, uint16_t port, const StorageObjectId& object_id,
        const StorageEncryptedChunk& chunk);
    std::optional<StorageWriteResult> CommitStorageManifest(
        const std::string& address, uint16_t port, const StoragePublicManifest& manifest);
    std::optional<StoragePublicManifest> GetStorageManifest(
        const std::string& address, uint16_t port, const StorageObjectId& object_id);
    std::optional<StorageEncryptedChunk> GetStorageChunk(
        const std::string& address, uint16_t port, const StorageObjectId& object_id, uint32_t index);
    void DisconnectAll();

    /** Dynamic peer auto-discovery */
    size_t DiscoverPeers(size_t max_sessions = MAX_OUTBOUND_PEERS);
    std::vector<std::pair<std::string, uint16_t>> KnownEndpoints() const;

    /** BFT consensus broadcasts to connected peers with CAP_CONSENSUS */
    size_t BroadcastProposal(const BftProposalMsg& proposal);
    size_t BroadcastPrevote(const BftPrevoteMsg& prevote);
    size_t BroadcastPrecommit(const BftPrecommitMsg& precommit);
    /** Replay current-round messages to one newly connected peer. */
    bool SendConsensusTo(const std::string& address, uint16_t port,
        const std::optional<BftProposalMsg>& proposal,
        const std::optional<BftPrevoteMsg>& prevote,
        const std::optional<BftPrecommitMsg>& precommit);

private:
    using Endpoint = std::pair<std::string, uint16_t>;
    PeerSession* FindStorageSession(const std::string& address, uint16_t port, Endpoint* endpoint = nullptr);
    CybouNodeRuntime& m_runtime;
    boost::asio::io_context m_io;
    std::map<Endpoint, std::unique_ptr<PeerSession>> m_peers;
    std::optional<Endpoint> m_ping_cursor;
    std::optional<Endpoint> m_discovery_cursor;
    std::map<Endpoint, std::set<uint256>> m_announced_operations;
    std::map<Endpoint, std::set<uint256>> m_announced_blocks;
    // Last finalized height each peer reported in a BLOCK_RESULT ack, so the
    // fanout can skip (and mark announced) heads the peer already finalized
    // without spending the per-cycle offer budget on ancient history.
    std::map<Endpoint, uint64_t> m_peer_finalized_heights;
    // Operator-approved validator endpoints (canonical address:port form).
    std::set<Endpoint> m_explicit_endpoints;
    PeerConnectStatus m_last_connect_status{PeerConnectStatus::INVALID_REQUEST};
};

} // namespace cybou::p2p
#endif
