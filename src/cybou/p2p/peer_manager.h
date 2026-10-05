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

/// \brief Максимум одновременных outbound-сессий: `8`.
/// \details Это локальный эксплуатационный лимит fanout/discovery, а не wire-константа протокола.
inline constexpr size_t MAX_OUTBOUND_PEERS{8};

/// \brief Итог попытки соединения с пиром.
enum class PeerConnectStatus : uint8_t {
    CONNECTED,          ///< Соединение и P2P-рукопожатие успешно завершены.
    UNAVAILABLE,        ///< Адрес недоступен, таймаут вышел или нет места для нового пира.
    HANDSHAKE_FAILED,   ///< TCP/TLS/HELLO завершились некорректно или peer не прошел локальную проверку цепочки.
    WRONG_NETWORK,      ///< `NetworkBinding` удаленной стороны не совпал с локальной официальной сетью.
    ADMISSION_REJECTED, ///< Локальная geo/admission политика запретила адрес до подключения.
    INVALID_REQUEST,    ///< Некорректный endpoint или иной локальный входной параметр.
    LOCAL_FAILURE,      ///< Локальный сбой RNG/сокета/подготовки транспорта.
};

// Common ordinary TCP/TLS/HELLO dial path, including Geo, SPKI and chain checks.
std::unique_ptr<PeerSession> DialPeer(CybouNodeRuntime& runtime, boost::asio::io_context& io,
    const std::string& address, uint16_t port, PeerConnectStatus& status);

/// \brief Наблюдаемая информация о подключенном пире.
struct PeerInfo {
    /// \brief Канонический числовой IP-адрес удаленного endpoint'а.
    std::string address;
    /// \brief TCP-порт удаленного endpoint'а.
    uint16_t port{0};
    /// \brief Последний принятый `HELLO` этого пира.
    Hello hello;
    /// \brief Доказанный `StorageId`, если для этой сессии уже выполнялся `STORAGE_PROOF`.
    std::optional<StorageId> storage_id;
    /// \brief Payout binding этого StorageId; Authorization-подпись ещё не проверена.
    std::optional<StoragePayoutBinding> payout_binding;
};

/// \brief Итог отправки операции в конкретный пир или в выбранный маршрут fanout.
struct PeerSubmitResult {
    /// \brief `OperationID` exact signed кандидат-операции.
    cybou::Hash256 op_id;
    /// \brief Hop-by-hop `OP_RESULT`, если хотя бы один пир успел ответить.
    std::optional<OperationSubmitResult> acknowledgment;
    /// \brief Endpoint, от которого пришел сохраненный ответ.
    std::optional<std::pair<std::string, uint16_t>> endpoint;
    /// \brief `true`, если peer мог принять операцию, но ответ был потерян и итог подтверждения неизвестен.
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
    /// \param runtime Runtime, который поставляет локальный статус, gossip frontier и storage/operation API.
    explicit PeerManager(CybouNodeRuntime& runtime);
    /// \brief Устанавливает новое исходящее соединение с числовым IP-адресом.
    /// \param numeric_address Числовой IPv4/IPv6 адрес.
    /// \param port TCP-порт удаленного Full Node.
    /// \return `true` только при успешном TCP/TLS/HELLO и проверке `MatchesKnownFinalizedChain()`.
    bool Connect(const std::string& numeric_address, uint16_t port);
    PeerConnectStatus LastConnectStatus() const { return m_last_connect_status; }
    /// \brief Помечает endpoint'ы как явно заданные оператором.
    /// \details При заполненной емкости такой endpoint может вытеснить только незащищенного discovery-пира.
    /// \param endpoints Список канонизируемых endpoint'ов.
    void SetExplicitEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints);
    /// \brief Пингует все текущие соединения и возвращает число живых после прохода.
    /// \return Число сессий, переживших проход health-check.
    size_t PingAll();
    /// \brief Пингует не более `max_peers`, с ротацией стартовой позиции между вызовами.
    /// \param max_peers Верхняя граница числа пиров в одном проходе.
    size_t PingSome(size_t max_peers);
    /// \brief Подтягивает до `max_blocks` финализованных блоков от выбранного пира.
    /// \param numeric_address Адрес уже подключенного пира.
    /// \param port Порт уже подключенного пира.
    /// \param max_blocks Максимум блоков в одном проходе.
    SyncPeerResult SyncFromPeer(const std::string& numeric_address, uint16_t port, uint64_t max_blocks);
    /// \brief Отправляет операцию в конкретный уже известный пир.
    OperationSubmitResult SubmitOperation(const std::string& numeric_address, uint16_t port,
        const ProtocolOperation& operation, uint64_t work_nonce);
    /// \brief Пытается доставить операцию хотя бы в один из переданных endpoint'ов.
    /// \return Первый подтвержденный hop-by-hop результат; при потере ответа выставляет `delivery_uncertain`.
    PeerSubmitResult SubmitOperationToAny(
        const std::vector<std::pair<std::string, uint16_t>>& endpoints,
        const ProtocolOperation& operation, uint64_t work_nonce);
    /// \brief Последовательно рассылает finalized history от frontier каждого пира.
    /// \param max_per_peer Верхняя граница предложений `BLOCK_ANNOUNCE` на пир за один проход.
    size_t FanoutFinalizedBlocks(size_t max_per_peer = 16);
    /// \brief Запрашивает и обрабатывает новые relay-операции у подключенных пиров.
    size_t PollOperationRelays();
    /// \brief Проталкивает relay-операции этого узла по исходящим сессиям (узлы за NAT).
    size_t PushOperationRelays();
    size_t ConnectedCount() const { return m_peers.size(); }
    /// \brief Возвращает снимок известных подключенных пиров.
    std::vector<PeerInfo> Peers() const;
    /// \brief Возвращает подключенные Full Node'ы с уже доказанной storage identity.
    /// \return Только те пиры, у которых `STORAGE_PROOF` уже успешно связал endpoint с `StorageId`.
    std::vector<PeerInfo> StorageEndpoints();
    /// \brief Разрывает все текущие соединения.
    /// \post Сессии и их gossip frontier удалены; новый HELLO восстановит frontier.
    void DisconnectAll();

    /// \brief Выполняет auto-discovery новых endpoint'ов через уже подключенных пиров.
    /// \param max_sessions Сколько live-сессий опросить в одном проходе.
    size_t DiscoverPeers(size_t max_sessions = MAX_OUTBOUND_PEERS);
    /// \brief Возвращает текущий список endpoint'ов gossip/runtime.
    std::vector<std::pair<std::string, uint16_t>> KnownEndpoints() const;

private:
    using Endpoint = std::pair<std::string, uint16_t>;
    CybouNodeRuntime& m_runtime;
    boost::asio::io_context m_io;
    std::map<Endpoint, std::unique_ptr<PeerSession>> m_peers;
    std::optional<Endpoint> m_ping_cursor;
    std::optional<Endpoint> m_discovery_cursor;
    // Latest session HELLO/BLOCK_RESULT height; only a bounded fanout scheduling hint.
    std::map<Endpoint, uint64_t> m_peer_finalized_heights;
    /// \brief Явно заданные оператором endpoint'ы в канонической форме `address:port`.
    std::set<Endpoint> m_explicit_endpoints;
    PeerConnectStatus m_last_connect_status{PeerConnectStatus::INVALID_REQUEST};
};

} // namespace cybou::p2p
#endif
