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
#include <cybou/storage_audit.h>
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

/// \brief Максимальная полезная нагрузка одного transport frame: `64 * 1024` байт = `64 KiB`.
/// \details Предел ограничивает одно выделение памяти на сообщение и совпадает для отправки и приема.
inline constexpr uint32_t MAX_FRAME_PAYLOAD{64U * 1024U};
/// \brief Максимум блоков в одном `GET_BLOCKS`: `32`.
/// \details Соответствует ограничению P2P batch sync из `docs/cybou/08_P2P.md`.
inline constexpr uint8_t MAX_BLOCK_BATCH{32};
// Общая граница для discovery-списка: кодер и декодер должны проверять ее одинаково,
// чтобы злонамеренный пир не мог переполнить PEERS кадр сверх того, что честный узел сам отправляет.
/// \brief Максимум endpoint'ов в одном `PEERS`: `32`.
inline constexpr uint8_t MAX_PEER_DISCOVERY_ENTRIES{32};

/// \brief Тип одного P2P-сообщения на проводе.
enum class MessageType : uint8_t {
    HELLO = 1,                          ///< Обмен локальным `NetworkBinding`, финализованной высотой и nonce.
    PING = 2,                           ///< Liveness-проверка с echo nonce.
    PONG = 3,                           ///< Ответ на `PING`.
    GET_BLOCKS = 4,                     ///< Запрос диапазона финализованных блоков.
    BLOCK_META = 5,                     ///< Высота и полный размер одного блока в batch-ответе.
    BLOCK_DATA = 6,                     ///< Очередной chunk байтов блока после `BLOCK_META`.
    BLOCKS_END = 7,                     ///< Терминатор batch-ответа с точным количеством блоков.
    BLOCK_ANNOUNCE = 8,                 ///< Gossip-предложение одного финализованного блока.
    BLOCK_RESULT = 9,                   ///< Hop-by-hop ответ на `BLOCK_ANNOUNCE`.
    OP_POLL = 10,                       ///< Запрос одной relay кандидат-операции из очереди удаленного пира.
    OP_META = 11,                       ///< Размер serialized exact signed operation.
    OP_DATA = 12,                       ///< Очередной chunk байтов exact signed operation.
    OP_RESULT = 13,                     ///< Hop-by-hop итог локальной обработки кандидат-операции.
    GET_PEERS = 14,                     ///< Запрос discovery endpoint'ов.
    PEERS = 15,                         ///< Список числовых IPv4/IPv6 endpoint'ов.
    PUT_AUTHORIZED_CHUNK = 16,          ///< Запрос размещения финализованно-авторизованного encrypted chunk.
    AUTHORIZED_CHUNK_DATA = 17,         ///< Байты chunk'а после storage-метаданных.
    CHUNK_ADMISSION_RESULT = 18,        ///< Итог storage admission.
    GET_CHUNK_BY_ID = 19,               ///< Запрос stored chunk по `ChunkId`.
    CHUNK_DATA = 20,                    ///< Ответ с байтами chunk'а.
    GET_CHUNK_AUTHORIZATION_PROOF = 21, ///< Запрос Merkle proof для chunk'а.
    CHUNK_AUTHORIZATION_PROOF = 22,     ///< Ответ с proof авторизации chunk'а.
    STORAGE_PROOF = 23,                 ///< Доказательство владения storage-ключом, привязанное к TLS-сессии.
    VALIDATION_ATTESTATION_POLL = 24,   ///< Запрос одной `Validation` attestation из gossip-очереди пира.
    VALIDATION_ATTESTATION = 25,        ///< Gossip-сообщение с одной attestation.
    STORAGE_PROOF_REQUEST = 26,         ///< Challenge для on-demand доказательства `StorageId`.
    STORAGE_AUDIT_CHALLENGE = 27,       ///< Random-offset audit challenge по admitted chunk (DEC-276).
    STORAGE_AUDIT_RESPONSE = 28,        ///< Ответ на audit challenge по exact stored bytes.
};
/// \brief Наибольший допустимый wire-код сообщения в текущем baseline.
inline constexpr uint8_t MAX_MESSAGE_TYPE{static_cast<uint8_t>(MessageType::STORAGE_AUDIT_RESPONSE)};

