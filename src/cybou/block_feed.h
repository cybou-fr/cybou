// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_BLOCK_FEED_H
#define CYBOU_BLOCK_FEED_H

#include <cybou/authority_node.h>
#include <cybou/state_store.h>

#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace cybou {

inline constexpr uint32_t MAX_FINALIZED_BLOCK_FEED_BYTES{32U * 1024U * 1024U};
inline constexpr uint32_t MAX_OPERATION_PAYLOAD_BYTES{128U * 1024U};

class CybouNodeRuntime;

enum class FetchBlockStatus : uint8_t {
    OK,
    NOT_FOUND,
    NETWORK_MISMATCH,
    CONNECTION_FAILED,
    CORRUPT_BLOCK,
};

struct FetchBlockResult {
    FetchBlockStatus status{FetchBlockStatus::CONNECTION_FAILED};
    std::optional<FinalizedBlock> block{std::nullopt};

    explicit operator bool() const { return status == FetchBlockStatus::OK && block.has_value(); }
    bool has_value() const { return block.has_value(); }
    const FinalizedBlock* operator->() const { return &*block; }
    const FinalizedBlock& operator*() const { return *block; }
    const FinalizedBlock& value() const { return block.value(); }
};

enum class SyncPeerStatus : uint8_t {
    UP_TO_DATE,
    BLOCKS_APPLIED,
    CONNECTION_FAILED,
    NETWORK_MISMATCH,
    PROTOCOL_ERROR,
};

struct SyncPeerResult {
    SyncPeerStatus status{SyncPeerStatus::CONNECTION_FAILED};
    uint64_t blocks_applied{0};

    operator uint64_t() const { return blocks_applied; }
    bool IsConnected() const {
        return status == SyncPeerStatus::UP_TO_DATE || status == SyncPeerStatus::BLOCKS_APPLIED;
    }
};

/** Serve incoming TCP connection: handles block request (CYB1) or operation submission (CYBO). */
bool ServeCybouConnection(CybouNodeRuntime& runtime, boost::asio::ip::tcp::socket& socket);

/** Serve one height request on an already accepted socket. No state mutation. */
bool ServeFinalizedBlockRequest(CybouStateStore& store, boost::asio::ip::tcp::socket& socket);

/** Fetch an untrusted finalized block. Callers must validate it via StateStore. */
FetchBlockResult FetchFinalizedBlock(
    const std::string& host, uint16_t port, const uint256& network_id, uint64_t height);

/** Submit a protocol operation to a remote peer (authority / validator). Returns structured result. */
OperationSubmitResult SubmitOperationRemote(
    const std::string& host, uint16_t port, const uint256& network_id, const ProtocolOperation& op);

/** Fetch and atomically verify the next block against this observer's canonical state. */
bool SyncNextFinalizedBlock(
    CybouStateStore& observer, const std::string& host, uint16_t port);

} // namespace cybou

#endif // CYBOU_BLOCK_FEED_H
