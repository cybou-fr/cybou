// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/node_service.h>

#include <cybou/p2p/inbound_server.h>
#include <cybou/p2p/peer_manager.h>

#include <boost/asio.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace cybou {

CybouNodeService::CybouNodeService(CybouNodeServiceConfig config)
    : m_runtime{std::make_unique<CybouNodeRuntime>(std::move(config.runtime))},
      m_genesis{std::move(config.genesis)}
{
}

struct CybouNodeService::NetworkListener {
    boost::asio::io_context io;
    p2p::InboundPeerServer server;

    NetworkListener(CybouNodeRuntime& runtime, const boost::asio::ip::tcp::endpoint& endpoint)
        : server{runtime, io, endpoint}
    {
    }
};

CybouNodeService::~CybouNodeService()
{
    StopBlockProduction();
    StopNetwork();
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

void CybouNodeService::StartNetwork(
    CybouNetworkServiceConfig config,
    NetworkUpdate update)
{
    if (!m_started) throw std::logic_error("CYBOU node service must be started before network service");
    if (m_sync_thread.joinable() || m_listener_thread.joinable()) throw std::logic_error("network service is already running");
    if (config.sync_interval <= std::chrono::milliseconds::zero()) throw std::invalid_argument("network sync interval must be positive");
    if (config.sync_batch_size == 0 || config.sync_batch_size > 128) throw std::invalid_argument("invalid network sync batch size");
    m_stop_network.store(false);
    try {
        if (config.listen_endpoint) {
            const auto address = boost::asio::ip::make_address(config.listen_endpoint->first);
            if (config.listen_endpoint->second == 0) throw std::invalid_argument("network listener port must be nonzero");
            m_network_listener = std::make_unique<NetworkListener>(*m_runtime,
                boost::asio::ip::tcp::endpoint{address, config.listen_endpoint->second});
            m_listener_thread = std::thread{[this] { m_network_listener->server.Run(m_stop_network); }};
        }
        m_sync_thread = std::thread{[this, config, update = std::move(update)] {
        while (!m_stop_network.load()) {
            SyncPeerResult result;
            try {
                result = m_runtime->SyncFromConfiguredPeer(config.sync_batch_size);
            } catch (...) {
                result.status = SyncPeerStatus::PROTOCOL_ERROR;
            }

            try {
                const auto connected = m_runtime->ConnectedPeerCount();
                if (!update(result, m_runtime->GetStatus(), connected)) {
                    m_stop_network.store(true);
                }
            } catch (...) {
                // Keep the Full Node alive if a UI callback fails; protocol sync
                // remains owned by this worker and can be retried next cycle.
            }

            // Far behind the network: fetch the next batch at once instead of
            // pausing, so a fresh node catches up in minutes, not an hour.
            if (result.blocks_applied >= config.sync_batch_size) continue;

            auto remaining = config.sync_interval;
            constexpr auto SLEEP_SLICE = std::chrono::milliseconds{200};
            while (remaining > std::chrono::milliseconds::zero() && !m_stop_network.load()) {
                const auto sleep_for = std::min(remaining, SLEEP_SLICE);
                std::this_thread::sleep_for(sleep_for);
                remaining -= sleep_for;
            }
        }
        }};
        StartBlockProduction(config.block_interval_ms);
    } catch (...) {
        StopNetwork();
        throw;
    }
}

void CybouNodeService::StopNetwork()
{
    StopBlockProduction();
    m_stop_network.store(true);
    if (m_sync_thread.joinable()) m_sync_thread.join();
    if (m_listener_thread.joinable()) m_listener_thread.join();
    m_network_listener.reset();
}

void CybouNodeService::StartBlockProduction(const uint64_t block_interval_ms)
{
    if (!m_started) throw std::logic_error("CYBOU node service must be started before block production");
    if (block_interval_ms == 0 || block_interval_ms > 60000) {
        throw std::invalid_argument("invalid block production interval");
    }
    if (m_block_production_thread.joinable()) return;

    m_stop_block_production.store(false);
    m_block_production_thread = std::thread{[this, block_interval_ms] {
        auto next_block = std::chrono::steady_clock::now();
        while (!m_stop_block_production.load()) {
            const bool finalizer_enabled = m_runtime->IsPoaSignerActive();
            if (finalizer_enabled && std::chrono::steady_clock::now() >= next_block) {
                const auto block = m_runtime->ProduceBlock();
                if (!block) {
                    if (m_runtime->GetStatus().poa_safety_halted) {
                        if (auto log = m_runtime->EventLog()) log->Write(NodeEvent::poa_safety_halt);
                    }
                    m_runtime->DisablePoaSigner();
                    break;
                }
                next_block = std::chrono::steady_clock::now() + std::chrono::milliseconds{block_interval_ms};
            }

            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
    }};
}

void CybouNodeService::StopBlockProduction()
{
    m_stop_block_production.store(true);
    if (m_block_production_thread.joinable() && m_block_production_thread.get_id() != std::this_thread::get_id()) {
        m_block_production_thread.join();
    }
}

} // namespace cybou
