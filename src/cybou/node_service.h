// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_NODE_SERVICE_H
#define CYBOU_NODE_SERVICE_H

/// \file
/// \brief Координатор жизненного цикла Full Node поверх CybouNodeRuntime.

#include <cybou/node_runtime.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace cybou {

/// \brief Конфигурация запуска node-service: runtime plus canonical genesis state.
struct CybouNodeServiceConfig {
    NodeRuntimeConfig runtime;
    CybouState genesis;
};

/// \brief Параметры фоновой сети: verified sync, listener и локальная PoA-периодика.
struct CybouNetworkServiceConfig {
    std::chrono::milliseconds sync_interval{3000};
    uint64_t sync_batch_size{64};
    uint64_t block_interval_ms{1000};
    std::optional<std::pair<std::string, uint16_t>> listen_endpoint;
};

/// \brief Управляет запуском runtime и сетевым жизненным циклом Full Node.
class CybouNodeService final {
public:
    using NetworkUpdate = std::function<bool(const SyncPeerResult&, const NodeRuntimeStatus&, size_t)>;

    explicit CybouNodeService(CybouNodeServiceConfig config);
    ~CybouNodeService();

    CybouNodeService(const CybouNodeService&) = delete;
    CybouNodeService& operator=(const CybouNodeService&) = delete;

    /// \brief Открывает или инициализирует локальное состояние, отвергая чужую или повреждённую сеть.
    void Start();
    /// \brief Запускает CYBOU P2P maintenance, verified sync и optional listener.
    void StartNetwork(
        CybouNetworkServiceConfig config,
        NetworkUpdate update);
    /// \brief Останавливает сетевые фоновые потоки и listener.
    void StopNetwork();
    /// \brief Запускает локальное PoA block production при доступном signer.
    void StartBlockProduction(uint64_t block_interval_ms = 1000);
    /// \brief Останавливает локальное PoA block production.
    void StopBlockProduction();

    /// \brief Доступ к базовому runtime этого сервиса.
    CybouNodeRuntime& Runtime() { return *m_runtime; }
    /// \brief Константный доступ к базовому runtime этого сервиса.
    const CybouNodeRuntime& Runtime() const { return *m_runtime; }

private:
    struct NetworkListener;
    std::unique_ptr<CybouNodeRuntime> m_runtime;
    std::unique_ptr<NetworkListener> m_network_listener;
    CybouState m_genesis;
    bool m_started{false};
    std::atomic_bool m_stop_network{false};
    std::thread m_sync_thread;
    std::thread m_listener_thread;
    std::atomic_bool m_stop_block_production{true};
    std::thread m_block_production_thread;
};

} // namespace cybou

#endif // CYBOU_NODE_SERVICE_H
