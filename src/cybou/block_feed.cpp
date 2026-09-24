// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/block_feed.h>
#include <cybou/node_runtime.h>

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
constexpr std::array<unsigned char, 4> OP_MAGIC{'C', 'Y', 'B', 'O'};
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

bool ServeCybouConnection(CybouNodeRuntime& runtime, boost::asio::ip::tcp::socket& socket)
{
    try {
        SetIoTimeout(socket);
        std::array<unsigned char, 4> magic{};
        boost::asio::read(socket, boost::asio::buffer(magic));

        if (std::equal(REQUEST_MAGIC.begin(), REQUEST_MAGIC.end(), magic.begin())) {
            std::array<unsigned char, 32 + 8> req_rest{};
            boost::asio::read(socket, boost::asio::buffer(req_rest));
            if (!std::equal(runtime.GetNetworkId().begin(), runtime.GetNetworkId().end(), req_rest.begin())) {
                std::array<unsigned char, 4> mismatch_len{};
                WriteU32(mismatch_len, 0xFFFFFFFF);
                boost::asio::write(socket, boost::asio::buffer(mismatch_len));
                return false;
            }
            uint64_t height{0};
            for (size_t i = 0; i < 8; ++i) height |= uint64_t{req_rest[32 + i]} << (8 * i);
            const auto block = runtime.GetBlockAtHeight(height);
            const auto bytes = block ? SerializeFinalizedBlock(*block) : std::vector<unsigned char>{};
            if (bytes.size() > MAX_FINALIZED_BLOCK_FEED_BYTES) return false;
            std::array<unsigned char, 4> length{};
            WriteU32(length, static_cast<uint32_t>(bytes.size()));
            boost::asio::write(socket, boost::asio::buffer(length));
            if (!bytes.empty()) boost::asio::write(socket, boost::asio::buffer(bytes));
            return true;
        }

        if (std::equal(OP_MAGIC.begin(), OP_MAGIC.end(), magic.begin())) {
            std::array<unsigned char, 32 + 4> req_rest{};
            boost::asio::read(socket, boost::asio::buffer(req_rest));
            if (!std::equal(runtime.GetNetworkId().begin(), runtime.GetNetworkId().end(), req_rest.begin())) {
                std::array<unsigned char, 33> resp{};
                resp[0] = static_cast<unsigned char>(OperationSubmitStatus::NETWORK_MISMATCH);
                boost::asio::write(socket, boost::asio::buffer(resp));
                return false;
            }
            std::array<unsigned char, 4> len_bytes{};
            std::copy_n(req_rest.begin() + 32, 4, len_bytes.begin());
            const uint32_t op_size = ReadU32(len_bytes);
            if (op_size == 0 || op_size > MAX_OPERATION_PAYLOAD_BYTES) {
                std::array<unsigned char, 33> resp{};
                resp[0] = static_cast<unsigned char>(OperationSubmitStatus::INVALID_PAYLOAD);
                boost::asio::write(socket, boost::asio::buffer(resp));
                return false;
            }

            std::vector<unsigned char> op_bytes(op_size);
            boost::asio::read(socket, boost::asio::buffer(op_bytes));
            const auto op = DeserializeProtocolOperation(op_bytes);
            if (!op.has_value()) {
                std::array<unsigned char, 33> resp{};
                resp[0] = static_cast<unsigned char>(OperationSubmitStatus::INVALID_PAYLOAD);
                boost::asio::write(socket, boost::asio::buffer(resp));
                return false;
            }
            const auto sub_res = runtime.SubmitOperation(*op);
            std::array<unsigned char, 33> resp{};
            resp[0] = static_cast<unsigned char>(sub_res.status);
            std::copy(sub_res.op_id.begin(), sub_res.op_id.end(), resp.begin() + 1);
            boost::asio::write(socket, boost::asio::buffer(resp));
            return static_cast<bool>(sub_res);
        }

        return false;
    } catch (const boost::system::system_error&) {
        return false;
    }
}

