// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

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
    /// \brief Конфигурация базового runtime.
    NodeRuntimeConfig runtime;
    /// \brief Canonical genesis state, совпадающий с runtime.network_genesis.
    CybouState genesis;
};

/// \brief Параметры фоновой сети: verified sync, listener и локальная PoA-периодика.
struct CybouNetworkServiceConfig {
    /// \brief Пауза между sync-pass, если не было полного заполнения batch.
    /// \details Тот же такт, что у headless-узлов: PoA забирает кандидатов и рассылает новый
    ///          блок на этом такте, поэтому 3 с давали до ~6 с задержки на каждую операцию.
    std::chrono::milliseconds sync_interval{250};
    /// \brief Верхняя граница числа finalized blocks, запрашиваемых за один sync-pass.
    uint64_t sync_batch_size{64};
    /// \brief Целевой интервал локальной PoA-финализации, миллисекунды.
    uint64_t block_interval_ms{1000};
    /// \brief Optional listener endpoint для входящих CYBOU P2P connections.
    std::optional<std::pair<std::string, uint16_t>> listen_endpoint;
    /// \brief true: a busy listen port falls back to any free port; if none binds the node stays outbound-only.
    bool listen_optional{false};
    // Deployment gate: false until coordinated current-baseline acceptance.
    // Local acquisition policy only; never a wire capability or node role.
    bool observation_polling{false};
};

/// \brief Управляет запуском runtime и сетевым жизненным циклом Full Node.
class CybouNodeService final {
public:
    /// \brief Колбэк прогресса verified sync.
    /// \details Возвращаемое false просит сервис остановить сетевые потоки.
    using NetworkUpdate = std::function<bool(const SyncPeerResult&, const NodeRuntimeStatus&, size_t)>;

    /// \brief Создаёт сервис и runtime, но не запускает state initialization.
    /// \param config Конфигурация runtime и canonical genesis state.
    explicit CybouNodeService(CybouNodeServiceConfig config);
    /// \brief Останавливает block production и сетевые потоки, если они были запущены.
    ~CybouNodeService();

    CybouNodeService(const CybouNodeService&) = delete;
    CybouNodeService& operator=(const CybouNodeService&) = delete;

    /// \brief Открывает или инициализирует локальное состояние, отвергая чужую или повреждённую сеть.
    /// \pre Не вызывать одновременно из нескольких потоков.
    /// \post После успешного возврата Runtime().GetStatus().is_initialized истинно либо state уже существовал.
    /// \throws std::runtime_error При NETWORK_MISMATCH, CORRUPT или невозможности инициализировать genesis.
    void Start();
    /// \brief Запускает CYBOU P2P maintenance, verified sync и optional listener.
    /// \param config Частота sync, listener endpoint и интервал локальной PoA-финализации.
    /// \param update Колбэк, вызываемый после каждого sync-pass.
    /// \pre Start() уже был успешно вызван; сеть ещё не запущена.
    /// \post При успехе стартуют фоновые потоки sync/listener и block production.
    /// \throws std::logic_error или std::invalid_argument при неверном состоянии/параметрах.
    void StartNetwork(
        CybouNetworkServiceConfig config,
        NetworkUpdate update);
    /// \brief Останавливает сетевые фоновые потоки и listener.
    /// \post После возврата фоновые сетевые потоки не выполняются.
    void StopNetwork();
    /// \brief Запускает локальное PoA block production при доступном signer.
    /// \param block_interval_ms Целевой интервал между успешными блоками, мс.
    /// \pre Start() уже был вызван; 1 <= block_interval_ms <= 60000.
    /// \post При успехе запущен один фоновый поток block production.
    void StartBlockProduction(uint64_t block_interval_ms = 1000);
    /// \brief Останавливает локальное PoA block production.
    /// \post После возврата поток block production остановлен или не существовал.
    void StopBlockProduction();

    /// \brief Доступ к базовому runtime этого сервиса.
    /// \return Ссылка на underlying runtime; дальнейшая потокобезопасность определяется его собственным API.
    CybouNodeRuntime& Runtime() { return *m_runtime; }
    /// \brief Константный доступ к базовому runtime этого сервиса.
    /// \return Ссылка на underlying runtime.
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
