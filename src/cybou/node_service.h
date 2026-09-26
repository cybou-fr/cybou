// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_NODE_SERVICE_H
#define CYBOU_NODE_SERVICE_H

#include <cybou/node_runtime.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

namespace cybou {

struct CybouNodeServiceConfig {
    NodeRuntimeConfig runtime;
    CybouState genesis;
};

/** Owns node runtime startup and the observer's periodic verified sync loop. */
class CybouNodeService final {
public:
    using ObserverUpdate = std::function<bool(const SyncPeerResult&, const NodeRuntimeStatus&)>;

    explicit CybouNodeService(CybouNodeServiceConfig config);
    ~CybouNodeService();

    CybouNodeService(const CybouNodeService&) = delete;
    CybouNodeService& operator=(const CybouNodeService&) = delete;

    /** Open or initialize the local state, rejecting corrupt and foreign state. */
    void Start();
    /** Start the single background observer sync worker. */
    void StartObserverSync(
        std::pair<std::string, uint16_t> bootstrap_peer,
        std::chrono::milliseconds interval,
        ObserverUpdate update);
    void StopObserverSync();

    CybouNodeRuntime& Runtime() { return *m_runtime; }
    const CybouNodeRuntime& Runtime() const { return *m_runtime; }

private:
    std::unique_ptr<CybouNodeRuntime> m_runtime;
    CybouState m_genesis;
    bool m_started{false};
    std::atomic_bool m_stop_sync{false};
    std::thread m_sync_thread;
};

} // namespace cybou

#endif // CYBOU_NODE_SERVICE_H
