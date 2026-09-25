// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/inbound_server.h>

#include <cybou/node_runtime.h>
#include <cybou/p2p/session.h>

#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <thread>

namespace cybou::p2p {
namespace {

std::optional<Hello> LocalHello(const CybouNodeRuntime& runtime)
{
    const auto status = runtime.GetStatus();
    if (!status.is_initialized) return std::nullopt;
    std::array<unsigned char, 8> bytes{};
    if (RAND_bytes(bytes.data(), bytes.size()) != 1) return std::nullopt;
    uint64_t nonce{0};
    for (int i = 0; i < 8; ++i) nonce |= uint64_t{bytes[i]} << (8 * i);
    if (nonce == 0) return std::nullopt;
    return Hello{.network_id = status.network_id, .finalized_height = status.finalized_height,
        .finalized_tip = status.finalized_tip,
        .capabilities = CAP_SERVE_BLOCKS | (status.is_authority ? CAP_ACCEPT_OPERATIONS : 0),
        .nonce = nonce};
}

} // namespace

InboundPeerServer::InboundPeerServer(CybouNodeRuntime& runtime, boost::asio::io_context& io,
    const boost::asio::ip::tcp::endpoint& endpoint)
    : m_runtime{runtime}, m_acceptor{io, endpoint}
{
    m_acceptor.non_blocking(true);
}

void InboundPeerServer::Run(std::atomic_bool& stopping)
{
    while (!stopping) {
        m_workers.erase(std::remove_if(m_workers.begin(), m_workers.end(), [](const Worker& worker) {
            return worker.done->load();
        }), m_workers.end());
        boost::asio::ip::tcp::socket socket{m_acceptor.get_executor()};
        boost::system::error_code ec;
        m_acceptor.accept(socket, ec);
        if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }
        if (ec) { stopping = true; break; }
        if (m_workers.size() >= MAX_INBOUND_PEERS) {
            socket.close();
            continue;
        }
        auto done = std::make_shared<std::atomic_bool>(false);
        m_workers.push_back(Worker{done, std::jthread{[this, &stopping, done, socket = std::move(socket)]() mutable {
            PeerSession session{std::move(socket)};
            const auto hello = LocalHello(m_runtime);
            if (hello && session.Handshake(*hello)) {
                while (!stopping && session.ServeNext(m_runtime)) {}
            }
            done->store(true);
        }}});
    }
    m_workers.clear();
}

} // namespace cybou::p2p
