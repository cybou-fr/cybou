// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/session.h>

#include <algorithm>
#include <array>
#include <thread>

namespace cybou::p2p {
namespace {
constexpr size_t HEADER_SIZE{10};
constexpr size_t HELLO_SIZE{88};

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
} // namespace

std::optional<std::vector<unsigned char>> EncodeFrame(const Frame& frame)
{
    if (frame.payload.size() > MAX_FRAME_PAYLOAD ||
        (frame.type != MessageType::HELLO && frame.type != MessageType::PING && frame.type != MessageType::PONG)) return std::nullopt;
    std::vector<unsigned char> bytes{'C', 'Y', 'P', '2', WIRE_VERSION, static_cast<unsigned char>(frame.type)};
    const auto size = static_cast<uint32_t>(frame.payload.size());
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<unsigned char>(size >> (8 * i)));
    bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
    return bytes;
}

std::optional<Frame> DecodeFrame(std::span<const unsigned char> bytes)
{
    if (bytes.size() < HEADER_SIZE || !std::equal(bytes.begin(), bytes.begin() + 4, "CYP2") ||
        bytes[4] != WIRE_VERSION || bytes[5] < 1 || bytes[5] > 3) return std::nullopt;
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

bool PeerSession::ReadExact(unsigned char* out, size_t length)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
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

bool PeerSession::WriteExact(const unsigned char* bytes, size_t length)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
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
    const auto bytes = EncodeFrame(frame);
    if (!bytes) return false;
    return WriteExact(bytes->data(), bytes->size());
}

std::optional<Frame> PeerSession::Read()
{
    std::array<unsigned char, HEADER_SIZE> header{};
    if (!ReadExact(header.data(), header.size())) return std::nullopt;
    uint32_t size{0};
    for (int i = 0; i < 4; ++i) size |= uint32_t{header[6 + i]} << (8 * i);
    if (size > MAX_FRAME_PAYLOAD) return std::nullopt;
    std::vector<unsigned char> bytes{header.begin(), header.end()};
    bytes.resize(HEADER_SIZE + size);
    if (size && !ReadExact(bytes.data() + HEADER_SIZE, size)) {
        return std::nullopt;
    }
    return DecodeFrame(bytes);
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

} // namespace cybou::p2p
