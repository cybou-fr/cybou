// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/node_service.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace cybou {

CybouNodeService::CybouNodeService(CybouNodeServiceConfig config)
    : m_runtime{std::make_unique<CybouNodeRuntime>(std::move(config.runtime))},
      m_genesis{std::move(config.genesis)}
{
}

CybouNodeService::~CybouNodeService()
{
    StopObserverSync();
}

void CybouNodeService::Start()
{
    if (m_started) return;

    const auto status = m_runtime->GetStatus();
    if (status.runtime_state == NodeRuntimeState::NETWORK_MISMATCH) {
        throw std::runtime_error("CYBOU state belongs to another network; DEV reset requires an explicit cutover");
    }
    if (status.runtime_state == NodeRuntimeState::CORRUPT) {
        throw std::runtime_error("CYBOU state is unavailable or corrupt");
    }
    if (status.runtime_state == NodeRuntimeState::UNINITIALIZED && !m_runtime->InitializeGenesis(m_genesis)) {
        throw std::runtime_error("cannot initialize CYBOU genesis");
    }
    m_started = true;
}

void CybouNodeService::StartObserverSync(
    std::pair<std::string, uint16_t> bootstrap_peer,
    const std::chrono::milliseconds interval,
    ObserverUpdate update)
{
    if (!m_started) throw std::logic_error("CYBOU node service must be started before observer sync");
    if (m_runtime->GetStatus().is_authority) throw std::logic_error("authority runtime cannot start observer sync");
    if (m_sync_thread.joinable()) throw std::logic_error("observer sync is already running");
    if (interval <= std::chrono::milliseconds::zero()) throw std::invalid_argument("observer sync interval must be positive");

    m_stop_sync.store(false);
    m_sync_thread = std::thread{[this, peer = std::move(bootstrap_peer), interval, update = std::move(update)] {
        while (!m_stop_sync.load()) {
            SyncPeerResult result;
            try {
                result = m_runtime->HasP2pEndpoint() ? m_runtime->SyncFromConfiguredPeer(1) :
                    m_runtime->SyncFromPeer(peer.first, peer.second, 100);
            } catch (...) {
                result.status = SyncPeerStatus::PROTOCOL_ERROR;
            }

            try {
                if (!update(result, m_runtime->GetStatus())) {
                    m_stop_sync.store(true);
                }
            } catch (...) {
                // Keep an observer alive if a UI callback fails; protocol sync
                // remains owned by this worker and can be retried next cycle.
            }

            auto remaining = interval;
            constexpr auto SLEEP_SLICE = std::chrono::milliseconds{200};
            while (remaining > std::chrono::milliseconds::zero() && !m_stop_sync.load()) {
                const auto sleep_for = std::min(remaining, SLEEP_SLICE);
                std::this_thread::sleep_for(sleep_for);
                remaining -= sleep_for;
            }
        }
    }};
}

void CybouNodeService::StopObserverSync()
{
    m_stop_sync.store(true);
    if (m_sync_thread.joinable()) m_sync_thread.join();
}

} // namespace cybou