/// \brief Стабильная identity storage-провайдера: BLAKE3 от его STORAGE public key.
using StorageId = std::array<unsigned char, 32>;
/// \brief Сообщение, которое storage-провайдер подписывает для доказательства ключа в текущей TLS-сессии.
struct Hello;
/// \param signer `HELLO` стороны, которая подписывает доказательство.
/// \param verifier `HELLO` стороны, которая проверяет доказательство.
/// \param tls_exporter 32-байтовый TLS exporter этой сессии.
/// \return Каноническое сообщение домена `CYBOU/STORAGE-PROOF`, либо пустой вектор при неверной длине exporter'а.
std::vector<unsigned char> StorageProofMessage(const Hello& signer, const Hello& verifier,
    std::span<const unsigned char> tls_exporter);
/// \brief Проверяет payload `STORAGE_PROOF` и возвращает доказанный `StorageId`.
/// \return `std::nullopt` при неверной длине payload или неуспешной cryptographic verification.
std::optional<StorageId> VerifyStorageProof(std::span<const unsigned char> payload,
    std::span<const unsigned char> message);

/// \brief Одно уже декодированное транспортное сообщение.
struct Frame {
    /// \brief Тип wire-сообщения.
    MessageType type;
    /// \brief Полезная нагрузка без 9-байтового транспортного заголовка.
    std::vector<unsigned char> payload;
};

/// \brief Параметры локальной цепочки, которые стороны обменивают в HELLO.
struct Hello {
    /// \brief `NetworkBinding` официальной сети, к которой привязан узел.
    cybou::Hash256 network_binding;
    /// \brief Последняя локально финализованная высота отправителя.
    uint64_t finalized_height{0};
    /// \brief `BlockID` локально финализованной вершины отправителя.
    cybou::Hash256 finalized_tip;
    /// \brief Случайный nonce рукопожатия для защиты от зеркального self-echo.
    uint64_t nonce{0};

    friend bool operator==(const Hello&, const Hello&) = default;
};

/// \brief Как данная сторона вошла в TLS-сеанс.
enum class TransportRole : uint8_t {
    CLIENT, ///< Эта сторона инициировала исходящее TCP/TLS-соединение.
    SERVER, ///< Эта сторона приняла входящее TCP/TLS-соединение.
};

/// \brief Необязательная стабильная TLS-identity/пин для сервисных сессий вроде bootstrap.
struct TlsSessionConfig {
    /// \brief PEM-цепочка сертификата для стабильной серверной TLS identity.
    std::filesystem::path certificate_chain_file;
    /// \brief PEM/private key для стабильной серверной TLS identity.
    std::filesystem::path private_key_file;
    /// \brief Необязательный SPKI pin ожидаемого удаленного TLS-сервера.
    std::optional<std::array<unsigned char, 32>> expected_server_spki_sha256;
};

/// \brief Вычисляет SHA-256 pin DER SubjectPublicKeyInfo у PEM-сертификата.
/// \param certificate_file PEM-файл сертификата или цепочки.
/// \return `std::nullopt` при ошибке чтения/разбора; иначе 32-байтовый digest SPKI.
std::optional<std::array<unsigned char, 32>> TlsCertificateSpkiSha256(
    const std::filesystem::path& certificate_file);

enum class HandshakeStatus : uint8_t {
    NOT_ATTEMPTED, ///< `Handshake()` еще не вызывался.
    CONNECTED,     ///< TLS и `HELLO` завершились успешно.
    UNAVAILABLE,   ///< Транспорт/TLS недоступен или прерван до валидного `HELLO`.
    INVALID_PEER,  ///< Удаленная сторона прислала некорректный wire/HELLO ответ.
    WRONG_NETWORK, ///< `NetworkBinding` удаленной стороны не совпал с локальным.
    INVALID_LOCAL, ///< Локальный `Hello` нарушил предусловия и рукопожатие не стартовало.
};

enum class BlockRequestStatus : uint8_t {
    OK,               ///< Получен корректный ответ и вызывающий код может продолжать потребление.
    NOT_FOUND,        ///< Удаленная сторона явно не имеет запрошенного диапазона.
    UNAVAILABLE,      ///< Сессия или транспорт недоступны.
    INVALID_RESPONSE, ///< Формат ответа, счетчики или последовательность кадров некорректны.
    INVALID_REQUEST,  ///< Локальные параметры вызова нарушили предусловия.
};

