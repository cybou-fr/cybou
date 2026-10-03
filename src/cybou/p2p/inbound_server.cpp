// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Реализация ограниченного inbound-listener'а CYBOU P2P.

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

std::optional<uint64_t> RandomNonce()
{
    std::array<unsigned char, 8> bytes{};
    if (RAND_bytes(bytes.data(), bytes.size()) != 1) return std::nullopt;
    uint64_t nonce{0};
    for (int i = 0; i < 8; ++i) nonce |= uint64_t{bytes[i]} << (8 * i);
    return nonce == 0 ? std::nullopt : std::optional<uint64_t>{nonce};
}

std::optional<Hello> LocalHello(const CybouNodeRuntime& runtime)
{
    const auto status = runtime.GetStatus();
    if (!status.is_initialized) return std::nullopt;
    const auto nonce = RandomNonce();
    if (!nonce) return std::nullopt;
    return Hello{.network_binding = status.network_binding, .finalized_height = status.finalized_height,
        .finalized_tip = status.finalized_tip,
        .nonce = *nonce};
}

} // namespace

InboundPeerServer::InboundPeerServer(CybouNodeRuntime& runtime, boost::asio::io_context& io,
    const boost::asio::ip::tcp::endpoint& endpoint)
    : m_runtime{runtime}, m_acceptor{io, endpoint}
{
    m_acceptor.non_blocking(true);
    m_workers.reserve(MAX_INBOUND_PEERS);
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
        const auto remote = socket.remote_endpoint(ec);
        if (ec || !m_runtime.AdmitPeerAddress(remote.address().to_string()) ||
            !m_runtime.AdmitIngress(remote.address().to_string(), IngressBudget::Work::CONNECTION)) {
            socket.close(); continue;
        }
        if (m_workers.size() >= MAX_INBOUND_PEERS) {
            socket.close();
            continue;
        }
        auto done = std::make_shared<std::atomic_bool>(false);
        m_workers.push_back(Worker{done, std::jthread{[this, &stopping, done, socket = std::move(socket)]() mutable {
            TlsSessionConfig tls;
            if (const auto& identity = m_runtime.GetTlsServerIdentity()) {
                tls.certificate_chain_file = identity->certificate_chain_file;
                tls.private_key_file = identity->private_key_file;
            }
            PeerSession session{std::move(socket), TransportRole::SERVER, std::move(tls)};
            const auto hello = LocalHello(m_runtime);
            if (hello && session.Handshake(*hello) &&
                MatchesKnownFinalizedChain(m_runtime, *session.Peer())) {
                while (!stopping && session.ServeNext(m_runtime)) {}
            }
            done->store(true);
        }}});
    }
    m_workers.clear();
}

} // namespace cybou::p2p
