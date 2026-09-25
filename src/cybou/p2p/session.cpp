// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/session.h>
#include <cybou/block_feed.h>
#include <cybou/node_runtime.h>

#include <algorithm>
#include <array>
#include <thread>

namespace cybou::p2p {
namespace {
constexpr size_t HEADER_SIZE{10};
constexpr size_t HELLO_SIZE{88};
constexpr auto BLOCK_TRANSFER_TIMEOUT{std::chrono::seconds{30}};

void Put64(std::vector<unsigned char>& out, uint64_t value)
{
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint64_t Read64(const unsigned char* data)
{
    uint64_t value{0};
    for (int i = 0; i < 8; ++i) value |= uint64_t{data[i]} << (8 * i);
    return value;
}

void Put32(std::vector<unsigned char>& out, uint32_t value)
{
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint32_t Read32(const unsigned char* data)
{
    uint32_t value{0};
    for (int i = 0; i < 4; ++i) value |= uint32_t{data[i]} << (8 * i);
    return value;
}
} // namespace

std::optional<std::vector<unsigned char>> EncodeFrame(const Frame& frame)
{
    if (frame.payload.size() > MAX_FRAME_PAYLOAD ||
        (static_cast<uint8_t>(frame.type) < 1 || static_cast<uint8_t>(frame.type) > 9)) return std::nullopt;
    std::vector<unsigned char> bytes{'C', 'Y', 'P', '2', WIRE_VERSION, static_cast<unsigned char>(frame.type)};
    const auto size = static_cast<uint32_t>(frame.payload.size());
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<unsigned char>(size >> (8 * i)));
    bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
    return bytes;
}

std::optional<Frame> DecodeFrame(std::span<const unsigned char> bytes)
{
    if (bytes.size() < HEADER_SIZE || !std::equal(bytes.begin(), bytes.begin() + 4, "CYP2") ||
        bytes[4] != WIRE_VERSION || bytes[5] < 1 || bytes[5] > 9) return std::nullopt;
    uint32_t size{0};
    for (int i = 0; i < 4; ++i) size |= uint32_t{bytes[6 + i]} << (8 * i);
    if (size > MAX_FRAME_PAYLOAD || bytes.size() != HEADER_SIZE + size) return std::nullopt;
    return Frame{static_cast<MessageType>(bytes[5]),
        std::vector<unsigned char>{bytes.begin() + HEADER_SIZE, bytes.end()}};
}

std::vector<unsigned char> EncodeHello(const Hello& hello)
{
    std::vector<unsigned char> out;
    out.reserve(HELLO_SIZE);
    out.insert(out.end(), hello.network_id.begin(), hello.network_id.end());
    Put64(out, hello.finalized_height);
    out.insert(out.end(), hello.finalized_tip.begin(), hello.finalized_tip.end());
    Put64(out, hello.capabilities);
    Put64(out, hello.nonce);
    return out;
}

std::optional<Hello> DecodeHello(std::span<const unsigned char> bytes)
{
    if (bytes.size() != HELLO_SIZE) return std::nullopt;
    Hello hello;
    std::copy_n(bytes.begin(), 32, hello.network_id.begin());
    hello.finalized_height = Read64(bytes.data() + 32);
    std::copy_n(bytes.begin() + 40, 32, hello.finalized_tip.begin());
    hello.capabilities = Read64(bytes.data() + 72);
    hello.nonce = Read64(bytes.data() + 80);
    if (hello.network_id.IsNull() || hello.nonce == 0) return std::nullopt;
    return hello;
}

PeerSession::PeerSession(boost::asio::ip::tcp::socket socket) : m_socket{std::move(socket)}
{
    boost::system::error_code ec;
    m_socket.non_blocking(true, ec);
    if (ec) m_socket.close();
}

bool PeerSession::ReadExact(unsigned char* out, size_t length, std::chrono::steady_clock::time_point deadline)
{
    size_t done{0};
    while (done < length && std::chrono::steady_clock::now() < deadline) {
        boost::system::error_code ec;
        const auto count = m_socket.read_some(boost::asio::buffer(out + done, length - done), ec);
        if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (ec || count == 0) return false;
        done += count;
    }
    return done == length;
}

bool PeerSession::WriteExact(const unsigned char* bytes, size_t length,
    std::chrono::steady_clock::time_point deadline)
{
    size_t done{0};
    while (done < length && std::chrono::steady_clock::now() < deadline) {
        boost::system::error_code ec;
        const auto count = m_socket.write_some(boost::asio::buffer(bytes + done, length - done), ec);
        if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (ec || count == 0) return false;
        done += count;
    }
    return done == length;
}

bool PeerSession::Write(const Frame& frame)
{
    return Write(frame, std::chrono::steady_clock::now() + std::chrono::seconds(5));
}

bool PeerSession::Write(const Frame& frame, std::chrono::steady_clock::time_point deadline)
{
    const auto bytes = EncodeFrame(frame);
    if (!bytes) return false;
    return WriteExact(bytes->data(), bytes->size(), deadline);
}

std::optional<Frame> PeerSession::Read(std::chrono::steady_clock::time_point deadline)
{
    std::array<unsigned char, HEADER_SIZE> header{};
    if (!ReadExact(header.data(), header.size(), deadline)) return std::nullopt;
    uint32_t size{0};
    for (int i = 0; i < 4; ++i) size |= uint32_t{header[6 + i]} << (8 * i);
    if (size > MAX_FRAME_PAYLOAD) return std::nullopt;
    std::vector<unsigned char> bytes{header.begin(), header.end()};
    bytes.resize(HEADER_SIZE + size);
    if (size && !ReadExact(bytes.data() + HEADER_SIZE, size, deadline)) {
        return std::nullopt;
    }
    return DecodeFrame(bytes);
}

std::optional<Frame> PeerSession::Read()
{
    return Read(std::chrono::steady_clock::now() + std::chrono::seconds(5));
}

bool PeerSession::Handshake(const Hello& local)
{
    if (local.network_id.IsNull() || local.nonce == 0 ||
        !Write(Frame{MessageType::HELLO, EncodeHello(local)})) return false;
    const auto frame = Read();
    if (!frame || frame->type != MessageType::HELLO) return false;
    const auto peer = DecodeHello(frame->payload);
    if (!peer || peer->network_id != local.network_id || peer->nonce == local.nonce) return false;
    m_peer = *peer;
    m_local_capabilities = local.capabilities;
    return true;
}

bool PeerSession::Ping(uint64_t nonce)
{
    if (!m_peer || nonce == 0) return false;
    std::vector<unsigned char> payload;
    Put64(payload, nonce);
    if (!Write(Frame{MessageType::PING, payload})) return false;
    const auto response = Read();
    return response && response->type == MessageType::PONG && response->payload == payload;
}

bool PeerSession::AnswerPing()
{
    if (!m_peer) return false;
    const auto request = Read();
    return request && request->type == MessageType::PING && request->payload.size() == 8 &&
        Write(Frame{MessageType::PONG, request->payload});
}

std::optional<std::vector<unsigned char>> PeerSession::RequestBlock(uint64_t height)
{
    if (!m_peer || !(m_peer->capabilities & CAP_SERVE_BLOCKS) || height == 0) return std::nullopt;
    std::vector<unsigned char> request;
    Put64(request, height);
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::GET_BLOCK, request}, deadline)) return std::nullopt;
    const auto meta = Read(deadline);
    if (!meta || meta->type != MessageType::BLOCK_META || meta->payload.size() != 4) return std::nullopt;
    const uint32_t size = Read32(meta->payload.data());
    if (size > MAX_FINALIZED_BLOCK_FEED_BYTES) return std::nullopt;
    std::vector<unsigned char> bytes;
    bytes.reserve(size);
    while (bytes.size() < size) {
        const auto chunk = Read(deadline);
        if (!chunk || chunk->type != MessageType::BLOCK_CHUNK || chunk->payload.empty() ||
            chunk->payload.size() > size - bytes.size()) return std::nullopt;
        bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
    }
    return bytes;
}