bool ServeFinalizedBlockRequest(CybouStateStore& store, boost::asio::ip::tcp::socket& socket)
{
    try {
        SetIoTimeout(socket);
        std::array<unsigned char, REQUEST_SIZE> request{};
        boost::asio::read(socket, boost::asio::buffer(request));
        if (!std::equal(REQUEST_MAGIC.begin(), REQUEST_MAGIC.end(), request.begin())) return false;
        if (!std::equal(store.GetNetworkId().begin(), store.GetNetworkId().end(), request.begin() + 4)) {
            std::array<unsigned char, 4> mismatch_len{};
            WriteU32(mismatch_len, 0xFFFFFFFF);
            boost::asio::write(socket, boost::asio::buffer(mismatch_len));
            return false;
        }

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

OperationSubmitResult SubmitOperationRemote(
    const std::string& host, const uint16_t port, const uint256& network_id, const ProtocolOperationV1& op)
{
    const auto op_id = ComputeOperationId(op);
    try {
        const auto op_bytes = SerializeProtocolOperation(op);
        if (op_bytes.empty() || op_bytes.size() > MAX_OPERATION_PAYLOAD_BYTES) {
            return OperationSubmitResult{.status = OperationSubmitStatus::INVALID_PAYLOAD, .op_id = op_id};
        }

        boost::asio::io_context io;
        boost::asio::ip::tcp::resolver resolver(io);
        boost::asio::ip::tcp::socket socket(io);
        boost::asio::connect(socket, resolver.resolve(host, std::to_string(port)));
        SetIoTimeout(socket);

        std::vector<unsigned char> msg;
        msg.reserve(4 + 32 + 4 + op_bytes.size());
        msg.insert(msg.end(), OP_MAGIC.begin(), OP_MAGIC.end());
        msg.insert(msg.end(), network_id.begin(), network_id.end());
        std::array<unsigned char, 4> len_bytes{};
        WriteU32(len_bytes, static_cast<uint32_t>(op_bytes.size()));
        msg.insert(msg.end(), len_bytes.begin(), len_bytes.end());
        msg.insert(msg.end(), op_bytes.begin(), op_bytes.end());

        boost::asio::write(socket, boost::asio::buffer(msg));

        std::array<unsigned char, 33> reply{};
        boost::system::error_code ec;
        boost::asio::read(socket, boost::asio::buffer(reply), ec);
        if (ec) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }

        const auto status = static_cast<OperationSubmitStatus>(reply[0]);
        uint256 confirmed_op_id;
        std::copy_n(reply.begin() + 1, 32, confirmed_op_id.begin());
        if (status == OperationSubmitStatus::ACCEPTED && confirmed_op_id != op_id) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = confirmed_op_id};
        }

        return OperationSubmitResult{
            .status = status,
            .op_id = confirmed_op_id,
        };
    } catch (const boost::system::system_error&) {
        return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
    }
}

FetchBlockResult FetchFinalizedBlock(
    const std::string& host, const uint16_t port, const uint256& network_id, const uint64_t height)
{
    if (height == 0) {
        return FetchBlockResult{.status = FetchBlockStatus::NOT_FOUND, .block = std::nullopt};
    }
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
        if (size == 0xFFFFFFFF) {
            return FetchBlockResult{.status = FetchBlockStatus::NETWORK_MISMATCH, .block = std::nullopt};
        }
        if (size == 0) {
            return FetchBlockResult{.status = FetchBlockStatus::NOT_FOUND, .block = std::nullopt};
        }
        if (size > MAX_FINALIZED_BLOCK_FEED_BYTES) {
            return FetchBlockResult{.status = FetchBlockStatus::CORRUPT_BLOCK, .block = std::nullopt};
        }
        std::vector<unsigned char> bytes(size);
        boost::asio::read(socket, boost::asio::buffer(bytes));
        auto block = DeserializeFinalizedBlock(bytes);
        if (!block || block->block.height != height ||
            block->certificate.block_id != ComputeBlockId(block->block)) {
            return FetchBlockResult{.status = FetchBlockStatus::CORRUPT_BLOCK, .block = std::nullopt};
        }
        if (block->certificate.network_id != network_id) {
            return FetchBlockResult{.status = FetchBlockStatus::NETWORK_MISMATCH, .block = std::nullopt};
        }
        return FetchBlockResult{.status = FetchBlockStatus::OK, .block = std::move(block)};
    } catch (const boost::system::system_error&) {
        return FetchBlockResult{.status = FetchBlockStatus::CONNECTION_FAILED, .block = std::nullopt};
    }
}

bool SyncNextFinalizedBlock(CybouStateStore& observer, const std::string& host, const uint16_t port)
{
    const auto height = observer.GetFinalizedHeight();
    if (!height || *height == std::numeric_limits<uint64_t>::max()) return false;
    auto res = FetchFinalizedBlock(host, port, observer.GetNetworkId(), *height + 1);
    return res && static_cast<bool>(observer.CommitFinalizedBlock(*res.block));
}

} // namespace cybou
