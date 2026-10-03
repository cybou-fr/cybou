// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_NODE_SERVICE_H
#define CYBOU_NODE_SERVICE_H

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

struct CybouNodeServiceConfig {
    NodeRuntimeConfig runtime;
    CybouState genesis;
};

struct CybouNetworkServiceConfig {
    std::chrono::milliseconds sync_interval{3000};
    uint64_t sync_batch_size{64};
    uint64_t block_interval_ms{1000};
    std::optional<std::pair<std::string, uint16_t>> listen_endpoint;
};

/** Owns node runtime startup and the Full Node network lifecycle. */
class CybouNodeService final {
public:
    using NetworkUpdate = std::function<bool(const SyncPeerResult&, const NodeRuntimeStatus&, size_t)>;

    explicit CybouNodeService(CybouNodeServiceConfig config);
    ~CybouNodeService();

    CybouNodeService(const CybouNodeService&) = delete;
    CybouNodeService& operator=(const CybouNodeService&) = delete;

    /** Open or initialize the local state, rejecting corrupt and foreign state. */
    void Start();
    /** Start CYP2 peer maintenance and verified sync, optionally before any peer is known. */
    void StartNetwork(
        CybouNetworkServiceConfig config,
        NetworkUpdate update);
    void StopNetwork();
    /** Start or stop local PoA block production as the vault unlocks/locks. */
    void StartBlockProduction(uint64_t block_interval_ms = 1000);
    void StopBlockProduction();

    CybouNodeRuntime& Runtime() { return *m_runtime; }
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
