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

struct CybouFinalizerServiceConfig {
    std::string bind_address;
    uint16_t block_feed_port{0};
    std::optional<uint16_t> p2p_port;
    uint64_t block_interval_ms{1000};
    std::vector<std::pair<std::string, uint16_t>> peers;
};

struct CybouNetworkServiceConfig {
    std::chrono::milliseconds sync_interval{3000};
    uint64_t sync_batch_size{64};
    std::optional<std::pair<std::string, uint16_t>> listen_endpoint;
};

/** Owns node runtime startup and the observer's network lifecycle. */
class CybouNodeService final {
public:
    using NetworkUpdate = std::function<bool(const SyncPeerResult&, const NodeRuntimeStatus&, size_t)>;

    explicit CybouNodeService(CybouNodeServiceConfig config);
    ~CybouNodeService();

    CybouNodeService(const CybouNodeService&) = delete;
    CybouNodeService& operator=(const CybouNodeService&) = delete;

    /** Open or initialize the local state, rejecting corrupt and foreign state. */
    void Start();
    /** Start shared observer P2P maintenance, verified sync, and optional inbound CYP2. */
    void StartNetwork(
        std::pair<std::string, uint16_t> fallback_peer,
        CybouNetworkServiceConfig config,
        NetworkUpdate update);
    void StopNetwork();
    /** Run the finalizer block-feed, consensus, inbound CYP2, and gossip loops. */
    int RunFinalizer(const CybouFinalizerServiceConfig& config, std::atomic_bool& stopping);

    CybouNodeRuntime& Runtime() { return *m_runtime; }
    const CybouNodeRuntime& Runtime() const { return *m_runtime; }

private:
    struct ObserverListener;
    std::unique_ptr<CybouNodeRuntime> m_runtime;
    std::unique_ptr<ObserverListener> m_observer_listener;
    CybouState m_genesis;
    bool m_started{false};
    std::atomic_bool m_stop_network{false};
    std::thread m_sync_thread;
    std::thread m_listener_thread;
};

} // namespace cybou

#endif // CYBOU_NODE_SERVICE_H
