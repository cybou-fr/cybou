// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_SESSION_H
#define CYBOU_P2P_SESSION_H

#include <uint256.h>
#include <cybou/account_id.h>
#include <cybou/poa_finalizer.h>
#include <cybou/finalized_chunk_store.h>
#include <cybou/operation_relay.h>
#include <cybou/validation_pool.h>

#include <boost/asio/ip/tcp.hpp>

#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <cstdint>
#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace cybou { class CybouNodeRuntime; }
namespace cybou::p2p {

inline constexpr uint32_t MAX_FRAME_PAYLOAD{4096};
inline constexpr uint8_t WIRE_VERSION{5};
inline constexpr uint8_t MAX_BLOCK_INVENTORY{32};
// Shared bound for the peer discovery list: both the encoder and the decoder
// must enforce it so a malicious peer cannot stuff a PEERS frame with more
// entries than an honest node would ever send.
inline constexpr uint8_t MAX_PEER_DISCOVERY_ENTRIES{32};

enum class MessageType : uint8_t {
    HELLO = 1, PING = 2, PONG = 3, GET_BLOCK = 4, BLOCK_META = 5,
    BLOCK_CHUNK = 6, OP_META = 7, OP_CHUNK = 8, OP_RESULT = 9,
    RESERVED_10 = 10, RESERVED_11 = 11, RESERVED_12 = 12,
    GET_BLOCKS = 13, BLOCK_INV = 14,
    BLOCK_RESULT = 15,
    RESERVED_16 = 16,
    RESERVED_17 = 17,
    RESERVED_18 = 18,
    GET_PEERS = 19,
    PEERS = 20,
    RESERVED_21 = 21,
    RESERVED_22 = 22,
    RESERVED_23 = 23,
    RESERVED_24 = 24,
    RESERVED_25 = 25,
    RESERVED_26 = 26,
    RESERVED_27 = 27,
    RESERVED_28 = 28,
    PUT_AUTHORIZED_CHUNK = 29,
    AUTHORIZED_CHUNK_DATA = 30,
    CHUNK_ADMISSION_RESULT = 31,
    GET_CHUNK_BY_ID = 32,
    CHUNK_DATA = 33,
    GET_CHUNK_AUTHORIZATION_PROOF = 34,
    CHUNK_AUTHORIZATION_PROOF = 35,
    PROVIDER_PROOF = 36,
    RESERVED_37 = 37, RESERVED_38 = 38, RESERVED_39 = 39,
    RESERVED_41 = 41,
    RESERVED_42 = 42,
    RESERVED_43 = 43,
    OPERATION_RELAY_POLL = 44,
    OPERATION_RELAY_OPERATION_META = 45,
    OPERATION_RELAY_OPERATION_CHUNK = 46,
    OPERATION_RELAY_ACK = 47,
    OPERATION_RELAY_ACK_RESULT = 48,
    /** Pull one Validation attestation this session has not yet served. */
    VALIDATION_ATTESTATION_POLL = 49,
    /** One serialized ValidationAttestation, or an empty payload when none is new. */
    VALIDATION_ATTESTATION = 50,
    GET_PROVIDER_PROOF = 51,
};
inline constexpr uint8_t MAX_MESSAGE_TYPE{static_cast<uint8_t>(MessageType::GET_PROVIDER_PROOF)};

/** Stable identity of a storage provider: BLAKE3 of its STORAGE_PROVIDER public key. */
using ProviderId = std::array<unsigned char, 32>;
/** Message a provider signs to prove its key in this TLS session. */
struct Hello;
std::vector<unsigned char> ProviderProofMessage(const Hello& signer, const Hello& verifier,
    std::span<const unsigned char> tls_exporter);
/** Verifies a PROVIDER_PROOF payload and returns the proven ProviderID. */
std::optional<ProviderId> VerifyProviderProof(std::span<const unsigned char> payload,
    std::span<const unsigned char> message);
struct Frame {
    MessageType type;
    std::vector<unsigned char> payload;
};

struct Hello {
    uint256 network_binding;
    uint64_t finalized_height{0};
    uint256 finalized_tip;
    uint64_t nonce{0};

