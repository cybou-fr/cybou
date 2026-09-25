// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_INBOUND_SERVER_H
#define CYBOU_P2P_INBOUND_SERVER_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace cybou { class CybouNodeRuntime; }
namespace cybou::p2p {

inline constexpr size_t MAX_INBOUND_PEERS{8};

// Bounded DEV listener. Each peer has one worker; no peer can block accept.
class InboundPeerServer {
public:
    InboundPeerServer(CybouNodeRuntime& runtime, boost::asio::io_context& io,
        const boost::asio::ip::tcp::endpoint& endpoint);
    void Run(std::atomic_bool& stopping);
    uint16_t Port() const { return m_acceptor.local_endpoint().port(); }

private:
    struct Worker {
        std::shared_ptr<std::atomic_bool> done;
        std::jthread thread;
    };
    CybouNodeRuntime& m_runtime;
    boost::asio::ip::tcp::acceptor m_acceptor;
    std::vector<Worker> m_workers;
};

} // namespace cybou::p2p
#endif