/// \brief Итог запроса одного блока.
struct BlockRequestResult {
    /// \brief Итог статуса single-block запроса.
    BlockRequestStatus status{BlockRequestStatus::INVALID_REQUEST};
    /// \brief Сырые байты блока при `status == OK`.
    std::vector<unsigned char> bytes;
};

/// \brief Одно объявление финализованного блока в gossip.
struct BlockAnnouncement {
    /// \brief Высота предлагаемого финализованного блока.
    uint64_t height{0};
    /// \brief Канонический `BlockID` этого блока.
    cybou::Hash256 block_id;
};

/// \brief Итог пакетного запроса блоков.
struct BlockBatchResult {
    /// \brief Итог статуса batch-запроса.
    BlockRequestStatus status{BlockRequestStatus::INVALID_REQUEST};
    /// \brief Число фактически принятых блоков в текущем ответе.
    uint8_t count{0};
};

/// \brief Локальный исход одного `BLOCK_ANNOUNCE`.
enum class BlockAnnounceResult : uint8_t {
    APPLIED = 0,      ///< Блок или его анонс принят, peer может продолжать дальше.
    ALREADY_HAVE = 1, ///< Локальный узел уже знает или финализовал этот блок.
    GAP = 2,          ///< Между локальной вершиной и объявленным блоком есть разрыв; нужен sync через `GET_BLOCKS`.
};

/// \brief Кодирует transport frame в точные проводные байты.
/// \return `std::nullopt`, если payload слишком велик или тип сообщения неподдерживаем.
std::optional<std::vector<unsigned char>> EncodeFrame(const Frame& frame);
/// \brief Декодирует transport frame из проводных байтов.
/// \return `std::nullopt`, если заголовок, длина или тип сообщения некорректны.
std::optional<Frame> DecodeFrame(std::span<const unsigned char> bytes);
/// \brief Кодирует HELLO в фиксированный бинарный формат.
std::vector<unsigned char> EncodeHello(const Hello& hello);
/// \brief Декодирует HELLO из фиксированного бинарного формата.
/// \return `std::nullopt`, если длина, нулевые поля или nonce недопустимы.
std::optional<Hello> DecodeHello(std::span<const unsigned char> bytes);
/// \brief Кодирует список endpoint'ов для payload сообщения `PEERS`.
std::vector<unsigned char> EncodePeersPayload(const std::vector<std::pair<std::string, uint16_t>>& peers);
/// \brief Декодирует payload `PEERS` в список числовых endpoint'ов.
/// \return `std::nullopt`, если payload усечен, содержит неизвестное семейство адресов или нарушает лимиты.
std::optional<std::vector<std::pair<std::string, uint16_t>>> DecodePeersPayload(std::span<const unsigned char> bytes);
/// \brief Сверяет заявленную финализованную вершину пира с локально известной цепочкой.
/// \details Это liveness/consistency фильтр на уже известную историю; он не заменяет независимую проверку каждого блока.
bool MatchesKnownFinalizedChain(const CybouNodeRuntime& runtime, const Hello& peer);

