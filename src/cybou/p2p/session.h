// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_SESSION_H
#define CYBOU_P2P_SESSION_H

#include <uint256.h>
#include <cybou/account_id.h>
#include <cybou/bootstrap_identity.h>
#include <cybou/finalizer_node.h>
#include <cybou/finalized_chunk_store.h>
#include <cybou/bootstrap_operation_relay.h>

#include <boost/asio/ip/tcp.hpp>

#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <cstdint>
#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace cybou { class CybouNodeRuntime; }
namespace cybou::p2p {

inline constexpr uint32_t MAX_FRAME_PAYLOAD{4096};
inline constexpr uint32_t MAX_BOOTSTRAP_FRAME_PAYLOAD{16 * 1024 * 1024 + 16 * 1024};
inline constexpr uint8_t WIRE_VERSION{3};
inline constexpr uint64_t CAP_SERVE_BLOCKS{1ULL << 0};
inline constexpr uint64_t CAP_ACCEPT_OPERATIONS{1ULL << 1};
inline constexpr uint64_t CAP_BLOCK_INVENTORY{1ULL << 3};
inline constexpr uint64_t CAP_BLOCK_ANNOUNCEMENTS{1ULL << 4};
inline constexpr uint64_t CAP_PEER_DISCOVERY{1ULL << 6};
inline constexpr uint64_t CAP_STORAGE{1ULL << 7};
inline constexpr uint64_t CAP_STORAGE_PROOFS{1ULL << 8};
inline constexpr uint64_t CAP_BOOTSTRAP{1ULL << 9};
/** Genesis-granted bootstrap endpoint can relay operations to its live finalizer session. */
inline constexpr uint64_t CAP_OPERATION_RELAY{1ULL << 10};
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
    FINALIZER_PROOF = 40,
    BOOTSTRAP_REQUEST = 41,
    BOOTSTRAP_RESPONSE = 42,
    BOOTSTRAP_PROOF = 43,
    BOOTSTRAP_RELAY_POLL = 44,
    BOOTSTRAP_RELAY_OPERATION_META = 45,
    BOOTSTRAP_RELAY_OPERATION_CHUNK = 46,
    BOOTSTRAP_RELAY_ACK = 47,
    BOOTSTRAP_RELAY_ACK_RESULT = 48,
};
inline constexpr uint8_t MAX_MESSAGE_TYPE{static_cast<uint8_t>(MessageType::BOOTSTRAP_RELAY_ACK_RESULT)};

/** Stable identity of a storage provider: BLAKE3 of its STORAGE_PROVIDER public key. */
using ProviderId = std::array<unsigned char, 32>;
/**
 * Signs a PROVIDER_PROOF message with the local provider key and returns the
 * encoded proof payload (public key + hybrid signature), or nullopt.
 */
using ProviderProofSigner = std::function<std::optional<std::vector<unsigned char>>(
    std::span<const unsigned char> message)>;
using FinalizerProofSigner = ProviderProofSigner;
using BootstrapProofSigner = ProviderProofSigner;
using BootstrapIdentityResolver = std::function<std::optional<IdentityHybridPublicKey>(const AccountId&)>;

struct BootstrapProofIdentity {
    AccountId account_id;
    BootstrapProofSigner signer;
};

/** Message a provider signs to prove its key in this TLS session. */
struct Hello;
std::vector<unsigned char> ProviderProofMessage(const Hello& signer, const Hello& verifier,
    std::span<const unsigned char> tls_exporter);
/** Verifies a PROVIDER_PROOF payload and returns the proven ProviderID. */
std::optional<ProviderId> VerifyProviderProof(std::span<const unsigned char> payload,
    std::span<const unsigned char> message);
/** Channel-bound challenge a genesis-key finalizer signs for transport authentication. */
std::vector<unsigned char> FinalizerProofMessage(const Hello& signer, const Hello& verifier,
    std::span<const unsigned char> tls_exporter);
bool VerifyFinalizerProof(std::span<const unsigned char> payload, std::span<const unsigned char> message,
    const IdentityHybridPublicKey& genesis_finalizer_key);
/** Session-bound proof for a genesis-granted bootstrap AccountID. */
std::vector<unsigned char> BootstrapProofMessage(const Hello& signer, const Hello& verifier,
    std::span<const unsigned char> tls_exporter, const AccountId& account_id);
std::optional<AccountId> VerifyBootstrapProof(std::span<const unsigned char> payload,
    std::span<const unsigned char> message, const IdentityHybridPublicKey& authorization_key);

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
    /**
     * A peer advertising CAP_STORAGE must prove its provider key; a local
     * CAP_STORAGE hello needs provider_signer to do the same.
     */
    bool Handshake(const Hello& local, const ProviderProofSigner& provider_signer = {},
        const FinalizerProofSigner& finalizer_signer = {},
        const IdentityHybridPublicKey* genesis_finalizer_key = nullptr,
        const BootstrapProofIdentity* local_bootstrap_identity = nullptr,
        const BootstrapIdentityResolver& bootstrap_identity_resolver = {});
    /** Proven ProviderID of a storage peer. */
    const std::optional<ProviderId>& PeerProviderId() const { return m_peer_provider_id; }
    /** True only while this live session has verified the peer's genesis PoA proof. */
    bool PeerFinalizerAuthenticated() const { return m_peer_finalizer_authenticated; }
    bool PeerBootstrapAuthenticated() const { return m_peer_bootstrap_account.has_value(); }
    const std::optional<AccountId>& PeerBootstrapAccountId() const { return m_peer_bootstrap_account; }
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
    bool PollBootstrapRelay(CybouNodeRuntime& runtime);
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
    bool ServeNext(CybouNodeRuntime& runtime,
        std::optional<BootstrapOperationRelay::FinalizerSession> relay_session = std::nullopt);
    const std::optional<Hello>& Peer() const { return m_peer; }
    /** One authenticated CYP2 frame for bounded peer extensions and protocol tests. */
    bool SendFrame(const Frame& frame,
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    std::optional<Frame> ReceiveFrame(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    /** One pinned bootstrap protocol exchange without a network-bound HELLO. */
    std::optional<Frame> RequestBootstrap(const Frame& request);
    /** Pinned pre-genesis endpoint proves AccountID + Recovery on this TLS session. */
    std::optional<BootstrapIdentityClaim> RequestBootstrapIdentityClaim();
    bool ServeBootstrapIdentityClaim(const AccountId& account_id,
        const IdentityHybridPublicKey& recovery_key,
        const ProviderProofSigner& recovery_signer);
    /** Serve one bootstrap request over a persistent-identity TLS session. */
    bool ServeBootstrapRequest(const std::function<std::optional<Frame>(const Frame&)>& handler);
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
    // Ephemeral session fact. Never serialize or persist an Authority address
    // or map this role to a durable node identity.
    bool m_peer_finalizer_authenticated{false};
    std::optional<AccountId> m_peer_bootstrap_account;
    uint64_t m_local_capabilities{0};
    HandshakeStatus m_handshake_status{HandshakeStatus::NOT_ATTEMPTED};
};

} // namespace cybou::p2p
#endif
