// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/block_feed.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/system_error.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#ifndef WIN32
#include <sys/time.h>
#endif

namespace cybou {
namespace {

constexpr std::array<unsigned char, 4> REQUEST_MAGIC{'C', 'Y', 'B', '1'};
constexpr size_t REQUEST_SIZE{4 + 32 + 8};

void WriteU32(std::array<unsigned char, 4>& out, const uint32_t value)
{
    for (size_t i = 0; i < out.size(); ++i) out[i] = static_cast<unsigned char>(value >> (8 * i));
}

uint32_t ReadU32(const std::array<unsigned char, 4>& in)
{
    uint32_t value{0};
    for (size_t i = 0; i < in.size(); ++i) value |= uint32_t{in[i]} << (8 * i);
    return value;
}

void SetIoTimeout(boost::asio::ip::tcp::socket& socket)
{
#ifdef WIN32
    const unsigned long timeout_ms = 5000;
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    const timeval timeout{5, 0};
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

} // namespace

bool ServeFinalizedBlockRequest(CybouStateStore& store, boost::asio::ip::tcp::socket& socket)
{
    try {
        SetIoTimeout(socket);
        std::array<unsigned char, REQUEST_SIZE> request{};
        boost::asio::read(socket, boost::asio::buffer(request));
        if (!std::equal(REQUEST_MAGIC.begin(), REQUEST_MAGIC.end(), request.begin())) return false;
        if (!std::equal(store.GetNetworkId().begin(), store.GetNetworkId().end(), request.begin() + 4)) return false;

        uint64_t height{0};
        for (size_t i = 0; i < 8; ++i) height |= uint64_t{request[36 + i]} << (8 * i);
        const auto block = store.GetBlockAtHeight(height);
        const auto bytes = block ? SerializeFinalizedBlock(*block) : std::vector<unsigned char>{};
        if (bytes.size() > MAX_FINALIZED_BLOCK_FEED_BYTES) return false;
        std::array<unsigned char, 4> length{};
        WriteU32(length, static_cast<uint32_t>(bytes.size()));
        boost::asio::write(socket, boost::asio::buffer(length));
        if (!bytes.empty()) boost::asio::write(socket, boost::asio::buffer(bytes));
        return true;
    } catch (const boost::system::system_error&) {
        return false;
    }
}

std::optional<FinalizedBlockV1> FetchFinalizedBlock(
    const std::string& host, const uint16_t port, const uint256& network_id, const uint64_t height)
{
    if (height == 0) return std::nullopt;
    try {
        boost::asio::io_context io;
        boost::asio::ip::tcp::resolver resolver(io);
        boost::asio::ip::tcp::socket socket(io);
        boost::asio::connect(socket, resolver.resolve(host, std::to_string(port)));
        SetIoTimeout(socket);
        std::array<unsigned char, REQUEST_SIZE> request{};
        std::copy(REQUEST_MAGIC.begin(), REQUEST_MAGIC.end(), request.begin());
        std::copy(network_id.begin(), network_id.end(), request.begin() + 4);
        for (size_t i = 0; i < 8; ++i) request[36 + i] = static_cast<unsigned char>(height >> (8 * i));
        boost::asio::write(socket, boost::asio::buffer(request));

        std::array<unsigned char, 4> length{};
        boost::asio::read(socket, boost::asio::buffer(length));
        const uint32_t size = ReadU32(length);
        if (size == 0 || size > MAX_FINALIZED_BLOCK_FEED_BYTES) return std::nullopt;
        std::vector<unsigned char> bytes(size);
        boost::asio::read(socket, boost::asio::buffer(bytes));
        auto block = DeserializeFinalizedBlock(bytes);
        if (!block || block->block.height != height ||
            block->certificate.network_id != network_id ||
            block->certificate.block_id != ComputeBlockId(block->block)) return std::nullopt;
        return block;
    } catch (const boost::system::system_error&) {
        return std::nullopt;
    }
}

bool SyncNextFinalizedBlock(CybouStateStore& observer, const std::string& host, const uint16_t port)
{
    const auto height = observer.GetFinalizedHeight();
    if (!height || *height == std::numeric_limits<uint64_t>::max()) return false;
    auto block = FetchFinalizedBlock(host, port, observer.GetNetworkId(), *height + 1);
    return block && static_cast<bool>(observer.CommitFinalizedBlock(*block));
}

} // namespace cybou
