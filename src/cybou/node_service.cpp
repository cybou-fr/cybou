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

struct CybouNodeService::ObserverListener {
    boost::asio::io_context io;
    p2p::InboundPeerServer server;

    ObserverListener(CybouNodeRuntime& runtime, const boost::asio::ip::tcp::endpoint& endpoint)
        : server{runtime, io, endpoint}
    {
    }
};

CybouNodeService::~CybouNodeService()
{
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
    std::pair<std::string, uint16_t> fallback_peer,
    CybouNetworkServiceConfig config,
    NetworkUpdate update)
{
    if (!m_started) throw std::logic_error("CYBOU node service must be started before network service");
    if (m_runtime->GetStatus().is_authority) throw std::logic_error("authority runtime cannot start observer network service");
    if (m_sync_thread.joinable() || m_listener_thread.joinable()) throw std::logic_error("observer network service is already running");
    if (config.sync_interval <= std::chrono::milliseconds::zero()) throw std::invalid_argument("network sync interval must be positive");
    if (config.sync_batch_size == 0 || config.sync_batch_size > 128) throw std::invalid_argument("invalid network sync batch size");
    if (config.listen_endpoint && !m_runtime->HasP2pEndpoint()) {
        throw std::invalid_argument("inbound CYP2 requires a configured outbound CYP2 peer");
    }
    if (fallback_peer.second == 0) throw std::invalid_argument("fallback peer port must be nonzero");

    m_stop_network.store(false);
    try {
        if (config.listen_endpoint) {
            const auto address = boost::asio::ip::make_address(config.listen_endpoint->first);
            if (config.listen_endpoint->second == 0) throw std::invalid_argument("network listener port must be nonzero");
            m_observer_listener = std::make_unique<ObserverListener>(*m_runtime,
                boost::asio::ip::tcp::endpoint{address, config.listen_endpoint->second});
            m_listener_thread = std::thread{[this] { m_observer_listener->server.Run(m_stop_network); }};
        }
        m_sync_thread = std::thread{[this, peer = std::move(fallback_peer), config, update = std::move(update)] {
        while (!m_stop_network.load()) {
            SyncPeerResult result;
            try {
                result = m_runtime->HasP2pEndpoint() ? m_runtime->SyncFromConfiguredPeer(config.sync_batch_size) :
                    m_runtime->SyncFromPeer(peer.first, peer.second, 100);
            } catch (...) {
                result.status = SyncPeerStatus::PROTOCOL_ERROR;
            }

            try {
                const auto connected = m_runtime->HasP2pEndpoint() ? m_runtime->ConnectedPeerCount() :
                    static_cast<size_t>(result.IsConnected() ? 1 : 0);
                if (!update(result, m_runtime->GetStatus(), connected)) {
                    m_stop_network.store(true);
                }
            } catch (...) {
                // Keep an observer alive if a UI callback fails; protocol sync
                // remains owned by this worker and can be retried next cycle.
            }

            auto remaining = config.sync_interval;
            constexpr auto SLEEP_SLICE = std::chrono::milliseconds{200};
            while (remaining > std::chrono::milliseconds::zero() && !m_stop_network.load()) {
                const auto sleep_for = std::min(remaining, SLEEP_SLICE);
                std::this_thread::sleep_for(sleep_for);
                remaining -= sleep_for;
            }
        }
        }};
    } catch (...) {
        StopNetwork();
        throw;
    }
}

void CybouNodeService::StopNetwork()
{
    m_stop_network.store(true);
    if (m_sync_thread.joinable()) m_sync_thread.join();
    if (m_listener_thread.joinable()) m_listener_thread.join();
    m_observer_listener.reset();
}