std::optional<OperationSubmitResult> PeerSession::SubmitOperation(const ProtocolOperation& operation)
{
    if (!m_peer || !(m_peer->capabilities & CAP_ACCEPT_OPERATIONS)) return std::nullopt;
    const auto bytes = SerializeProtocolOperation(operation);
    const auto op_id = ComputeOperationId(operation);
    if (!bytes || !op_id || bytes->empty() || bytes->size() > MAX_OPERATION_PAYLOAD_BYTES) return std::nullopt;
    std::vector<unsigned char> meta;
    Put32(meta, static_cast<uint32_t>(bytes->size()));
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::OP_META, meta}, deadline)) return std::nullopt;
    for (size_t offset = 0; offset < bytes->size(); offset += MAX_FRAME_PAYLOAD) {
        const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, bytes->size() - offset);
        if (!Write(Frame{MessageType::OP_CHUNK,
            std::vector<unsigned char>{bytes->begin() + offset, bytes->begin() + offset + count}}, deadline)) return std::nullopt;
    }
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::OP_RESULT || response->payload.size() != 33 ||
        response->payload[0] > static_cast<uint8_t>(OperationSubmitStatus::NETWORK_MISMATCH) ||
        !std::equal(op_id->begin(), op_id->end(), response->payload.begin() + 1)) return std::nullopt;
    return OperationSubmitResult{.status = static_cast<OperationSubmitStatus>(response->payload[0]),
        .op_id = *op_id};
}

