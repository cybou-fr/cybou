// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_SESSION_H
#define CYBOU_P2P_SESSION_H

#include <uint256.h>
#include <cybou/authority_node.h>

#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <chrono>
#include <optional>
#include <span>
#include <vector>

namespace cybou { class CybouNodeRuntime; }
namespace cybou::p2p {

inline constexpr uint32_t MAX_FRAME_PAYLOAD{4096};
inline constexpr uint8_t WIRE_VERSION{1};

enum class MessageType : uint8_t {
    HELLO = 1, PING = 2, PONG = 3, GET_BLOCK = 4, BLOCK_META = 5,
    BLOCK_CHUNK = 6, OP_META = 7, OP_CHUNK = 8, OP_RESULT = 9,
};

struct Frame {
    MessageType type;
    std::vector<unsigned char> payload;
};

struct Hello {
    uint256 network_id;
    uint64_t finalized_height{0};
    uint256 finalized_tip;
    uint64_t capabilities{0};
    uint64_t nonce{0};

    friend bool operator==(const Hello&, const Hello&) = default;
};

std::optional<std::vector<unsigned char>> EncodeFrame(const Frame& frame);
std::optional<Frame> DecodeFrame(std::span<const unsigned char> bytes);
std::vector<unsigned char> EncodeHello(const Hello& hello);
std::optional<Hello> DecodeHello(std::span<const unsigned char> bytes);

// One persistent TCP socket. The caller owns connection setup and deadlines.
class PeerSession {
public:
    explicit PeerSession(boost::asio::ip::tcp::socket socket);
    bool Handshake(const Hello& local);
    bool Ping(uint64_t nonce);
    bool AnswerPing();
    // Empty bytes mean the height is not available. Nullopt means a protocol
    // or transport failure; callers must verify returned blocks before commit.
    std::optional<std::vector<unsigned char>> RequestBlock(uint64_t height);
    std::optional<OperationSubmitResult> SubmitOperation(const ProtocolOperation& operation);
    bool ServeNext(CybouNodeRuntime& runtime);
    const std::optional<Hello>& Peer() const { return m_peer; }
    boost::asio::ip::tcp::socket& Socket() { return m_socket; }

private:
    bool ReadExact(unsigned char* out, size_t length, std::chrono::steady_clock::time_point deadline);
    bool WriteExact(const unsigned char* bytes, size_t length, std::chrono::steady_clock::time_point deadline);
    bool Write(const Frame& frame);
    bool Write(const Frame& frame, std::chrono::steady_clock::time_point deadline);
    std::optional<Frame> Read(std::chrono::steady_clock::time_point deadline);
    std::optional<Frame> Read();
    boost::asio::ip::tcp::socket m_socket;
    std::optional<Hello> m_peer;
};

} // namespace cybou::p2p
#endif