int CybouNodeService::RunAuthority(const CybouAuthorityServiceConfig& config, std::atomic_bool& stopping)
{
    if (!m_started) throw std::logic_error("CYBOU node service must be started before authority service");
    if (!m_runtime->GetStatus().is_authority) throw std::logic_error("authority service requires a PoA finalizer runtime");
    if (config.block_feed_port == 0 || config.block_interval_ms == 0 || config.block_interval_ms > 60000) {
        throw std::invalid_argument("invalid authority listener or block interval");
    }

    const auto bind_address = boost::asio::ip::make_address(config.bind_address);
    if (config.p2p_port && *config.p2p_port == config.block_feed_port) {
        throw std::invalid_argument("P2P port must differ from block feed port");
    }
    if (config.p2p_port) {
        for (const auto& [address, peer_port] : config.peers) {
            if (address == bind_address.to_string() && peer_port == *config.p2p_port) {
                throw std::runtime_error("P2P peer list contains this listener");
            }
        }
    }

    boost::asio::io_context io;
    std::optional<p2p::InboundPeerServer> p2p_server;
    if (config.p2p_port) {
        p2p_server.emplace(*m_runtime, io,
            boost::asio::ip::tcp::endpoint{bind_address, *config.p2p_port});
    }

    boost::asio::ip::tcp::acceptor acceptor(io, {
        bind_address,
        config.block_feed_port,
    });
    acceptor.non_blocking(true);

    std::jthread block_worker;
    std::optional<std::jthread> p2p_listener;
    std::optional<std::jthread> gossip_worker;
    struct StopWorkersOnExit {
        std::atomic_bool& stopping;
        ~StopWorkersOnExit() { stopping = true; }
    } stop_workers_on_exit{stopping};
    block_worker = std::jthread{[&] {
        while (!stopping) {
            const auto block = m_runtime->ProduceBlock();
            if (!block) {
                std::cerr << "PoA block production stopped\n";
                stopping = true;
                break;
            }
            std::cout << "height=" << block->block.height << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds{config.block_interval_ms});
        }
    }};

    if (p2p_server) p2p_listener.emplace([&] { p2p_server->Run(stopping); });

    if (!config.peers.empty()) gossip_worker.emplace([&] {
        m_runtime->SetExplicitPeerEndpoints(config.peers);
        p2p::PeerManager peers{*m_runtime};
        std::map<std::pair<std::string, uint16_t>, std::chrono::steady_clock::time_point> retry_after;
        const auto reconnect_interval_str = std::getenv("CYBOU_RECONNECT_INTERVAL_MS");
        const int reconnect_interval_ms = reconnect_interval_str ? std::max(0, std::atoi(reconnect_interval_str)) : 0;
        auto last_reconnect = std::chrono::steady_clock::now();
        while (!stopping) {
            if (reconnect_interval_ms > 0 &&
                std::chrono::steady_clock::now() - last_reconnect >= std::chrono::milliseconds{reconnect_interval_ms}) {
                peers.DisconnectAll();
                last_reconnect = std::chrono::steady_clock::now();
            }
            peers.DiscoverPeers();
            const auto gossip_targets = m_runtime->GetPeerEndpointsForGossip();
            for (const auto& [host, peer_port] : gossip_targets) {
                if (stopping) break;
                const auto connected = peers.Peers();
                const bool present = std::any_of(connected.begin(), connected.end(), [&](const auto& peer) {
                    return peer.address == host && peer.port == peer_port;
                });
                const auto endpoint = std::make_pair(host, peer_port);
                if (!present && std::chrono::steady_clock::now() >= retry_after[endpoint]) {
                    if (!peers.Connect(host, peer_port)) {
                        retry_after[endpoint] = std::chrono::steady_clock::now() + std::chrono::seconds{5};
                    }
                }
            }
            if (!stopping) {
                peers.FanoutRecentBlocks();
                peers.PingAll();
            }

            if (!stopping) {
                const auto sync_status = m_runtime->GetStatus();
                if (sync_status.is_initialized) {
                    const auto connected = peers.Peers();
                    const auto ahead_it = std::max_element(connected.begin(), connected.end(),
                        [](const p2p::PeerInfo& a, const p2p::PeerInfo& b) {
                            return a.hello.finalized_height < b.hello.finalized_height;
                        });
                    if (ahead_it != connected.end() &&
                        ahead_it->hello.finalized_height > sync_status.finalized_height + 1) {
                        peers.SyncFromPeer(ahead_it->address, ahead_it->port, 64);
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds{250});
        }
    });

    auto last_storage_gc = std::chrono::steady_clock::now();
    while (!stopping) {
        const auto now = std::chrono::steady_clock::now();
        if (now - last_storage_gc >= std::chrono::minutes{1}) {
            m_runtime->GarbageCollectStorageStaging();
            last_storage_gc = now;
        }
        boost::asio::ip::tcp::socket socket(io);
        boost::system::error_code ec;
        acceptor.accept(socket, ec);
        if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
            std::this_thread::sleep_for(std::chrono::milliseconds{25});
            continue;
        }
        if (ec) {
            stopping = true;
            throw boost::system::system_error(ec);
        }
        ServeCybouConnection(*m_runtime, socket);
    }
    return 0;
}

} // namespace cybou