/// \brief Одна долговечная TCP/TLS-сессия с пиром.
/// \details Вызывающая сторона владеет самим соединением, его жизненным циклом и дедлайнами повторных вызовов.
class PeerSession {
public:
    /// \brief Принимает уже открытый сокет и настраивает транспортную роль.
    /// \param socket Уже подключенный TCP-сокет.
    /// \param transport_role Клиентская или серверная сторона сеанса.
    /// \param tls_config Необязательная стабильная TLS identity/пин.
    explicit PeerSession(boost::asio::ip::tcp::socket socket, TransportRole transport_role,
        TlsSessionConfig tls_config = {});
    ~PeerSession();
    PeerSession(const PeerSession&) = delete;
    PeerSession& operator=(const PeerSession&) = delete;
    /// \brief Выполняет TLS+HELLO рукопожатие и запоминает параметры пира.
    /// \param local Локальный `HELLO` с ненулевыми `network_binding`, `finalized_tip` и `nonce`.
    /// \return `true` только если транспорт защищен и удаленная сторона ответила корректным `HELLO` той же сети.
    /// \pre Объект не должен использоваться конкурентно из нескольких потоков.
    bool Handshake(const Hello& local);
    /// \brief Добывает on-demand proof storage-identity, привязанный к текущему TLS-каналу.
    /// \return Закэшированный или только что доказанный `StorageId`; `std::nullopt` при любой ошибке challenge/verify.
    std::optional<StorageId> ProveStorageIdentity();
    const std::optional<StorageId>& PeerStorageId() const { return m_peer_storage_id; }
    HandshakeStatus LastHandshakeStatus() const { return m_handshake_status; }
    /// \brief Отправляет ping с nonce и ожидает ответный pong.
    /// \param nonce Ненулевой echo-marker.
    bool Ping(uint64_t nonce);
    /// \brief Отвечает на входящий ping текущей сессии.
    bool AnswerPing();
    /// \brief Запрашивает один финализованный блок по высоте.
    /// \details Вызывающая сторона обязана повторно проверить возвращенные байты перед commit.
    BlockRequestResult RequestBlock(uint64_t height);
    /// \brief Запрашивает batch последовательных финализованных блоков.
    /// \param first_height Первая требуемая высота.
    /// \param max_blocks Максимум блоков, не больше `MAX_BLOCK_BATCH`.
    /// \param consume Callback, вызываемый по одному разу на каждый уже собранный блок.
    BlockBatchResult RequestBlocks(uint64_t first_height, uint8_t max_blocks,
        const std::function<bool(uint64_t, std::span<const unsigned char>)>& consume);
    /// \brief Предлагает удаленному пиру один финализованный блок через `BLOCK_ANNOUNCE`.
    /// \param announcement Высота и `BlockID` предлагаемого блока.
    /// \param block Сам финализованный блок для немедленной передачи по запросу.
    /// \param peer_finalized_height [out] Последняя высота, которую peer подтвердил в `BLOCK_RESULT`, если ответ дошел.
    std::optional<BlockAnnounceResult> AdvertiseBlock(const BlockAnnouncement& announcement,
        const FinalizedBlock& block, uint64_t& peer_finalized_height);
    /// \brief Отправляет exact signed operation и возвращает hop-by-hop результат.
    std::optional<OperationSubmitResult> SubmitOperation(const ProtocolOperation& operation, uint64_t work_nonce);
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
    /// \brief Отправляет storage audit challenge и возвращает ответ provider.
    std::optional<StorageAuditAnswer> AuditChunk(const StorageAuditChallenge& challenge);
    /// \brief Обслуживает одно входящее сообщение сервера и выполняет локальный ответ.
    /// \return `false`, если сессию следует считать завершенной или небезопасной для продолжения.
    bool ServeNext(CybouNodeRuntime& runtime);
    const std::optional<Hello>& Peer() const { return m_peer; }
    /// \brief Отправляет один уже сформированный аутентифицированный P2P frame.
    /// \pre `Handshake()` уже завершил TLS и сеанс находится в живом состоянии.
    bool SendFrame(const Frame& frame,
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    /// \brief Принимает один аутентифицированный P2P frame.
    /// \return `std::nullopt` при EOF, timeout или некорректном кадре.
    std::optional<Frame> ReceiveFrame(
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5});
    boost::asio::ip::tcp::socket& Socket() { return m_socket; }

private:
    bool SendBlock(uint64_t height, std::span<const unsigned char> bytes,
        std::chrono::steady_clock::time_point deadline);
    bool SendOperation(std::span<const unsigned char> bytes, uint64_t work_nonce,
        std::chrono::steady_clock::time_point deadline);
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
    /// \brief Локальный итог последнего низкоуровневого чтения frame.
    enum class ReadStatus : uint8_t {
        OK,            ///< Кадр успешно прочитан и декодирован.
        UNAVAILABLE,   ///< Сокет/TLS недоступен, закрыт или истек deadline.
        INVALID_FRAME, ///< Поток нарушил wire-формат заголовка или payload.
    };
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
    /// \brief Финализованная база, для которой уже выдавались attestation'ы этому пиру.
    cybou::Hash256 m_served_attestation_base;
    /// \brief Attestation'ы, уже выданные этому пиру на текущей финализованной базе, чтобы не дублировать gossip в пределах сеанса.
    std::set<ValidationPool::Key> m_served_attestations;
    std::set<cybou::Hash256> m_served_operations;
    HandshakeStatus m_handshake_status{HandshakeStatus::NOT_ATTEMPTED};
};

} // namespace cybou::p2p
#endif
