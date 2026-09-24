// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_PEER_MANAGER_H
#define CYBOU_P2P_PEER_MANAGER_H

#include <cybou/p2p/session.h>

#include <boost/asio/io_context.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cybou {
class CybouNodeRuntime;
}

namespace cybou::p2p {

inline constexpr size_t MAX_OUTBOUND_PEERS{8};

struct PeerInfo {
    std::string address;
    uint16_t port{0};
    Hello hello;
};

// Single-threaded outbound peer set. Callers schedule connection attempts and
// health checks; this class never supplies consensus trust or auto-discovers peers.
class PeerManager {
public:
    explicit PeerManager(CybouNodeRuntime& runtime);
    bool Connect(const std::string& numeric_address, uint16_t port);
    size_t PingAll();
    size_t ConnectedCount() const { return m_peers.size(); }
    std::vector<PeerInfo> Peers() const;
    void DisconnectAll();

private:
    using Endpoint = std::pair<std::string, uint16_t>;
    CybouNodeRuntime& m_runtime;
    boost::asio::io_context m_io;
    std::map<Endpoint, std::unique_ptr<PeerSession>> m_peers;
};

} // namespace cybou::p2p
#endif
