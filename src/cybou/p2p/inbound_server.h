// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Русский публичный API ограниченного inbound-сервера CYBOU P2P.

#ifndef CYBOU_P2P_INBOUND_SERVER_H
#define CYBOU_P2P_INBOUND_SERVER_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace cybou { class CybouNodeRuntime; }
namespace cybou::p2p {

/// \brief Максимум одновременных inbound worker'ов: `8`.
/// \details Это локальный эксплуатационный лимит, симметричный outbound-менеджеру, а не сетевой протокольный предел.
inline constexpr size_t MAX_INBOUND_PEERS{8};

/// \brief Ограниченный inbound-listener DEV-узла.
/// \details У каждого пира свой worker; один зависший пир не блокирует accept следующего.
class InboundPeerServer {
public:
    /// \brief Создает listener на указанной конечной точке.
    /// \param runtime Runtime, который будет обслуживать рукопожатие, relay и storage-запросы.
    /// \param io Внешний `io_context`, владеющий acceptor'ом.
    /// \param endpoint Локальный `IP:port` для bind/listen.
    InboundPeerServer(CybouNodeRuntime& runtime, boost::asio::io_context& io,
        const boost::asio::ip::tcp::endpoint& endpoint);
    /// \brief Запускает цикл приема новых TCP-подключений до установки флага остановки.
    /// \param stopping Общий флаг останова процесса/сервиса.
    /// \post При выходе все worker'ы присоединены и освобождены.
    void Run(std::atomic_bool& stopping);
    /// \brief Возвращает фактический локальный порт acceptor'а.
    uint16_t Port() const { return m_acceptor.local_endpoint().port(); }

private:
    struct Worker {
        /// \brief Флаг завершения конкретного worker'а для ленивой уборки без join storm на accept-пути.
        std::shared_ptr<std::atomic_bool> done;
        /// \brief Поток, который целиком обслуживает одну входящую P2P-сессию.
        std::jthread thread;
    };
    CybouNodeRuntime& m_runtime;
    boost::asio::ip::tcp::acceptor m_acceptor;
    std::vector<Worker> m_workers;
};

} // namespace cybou::p2p
#endif