bool PeerSession::ServeNext(CybouNodeRuntime& runtime)
{
    if (!m_peer) return false;
    const auto request = Read();
    if (!request) return false;
    if (request->type == MessageType::PING) {
        return request->payload.size() == 8 && Write(Frame{MessageType::PONG, request->payload});
    }
    if (request->type == MessageType::OP_META) {
        if (!(m_local_capabilities & CAP_ACCEPT_OPERATIONS)) return false;
        if (request->payload.size() != 4) return false;
        const uint32_t size = Read32(request->payload.data());
        if (size == 0 || size > MAX_OPERATION_PAYLOAD_BYTES) return false;
        const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        std::vector<unsigned char> bytes;
        bytes.reserve(size);
        while (bytes.size() < size) {
            const auto chunk = Read(deadline);
            if (!chunk || chunk->type != MessageType::OP_CHUNK || chunk->payload.empty() ||
                chunk->payload.size() > size - bytes.size()) return false;
            bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
        }
        const auto operation = DeserializeProtocolOperation(bytes);
        if (!operation) return false;
        const auto result = runtime.SubmitOperation(*operation);
        std::vector<unsigned char> response{static_cast<unsigned char>(result.status)};
        response.insert(response.end(), result.op_id.begin(), result.op_id.end());
        return Write(Frame{MessageType::OP_RESULT, response});
    }
    if (request->type != MessageType::GET_BLOCK || request->payload.size() != 8) return false;
    if (!(m_local_capabilities & CAP_SERVE_BLOCKS)) return false;
    const uint64_t height = Read64(request->payload.data());
    if (height == 0) return false;
    const auto block = runtime.GetBlockAtHeight(height);
    auto encoded = block ? SerializeFinalizedBlock(*block) : std::nullopt;
    if (block && !encoded) return false;
    const std::vector<unsigned char> empty;
    const auto& bytes = encoded ? *encoded : empty;
    if (bytes.size() > MAX_FINALIZED_BLOCK_FEED_BYTES) return false;
    std::vector<unsigned char> meta;
    Put32(meta, static_cast<uint32_t>(bytes.size()));
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::BLOCK_META, meta}, deadline)) return false;
    for (size_t offset = 0; offset < bytes.size(); offset += MAX_FRAME_PAYLOAD) {
        const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, bytes.size() - offset);
        if (!Write(Frame{MessageType::BLOCK_CHUNK,
            std::vector<unsigned char>{bytes.begin() + offset, bytes.begin() + offset + count}}, deadline)) return false;
    }
    return true;
}

} // namespace cybou::p2p
