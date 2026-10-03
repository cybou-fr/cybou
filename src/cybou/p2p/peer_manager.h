// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Русский публичный API single-threaded менеджера outbound-пиров CYBOU.

#ifndef CYBOU_P2P_PEER_MANAGER_H
#define CYBOU_P2P_PEER_MANAGER_H

#include <cybou/operation_submit.h>
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

/// \brief Итог попытки соединения с пиром.
enum class PeerConnectStatus : uint8_t {
    CONNECTED,
    UNAVAILABLE,
    HANDSHAKE_FAILED,
    WRONG_NETWORK,
    ADMISSION_REJECTED,
    INVALID_REQUEST,
    LOCAL_FAILURE,
};

/// \brief Наблюдаемая информация о подключенном пире.
struct PeerInfo {
    std::string address;
    uint16_t port{0};
    Hello hello;
    /** Proven StorageId for storage peers. */
    std::optional<StorageId> storage_id;
};

/// \brief Итог отправки операции в конкретный пир или в выбранный маршрут fanout.
struct PeerSubmitResult {
    cybou::Hash256 op_id;
    std::optional<OperationSubmitResult> acknowledgment;
    std::optional<std::pair<std::string, uint16_t>> endpoint;
    bool delivery_uncertain{false};

    explicit operator bool() const { return acknowledgment && static_cast<bool>(*acknowledgment); }
};

/// \brief Single-threaded набор outbound-пиров.
/// \details Вызывающий код сам планирует соединения и health-check'и; этот класс не выдает консенсусное доверие.
///          Динамическое discovery поставляет только недоверенные routing hints, а операторские endpoint'ы
///          всегда сохраняют приоритет доставки gossip.
class PeerManager {
public:
    /// \brief Создает менеджер и подхватывает явно сконфигурированные endpoint'ы runtime.
    explicit PeerManager(CybouNodeRuntime& runtime);
    /// \brief Устанавливает новое исходящее соединение с числовым IP-адресом.
    bool Connect(const std::string& numeric_address, uint16_t port);
    PeerConnectStatus LastConnectStatus() const { return m_last_connect_status; }
    /// \brief Помечает endpoint'ы как явно заданные оператором.
    /// \details При заполненной емкости такой endpoint может вытеснить только незащищенного discovery-пира.
    void SetExplicitEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints);
    /// \brief Пингует все текущие соединения и возвращает число живых после прохода.
    size_t PingAll();
    /// \brief Пингует не более `max_peers`, с ротацией стартовой позиции между вызовами.
    size_t PingSome(size_t max_peers);
    /// \brief Подтягивает до `max_blocks` финализованных блоков от выбранного пира.
    SyncPeerResult SyncFromPeer(const std::string& numeric_address, uint16_t port, uint64_t max_blocks);
    /// \brief Отправляет операцию в конкретный уже известный пир.
    OperationSubmitResult SubmitOperation(const std::string& numeric_address, uint16_t port,
        const ProtocolOperation& operation);
    /// \brief Пытается доставить операцию хотя бы в один из переданных endpoint'ов.
    PeerSubmitResult SubmitOperationToAny(
        const std::vector<std::pair<std::string, uint16_t>>& endpoints,
        const ProtocolOperation& operation);
    /// \brief Рассылает недавние финализованные блоки подключенным пирам.
    size_t FanoutRecentBlocks(size_t max_per_peer = 16);
    /// \brief Запрашивает и обрабатывает новые relay-операции у подключенных пиров.
    size_t PollOperationRelays();
    /// \brief Запрашивает Validation-attestation'ы у relay-пиров и возвращает число принятых записей.
    size_t PollValidationAttestations();
    size_t ConnectedCount() const { return m_peers.size(); }
    /// \brief Возвращает снимок известных подключенных пиров.
    std::vector<PeerInfo> Peers() const;
    /// \brief Возвращает подключенные Full Node'ы с уже доказанной storage identity.
    std::vector<PeerInfo> StorageEndpoints();
    /// \brief Передает финализованно-авторизованный зашифрованный chunk конкретному storage-пиру.
    std::optional<ChunkAdmissionResult> PutAuthorizedChunk(
        const std::string& address, uint16_t port, const StorageId& storage_id,
        const cybou::Hash256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof);
    /// \brief Запрашивает chunk по ChunkId у конкретного storage-пира.
    std::optional<std::vector<unsigned char>> GetChunkById(
        const std::string& address, uint16_t port, const StorageId& storage_id, const ChunkId& chunk_id);
    /// \brief Запрашивает proof авторизации chunk'а у конкретного storage-пира.
    std::optional<ChunkAuthorizationProof> GetChunkAuthorizationProof(
        const std::string& address, uint16_t port, const StorageId& storage_id,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id);
    /// \brief Разрывает все текущие соединения.
    void DisconnectAll();

    /// \brief Выполняет auto-discovery новых endpoint'ов через уже подключенных пиров.
    size_t DiscoverPeers(size_t max_sessions = MAX_OUTBOUND_PEERS);
    /// \brief Возвращает текущий список endpoint'ов gossip/runtime.
    std::vector<std::pair<std::string, uint16_t>> KnownEndpoints() const;

private:
    using Endpoint = std::pair<std::string, uint16_t>;
    PeerSession* FindStorageSession(const std::string& address, uint16_t port,
        const std::optional<StorageId>& storage_id, Endpoint* endpoint = nullptr);
    CybouNodeRuntime& m_runtime;
    boost::asio::io_context m_io;
    std::map<Endpoint, std::unique_ptr<PeerSession>> m_peers;
    std::optional<Endpoint> m_ping_cursor;
    std::optional<Endpoint> m_discovery_cursor;
    std::map<Endpoint, std::set<cybou::Hash256>> m_announced_blocks;
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
