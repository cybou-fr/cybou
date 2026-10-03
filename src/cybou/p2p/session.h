// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Русский публичный API одной TLS-защищенной P2P-сессии CYBOU.

#ifndef CYBOU_P2P_SESSION_H
#define CYBOU_P2P_SESSION_H

#include <cybou/operation_submit.h>
#include <cybou/hash256.h>
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

inline constexpr uint32_t MAX_FRAME_PAYLOAD{64U * 1024U};
inline constexpr uint8_t MAX_BLOCK_BATCH{32};
// Общая граница для discovery-списка: кодер и декодер должны проверять ее одинаково,
// чтобы злонамеренный пир не мог переполнить PEERS кадр сверх того, что честный узел сам отправляет.
inline constexpr uint8_t MAX_PEER_DISCOVERY_ENTRIES{32};

/// \brief Тип одного P2P-сообщения на проводе.
enum class MessageType : uint8_t {
    HELLO = 1,
    PING = 2,
    PONG = 3,
    GET_BLOCKS = 4,
    BLOCK_META = 5,
    BLOCK_DATA = 6,
    BLOCKS_END = 7,
    BLOCK_ANNOUNCE = 8,
    BLOCK_RESULT = 9,
    OP_POLL = 10,
    OP_META = 11,
    OP_DATA = 12,
    OP_RESULT = 13,
    GET_PEERS = 14,
    PEERS = 15,
    PUT_AUTHORIZED_CHUNK = 16,
    AUTHORIZED_CHUNK_DATA = 17,
    CHUNK_ADMISSION_RESULT = 18,
    GET_CHUNK_BY_ID = 19,
    CHUNK_DATA = 20,
    GET_CHUNK_AUTHORIZATION_PROOF = 21,
    CHUNK_AUTHORIZATION_PROOF = 22,
    STORAGE_PROOF = 23,
    VALIDATION_ATTESTATION_POLL = 24,
    VALIDATION_ATTESTATION = 25,
    STORAGE_PROOF_REQUEST = 26,
};
inline constexpr uint8_t MAX_MESSAGE_TYPE{static_cast<uint8_t>(MessageType::STORAGE_PROOF_REQUEST)};

/// \brief Стабильная identity storage-провайдера: BLAKE3 от его STORAGE public key.
using StorageId = std::array<unsigned char, 32>;
/// \brief Сообщение, которое storage-провайдер подписывает для доказательства ключа в текущей TLS-сессии.
struct Hello;
std::vector<unsigned char> StorageProofMessage(const Hello& signer, const Hello& verifier,
    std::span<const unsigned char> tls_exporter);
/// \brief Проверяет payload `STORAGE_PROOF` и возвращает доказанный `StorageId`.
std::optional<StorageId> VerifyStorageProof(std::span<const unsigned char> payload,
    std::span<const unsigned char> message);

/// \brief Одно уже декодированное транспортное сообщение.
struct Frame {
    MessageType type;
    std::vector<unsigned char> payload;
};

/// \brief Параметры локальной цепочки, которые стороны обменивают в HELLO.
struct Hello {
    cybou::Hash256 network_binding;
    uint64_t finalized_height{0};
    cybou::Hash256 finalized_tip;
    uint64_t nonce{0};

    friend bool operator==(const Hello&, const Hello&) = default;
};

enum class TransportRole : uint8_t { CLIENT, SERVER };

/// \brief Необязательная стабильная TLS-identity/пин для сервисных сессий вроде bootstrap.
struct TlsSessionConfig {
    std::filesystem::path certificate_chain_file;
    std::filesystem::path private_key_file;
    std::optional<std::array<unsigned char, 32>> expected_server_spki_sha256;
};

/// \brief Вычисляет SHA-256 pin DER SubjectPublicKeyInfo у PEM-сертификата.
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

/// \brief Итог запроса одного блока.
struct BlockRequestResult {
    BlockRequestStatus status{BlockRequestStatus::INVALID_REQUEST};
    std::vector<unsigned char> bytes;
};

/// \brief Одно объявление финализованного блока в gossip.
struct BlockAnnouncement {
    uint64_t height{0};
    cybou::Hash256 block_id;
};

/// \brief Итог пакетного запроса блоков.
struct BlockBatchResult {
    BlockRequestStatus status{BlockRequestStatus::INVALID_REQUEST};
    uint8_t count{0};
};

enum class BlockAnnounceResult : uint8_t { APPLIED = 0, ALREADY_HAVE = 1, GAP = 2 };

/// \brief Кодирует transport frame в точные проводные байты.
std::optional<std::vector<unsigned char>> EncodeFrame(const Frame& frame);
/// \brief Декодирует transport frame из проводных байтов.
std::optional<Frame> DecodeFrame(std::span<const unsigned char> bytes);
/// \brief Кодирует HELLO в фиксированный бинарный формат.
std::vector<unsigned char> EncodeHello(const Hello& hello);
/// \brief Декодирует HELLO из фиксированного бинарного формата.
std::optional<Hello> DecodeHello(std::span<const unsigned char> bytes);
/// \brief Кодирует список endpoint'ов для payload сообщения `PEERS`.
std::vector<unsigned char> EncodePeersPayload(const std::vector<std::pair<std::string, uint16_t>>& peers);
/// \brief Декодирует payload `PEERS` в список числовых endpoint'ов.
std::optional<std::vector<std::pair<std::string, uint16_t>>> DecodePeersPayload(std::span<const unsigned char> bytes);
/// \brief Сверяет заявленную финализованную вершину пира с локально известной цепочкой.
bool MatchesKnownFinalizedChain(const CybouNodeRuntime& runtime, const Hello& peer);

