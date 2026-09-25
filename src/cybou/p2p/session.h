// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_SESSION_H
#define CYBOU_P2P_SESSION_H

#include <uint256.h>
#include <cybou/authority_node.h>
#include <cybou/bft_engine.h>

#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <chrono>
#include <optional>
#include <span>
#include <vector>

namespace cybou { class CybouNodeRuntime; }
namespace cybou::p2p {

inline constexpr uint32_t MAX_FRAME_PAYLOAD{4096};
inline constexpr uint8_t WIRE_VERSION{1};
inline constexpr uint64_t CAP_SERVE_BLOCKS{1ULL << 0};
inline constexpr uint64_t CAP_ACCEPT_OPERATIONS{1ULL << 1};
inline constexpr uint64_t CAP_OP_INVENTORY{1ULL << 2};
inline constexpr uint64_t CAP_BLOCK_INVENTORY{1ULL << 3};
inline constexpr uint64_t CAP_BLOCK_ANNOUNCEMENTS{1ULL << 4};
inline constexpr uint64_t CAP_CONSENSUS{1ULL << 5};
inline constexpr uint64_t CAP_PEER_DISCOVERY{1ULL << 6};
inline constexpr uint8_t MAX_BLOCK_INVENTORY{32};
// Shared bound for the peer discovery list: both the encoder and the decoder
// must enforce it so a malicious peer cannot stuff a PEERS frame with more
// entries than an honest node would ever send.
inline constexpr uint8_t MAX_PEER_DISCOVERY_ENTRIES{32};

enum class MessageType : uint8_t {
    HELLO = 1, PING = 2, PONG = 3, GET_BLOCK = 4, BLOCK_META = 5,
    BLOCK_CHUNK = 6, OP_META = 7, OP_CHUNK = 8, OP_RESULT = 9,
    OP_INV = 10, GET_OP = 11, OP = 12, GET_BLOCKS = 13, BLOCK_INV = 14,
    BLOCK_RESULT = 15,
    CONSENSUS_PROPOSAL = 16,
    CONSENSUS_PREVOTE = 17,
    CONSENSUS_PRECOMMIT = 18,
    GET_PEERS = 19,
    PEERS = 20,
};

struct Frame {
    MessageType type;
    std::vector<unsigned char> payload;
};

struct Hello {
    uint256 network_id;
    uint64_t finalized_height{0};
    uint256 finalized_tip;
    uint64_t capabilities{0};
    uint64_t nonce{0};

    friend bool operator==(const Hello&, const Hello&) = default;
};

enum class HandshakeStatus : uint8_t {
    NOT_ATTEMPTED,
    CONNECTED,
    UNAVAILABLE,
    INVALID_PEER,
    INVALID_LOCAL,
};

enum class BlockRequestStatus : uint8_t {
    OK, NOT_FOUND, UNAVAILABLE, INVALID_RESPONSE, INVALID_REQUEST,
};

struct BlockRequestResult {
    BlockRequestStatus status{BlockRequestStatus::INVALID_REQUEST};
    std::vector<unsigned char> bytes;
};

struct BlockAnnouncement {
    uint64_t height{0};
    uint256 block_id;
};

struct BlockInventoryResult {
    BlockRequestStatus status{BlockRequestStatus::INVALID_REQUEST};
    std::vector<BlockAnnouncement> blocks;
};

enum class BlockAnnounceResult : uint8_t { APPLIED = 0, ALREADY_HAVE = 1, GAP = 2 };

std::optional<std::vector<unsigned char>> EncodeFrame(const Frame& frame);
std::optional<Frame> DecodeFrame(std::span<const unsigned char> bytes);
std::vector<unsigned char> EncodeHello(const Hello& hello);
std::optional<Hello> DecodeHello(std::span<const unsigned char> bytes);
std::vector<unsigned char> EncodePeersPayload(const std::vector<std::pair<std::string, uint16_t>>& peers);
std::optional<std::vector<std::pair<std::string, uint16_t>>> DecodePeersPayload(std::span<const unsigned char> bytes);
bool MatchesKnownFinalizedChain(const CybouNodeRuntime& runtime, const Hello& peer);

// One persistent TCP socket. The caller owns connection setup and deadlines.
class PeerSession {
public:
    explicit PeerSession(boost::asio::ip::tcp::socket socket);
    bool Handshake(const Hello& local);
    HandshakeStatus LastHandshakeStatus() const { return m_handshake_status; }
    bool Ping(uint64_t nonce);
    bool AnswerPing();
    // Callers must verify returned blocks before commit.
    BlockRequestResult RequestBlock(uint64_t height);
    BlockInventoryResult RequestBlockInventory(uint64_t first_height, uint8_t max_blocks);
    // On success, peer_finalized_height receives the peer's finalized height
    // as reported in the BLOCK_RESULT acknowledgement (0 if not present).
    std::optional<BlockAnnounceResult> AdvertiseBlock(const BlockAnnouncement& announcement,
        const FinalizedBlock& block, uint64_t& peer_finalized_height);
    std::optional<OperationSubmitResult> SubmitOperation(const ProtocolOperation& operation);
    std::optional<OperationSubmitResult> AdvertiseOperation(const ProtocolOperation& operation);
    bool SendProposal(const BftProposalMsg& proposal);
    bool SendPrevote(const BftPrevoteMsg& prevote);
    bool SendPrecommit(const BftPrecommitMsg& precommit);
    std::optional<BftProposalMsg> ReadProposal(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10});
    std::optional<BftPrevoteMsg> ReadPrevote(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10});
    std::optional<BftPrecommitMsg> ReadPrecommit(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10});
    std::vector<std::pair<std::string, uint16_t>> RequestPeers(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    bool SendPeers(const std::vector<std::pair<std::string, uint16_t>>& peers,
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    bool ServeNext(CybouNodeRuntime& runtime);
    const std::optional<Hello>& Peer() const { return m_peer; }
    boost::asio::ip::tcp::socket& Socket() { return m_socket; }

private:
    bool ReadExact(unsigned char* out, size_t length, std::chrono::steady_clock::time_point deadline);
    bool WriteExact(const unsigned char* bytes, size_t length, std::chrono::steady_clock::time_point deadline);
    bool Write(const Frame& frame);
    bool Write(const Frame& frame, std::chrono::steady_clock::time_point deadline);
    std::optional<Frame> Read(std::chrono::steady_clock::time_point deadline);
    std::optional<Frame> Read();
    enum class ReadStatus : uint8_t { OK, UNAVAILABLE, INVALID_FRAME };
    ReadStatus m_last_read_status{ReadStatus::UNAVAILABLE};
    boost::asio::ip::tcp::socket m_socket;
    std::optional<Hello> m_peer;
    uint64_t m_local_capabilities{0};
    HandshakeStatus m_handshake_status{HandshakeStatus::NOT_ATTEMPTED};
};

} // namespace cybou::p2p
#endif
