// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_BLOCK_FEED_H
#define CYBOU_BLOCK_FEED_H

#include <cybou/state_store.h>

#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace cybou {

inline constexpr uint32_t MAX_FINALIZED_BLOCK_FEED_BYTES{32U * 1024U * 1024U};

/** Serve one height request on an already accepted socket. No state mutation. */
bool ServeFinalizedBlockRequest(CybouStateStore& store, boost::asio::ip::tcp::socket& socket);

/** Fetch an untrusted finalized block. Callers must validate it via StateStore. */
std::optional<FinalizedBlockV1> FetchFinalizedBlock(
    const std::string& host, uint16_t port, const uint256& network_id, uint64_t height);

/** Fetch and atomically verify the next block against this observer's canonical state. */
bool SyncNextFinalizedBlock(
    CybouStateStore& observer, const std::string& host, uint16_t port);

} // namespace cybou

#endif // CYBOU_BLOCK_FEED_H