    friend bool operator==(const Hello&, const Hello&) = default;
};

enum class TransportRole : uint8_t { CLIENT, SERVER };

/** Optional stable TLS identity/pin for service sessions such as bootstrap. */
struct TlsSessionConfig {
    std::filesystem::path certificate_chain_file;
    std::filesystem::path private_key_file;
    std::optional<std::array<unsigned char, 32>> expected_server_spki_sha256;
};

/** SHA-256 pin of the DER SubjectPublicKeyInfo in a PEM certificate. */
std::optional<std::array<unsigned char, 32>> TlsCertificateSpkiSha256(
    const std::filesystem::path& certificate_file);

enum class HandshakeStatus : uint8_t {
    NOT_ATTEMPTED,
    CONNECTED,
    UNAVAILABLE,
    INVALID_PEER,
    WRONG_NETWORK,
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
    explicit PeerSession(boost::asio::ip::tcp::socket socket, TransportRole transport_role,
        TlsSessionConfig tls_config = {});
    ~PeerSession();
    PeerSession(const PeerSession&) = delete;
    PeerSession& operator=(const PeerSession&) = delete;
    bool Handshake(const Hello& local);
    /** On-demand, channel-bound storage relationship proof; never part of HELLO. */
    std::optional<ProviderId> ProveStorageIdentity();
    const std::optional<ProviderId>& PeerProviderId() const { return m_peer_provider_id; }
    HandshakeStatus LastHandshakeStatus() const { return m_handshake_status; }
    bool Ping(uint64_t nonce);
    bool AnswerPing();
    // Callers must verify returned blocks before commit.
    BlockRequestResult RequestBlock(uint64_t height);
    /**
     * Pipelined block transfer: send several GET_BLOCK requests at once, then
     * read their responses in the same order. The peer answers requests in
     * order, so one round trip covers a whole batch instead of one per block.
     */
    bool SendBlockRequest(uint64_t height);
    BlockRequestResult ReadBlockResponse();
    BlockInventoryResult RequestBlockInventory(uint64_t first_height, uint8_t max_blocks);
    // On success, peer_finalized_height receives the peer's finalized height
    // as reported in the BLOCK_RESULT acknowledgement (0 if not present).
    std::optional<BlockAnnounceResult> AdvertiseBlock(const BlockAnnouncement& announcement,
        const FinalizedBlock& block, uint64_t& peer_finalized_height);
    std::optional<OperationSubmitResult> SubmitOperation(const ProtocolOperation& operation);
    bool PollOperationRelay(CybouNodeRuntime& runtime);
    /** Pull one attestation; the local runtime re-verifies it against its own candidate and state. */
    bool PollValidationAttestation(CybouNodeRuntime& runtime);
    std::vector<std::pair<std::string, uint16_t>> RequestPeers(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    bool SendPeers(const std::vector<std::pair<std::string, uint16_t>>& peers,
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    std::optional<ChunkAdmissionResult> PutAuthorizedChunk(const uint256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof);
    std::optional<std::vector<unsigned char>> GetChunkById(const ChunkId& chunk_id);
    std::optional<ChunkAuthorizationProof> GetChunkAuthorizationProof(
        const uint256& publication_operation_id, const ChunkId& chunk_id);
    bool ServeNext(CybouNodeRuntime& runtime);
    const std::optional<Hello>& Peer() const { return m_peer; }
    /** One authenticated CYP2 frame for bounded peer extensions and protocol tests. */
    bool SendFrame(const Frame& frame,
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    std::optional<Frame> ReceiveFrame(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    boost::asio::ip::tcp::socket& Socket() { return m_socket; }

private:
    bool EstablishSecureTransport(std::chrono::steady_clock::time_point deadline);
    bool AdvanceTlsOperation(int result, std::chrono::steady_clock::time_point deadline);
    bool ReadExact(unsigned char* out, size_t length, std::chrono::steady_clock::time_point deadline);
    bool WriteExact(const unsigned char* bytes, size_t length, std::chrono::steady_clock::time_point deadline);
    bool Write(const Frame& frame);
    bool Write(const Frame& frame, std::chrono::steady_clock::time_point deadline);
    std::optional<Frame> Read(std::chrono::steady_clock::time_point deadline);
    std::optional<Frame> Read();
    enum class ReadStatus : uint8_t { OK, UNAVAILABLE, INVALID_FRAME };
    ReadStatus m_last_read_status{ReadStatus::UNAVAILABLE};
    boost::asio::ip::tcp::socket m_socket;
    TransportRole m_transport_role;
    TlsSessionConfig m_tls_config;
    SSL_CTX* m_owned_ssl_context{nullptr};
    SSL* m_ssl{nullptr};
    std::array<unsigned char, 32> m_tls_exporter{};
    std::optional<Hello> m_peer;
    std::optional<ProviderId> m_peer_provider_id;
    std::optional<Hello> m_local;
    // Attestations already served to this peer on the current finalized base.
    uint256 m_served_attestation_base;
    std::set<ValidationPool::Key> m_served_attestations;
    HandshakeStatus m_handshake_status{HandshakeStatus::NOT_ATTEMPTED};
};

} // namespace cybou::p2p
#endif