/// \brief Одна долговечная TCP/TLS-сессия с пиром.
/// \details Вызывающая сторона владеет самим соединением, его жизненным циклом и дедлайнами повторных вызовов.
class PeerSession {
public:
    /// \brief Принимает уже открытый сокет и настраивает транспортную роль.
    explicit PeerSession(boost::asio::ip::tcp::socket socket, TransportRole transport_role,
        TlsSessionConfig tls_config = {});
    ~PeerSession();
    PeerSession(const PeerSession&) = delete;
    PeerSession& operator=(const PeerSession&) = delete;
    /// \brief Выполняет TLS+HELLO рукопожатие и запоминает параметры пира.
    bool Handshake(const Hello& local);
    /// \brief Добывает on-demand proof storage-identity, привязанный к текущему TLS-каналу.
    std::optional<StorageId> ProveStorageIdentity();
    const std::optional<StorageId>& PeerStorageId() const { return m_peer_storage_id; }
    HandshakeStatus LastHandshakeStatus() const { return m_handshake_status; }
    /// \brief Отправляет ping с nonce и ожидает ответный pong.
    bool Ping(uint64_t nonce);
    /// \brief Отвечает на входящий ping текущей сессии.
    bool AnswerPing();
    // Вызывающая сторона обязана повторно проверить возвращенные блоки перед commit.
    BlockRequestResult RequestBlock(uint64_t height);
    BlockBatchResult RequestBlocks(uint64_t first_height, uint8_t max_blocks,
        const std::function<bool(uint64_t, std::span<const unsigned char>)>& consume);
    // При успехе peer_finalized_height получает финализованную высоту пира из BLOCK_RESULT (0, если поле отсутствует).
    std::optional<BlockAnnounceResult> AdvertiseBlock(const BlockAnnouncement& announcement,
        const FinalizedBlock& block, uint64_t& peer_finalized_height);
    /// \brief Отправляет exact signed operation и возвращает hop-by-hop результат.
    std::optional<OperationSubmitResult> SubmitOperation(const ProtocolOperation& operation);
    /// \brief Забирает одну relay-операцию у удаленного пира и передает ее runtime.
    bool PollOperationRelay(CybouNodeRuntime& runtime);
    /// \brief Забирает одну Validation-attestation; runtime затем перепроверяет ее на своем состоянии.
    bool PollValidationAttestation(CybouNodeRuntime& runtime);
    /// \brief Запрашивает у пира список известных endpoint'ов.
    std::vector<std::pair<std::string, uint16_t>> RequestPeers(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    /// \brief Отправляет ограниченный список endpoint'ов в ответ на `GET_PEERS`.
    bool SendPeers(const std::vector<std::pair<std::string, uint16_t>>& peers,
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    /// \brief Отправляет авторизованный encrypted chunk storage-пиру.
    std::optional<ChunkAdmissionResult> PutAuthorizedChunk(const cybou::Hash256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof);
    /// \brief Запрашивает encrypted chunk по его ChunkId.
    std::optional<std::vector<unsigned char>> GetChunkById(const ChunkId& chunk_id);
    /// \brief Запрашивает Merkle proof авторизации chunk'а.
    std::optional<ChunkAuthorizationProof> GetChunkAuthorizationProof(
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id);
    /// \brief Обслуживает одно входящее сообщение сервера и выполняет локальный ответ.
    bool ServeNext(CybouNodeRuntime& runtime);
    const std::optional<Hello>& Peer() const { return m_peer; }
    /// \brief Отправляет один уже сформированный аутентифицированный P2P frame.
    bool SendFrame(const Frame& frame,
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    /// \brief Принимает один аутентифицированный P2P frame.
    std::optional<Frame> ReceiveFrame(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    boost::asio::ip::tcp::socket& Socket() { return m_socket; }

private:
    bool SendBlock(uint64_t height, std::span<const unsigned char> bytes,
        std::chrono::steady_clock::time_point deadline);
    bool SendOperation(std::span<const unsigned char> bytes, std::chrono::steady_clock::time_point deadline);
    std::optional<OperationSubmitResult> ReceiveOperation(const Frame& meta, CybouNodeRuntime& runtime,
        bool allow_seen_retry, std::chrono::steady_clock::time_point deadline);
    bool SendOperationResult(const OperationSubmitResult& result, std::chrono::steady_clock::time_point deadline);
    std::optional<OperationSubmitResult> ReadOperationResult(const cybou::Hash256& operation_id,
        std::chrono::steady_clock::time_point deadline);
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
    std::optional<StorageId> m_peer_storage_id;
    std::optional<Hello> m_local;
    // Attestations already served to this peer on the current finalized base.
    cybou::Hash256 m_served_attestation_base;
    std::set<ValidationPool::Key> m_served_attestations;
    HandshakeStatus m_handshake_status{HandshakeStatus::NOT_ATTEMPTED};
};

} // namespace cybou::p2p
#endif
