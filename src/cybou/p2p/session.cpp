// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/session.h>
#include <cybou/block_feed.h>
#include <cybou/node_runtime.h>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <array>
#include <limits>
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
        (static_cast<uint8_t>(frame.type) < 1 || static_cast<uint8_t>(frame.type) > 20)) return std::nullopt;
    std::vector<unsigned char> bytes{'C', 'Y', 'P', '2', WIRE_VERSION, static_cast<unsigned char>(frame.type)};
    const auto size = static_cast<uint32_t>(frame.payload.size());
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<unsigned char>(size >> (8 * i)));
    bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
    return bytes;
}

std::optional<Frame> DecodeFrame(std::span<const unsigned char> bytes)
{
    if (bytes.size() < HEADER_SIZE || !std::equal(bytes.begin(), bytes.begin() + 4, "CYP2") ||
        bytes[4] != WIRE_VERSION || bytes[5] < 1 || bytes[5] > 20) return std::nullopt;
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
    if (hello.network_id.IsNull() || hello.finalized_tip.IsNull() || hello.nonce == 0) return std::nullopt;
    return hello;
}

std::vector<unsigned char> EncodePeersPayload(const std::vector<std::pair<std::string, uint16_t>>& peers)
{
    std::vector<unsigned char> out;
    const uint8_t count = static_cast<uint8_t>(std::min<size_t>(peers.size(), MAX_PEER_DISCOVERY_ENTRIES));
    out.push_back(count);
    size_t added = 0;
    for (const auto& [host, port] : peers) {
        if (added >= count) break;
        if (port == 0) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        if (addr.is_v4()) {
            out.push_back(4);
            const auto bytes = addr.to_v4().to_bytes();
            out.insert(out.end(), bytes.begin(), bytes.end());
            out.push_back(static_cast<unsigned char>(port & 0xFF));
            out.push_back(static_cast<unsigned char>((port >> 8) & 0xFF));
            ++added;
        } else if (addr.is_v6()) {
            out.push_back(6);
            const auto bytes = addr.to_v6().to_bytes();
            out.insert(out.end(), bytes.begin(), bytes.end());
            out.push_back(static_cast<unsigned char>(port & 0xFF));
            out.push_back(static_cast<unsigned char>((port >> 8) & 0xFF));
            ++added;
        }
    }
    out[0] = static_cast<uint8_t>(added);
    return out;
}

std::optional<std::vector<std::pair<std::string, uint16_t>>> DecodePeersPayload(std::span<const unsigned char> bytes)
{
    if (bytes.empty()) return std::nullopt;
    const uint8_t count = bytes[0];
    if (count > MAX_PEER_DISCOVERY_ENTRIES) return std::nullopt;
    size_t offset = 1;
    std::vector<std::pair<std::string, uint16_t>> result;
    result.reserve(count);
    for (uint8_t i = 0; i < count; ++i) {
        if (offset >= bytes.size()) return std::nullopt;
        const uint8_t family = bytes[offset++];
        if (family == 4) {
            if (offset + 6 > bytes.size()) return std::nullopt;
            boost::asio::ip::address_v4::bytes_type v4_bytes;
            std::copy_n(bytes.begin() + offset, 4, v4_bytes.begin());
            offset += 4;
            const uint16_t port = static_cast<uint16_t>(bytes[offset]) |
                (static_cast<uint16_t>(bytes[offset + 1]) << 8);
            offset += 2;
            if (port != 0) {
                result.emplace_back(boost::asio::ip::make_address_v4(v4_bytes).to_string(), port);
            }
        } else if (family == 6) {
            if (offset + 18 > bytes.size()) return std::nullopt;
            boost::asio::ip::address_v6::bytes_type v6_bytes;
            std::copy_n(bytes.begin() + offset, 16, v6_bytes.begin());
            offset += 16;
            const uint16_t port = static_cast<uint16_t>(bytes[offset]) |
                (static_cast<uint16_t>(bytes[offset + 1]) << 8);
            offset += 2;
            if (port != 0) {
                result.emplace_back(boost::asio::ip::make_address_v6(v6_bytes).to_string(), port);
            }
        } else {
            return std::nullopt;
        }
    }
    if (offset != bytes.size()) return std::nullopt;
    return result;
}

bool MatchesKnownFinalizedChain(const CybouNodeRuntime& runtime, const Hello& peer)
{
    if (peer.finalized_tip.IsNull()) return false;
    const auto status = runtime.GetStatus();
    if (!status.is_initialized) return false;
    if (peer.finalized_height == 0) {
        return peer.finalized_tip == runtime.GetNetworkDefinition().genesis_block_id;
    }
    if (peer.finalized_height > status.finalized_height) return true;
    const auto known = runtime.GetBlockAtHeight(peer.finalized_height);
    return !known || ComputeBlockId(known->block) == peer.finalized_tip;
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
        if (ec || count == 0) {
            boost::system::error_code close_ec;
            m_socket.close(close_ec);
            return false;
        }
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
        if (ec || count == 0) {
            boost::system::error_code close_ec;
            m_socket.close(close_ec);
            return false;
        }
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
    m_last_read_status = ReadStatus::UNAVAILABLE;
    std::array<unsigned char, HEADER_SIZE> header{};
    if (!ReadExact(header.data(), header.size(), deadline)) return std::nullopt;
    if (!std::equal(header.begin(), header.begin() + 4, "CYP2") ||
        header[4] != WIRE_VERSION || header[5] < 1 || header[5] > 20) {
        m_last_read_status = ReadStatus::INVALID_FRAME;
        return std::nullopt;
    }
    uint32_t size{0};
    for (int i = 0; i < 4; ++i) size |= uint32_t{header[6 + i]} << (8 * i);
    if (size > MAX_FRAME_PAYLOAD) {
        m_last_read_status = ReadStatus::INVALID_FRAME;
        return std::nullopt;
    }
    std::vector<unsigned char> bytes{header.begin(), header.end()};
    bytes.resize(HEADER_SIZE + size);
    if (size && !ReadExact(bytes.data() + HEADER_SIZE, size, deadline)) {
        return std::nullopt;
    }
    auto frame = DecodeFrame(bytes);
    m_last_read_status = frame ? ReadStatus::OK : ReadStatus::INVALID_FRAME;
    return frame;
}

std::optional<Frame> PeerSession::Read()
{
    return Read(std::chrono::steady_clock::now() + std::chrono::seconds(5));
}

bool PeerSession::Handshake(const Hello& local)
{
    m_handshake_status = HandshakeStatus::INVALID_LOCAL;
    if (local.network_id.IsNull() || local.finalized_tip.IsNull() || local.nonce == 0) return false;
    m_handshake_status = HandshakeStatus::UNAVAILABLE;
    if (!Write(Frame{MessageType::HELLO, EncodeHello(local)})) return false;
    const auto frame = Read();
    if (!frame) {
        if (m_last_read_status == ReadStatus::INVALID_FRAME) m_handshake_status = HandshakeStatus::INVALID_PEER;
        return false;
    }
    m_handshake_status = HandshakeStatus::INVALID_PEER;
    if (frame->type != MessageType::HELLO) return false;
    const auto peer = DecodeHello(frame->payload);
    if (!peer || peer->network_id != local.network_id || peer->nonce == local.nonce) return false;
    m_peer = *peer;
    m_local_capabilities = local.capabilities;
    m_handshake_status = HandshakeStatus::CONNECTED;
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

BlockRequestResult PeerSession::RequestBlock(uint64_t height)
{
    if (!m_peer || !(m_peer->capabilities & CAP_SERVE_BLOCKS) || height == 0)
        return {.status = BlockRequestStatus::INVALID_REQUEST, .bytes = {}};
    std::vector<unsigned char> request;
    Put64(request, height);
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::GET_BLOCK, request}, deadline))
        return {.status = BlockRequestStatus::UNAVAILABLE, .bytes = {}};
    const auto meta = Read(deadline);
    if (!meta) return {.status = m_last_read_status == ReadStatus::INVALID_FRAME ?
        BlockRequestStatus::INVALID_RESPONSE : BlockRequestStatus::UNAVAILABLE, .bytes = {}};
    if (meta->type != MessageType::BLOCK_META || meta->payload.size() != 4)
        return {.status = BlockRequestStatus::INVALID_RESPONSE, .bytes = {}};
    const uint32_t size = Read32(meta->payload.data());
    if (size > MAX_FINALIZED_BLOCK_FEED_BYTES) return {.status = BlockRequestStatus::INVALID_RESPONSE, .bytes = {}};
    if (size == 0) return {.status = BlockRequestStatus::NOT_FOUND, .bytes = {}};
    std::vector<unsigned char> bytes;
    bytes.reserve(size);
    while (bytes.size() < size) {
        const auto chunk = Read(deadline);
        if (!chunk) return {.status = m_last_read_status == ReadStatus::INVALID_FRAME ?
            BlockRequestStatus::INVALID_RESPONSE : BlockRequestStatus::UNAVAILABLE, .bytes = {}};
        if (chunk->type != MessageType::BLOCK_CHUNK || chunk->payload.empty() ||
            chunk->payload.size() > size - bytes.size()) return {.status = BlockRequestStatus::INVALID_RESPONSE, .bytes = {}};
        bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
    }
    return {.status = BlockRequestStatus::OK, .bytes = std::move(bytes)};
}

BlockInventoryResult PeerSession::RequestBlockInventory(uint64_t first_height, uint8_t max_blocks)
{
    if (!m_peer || !(m_peer->capabilities & CAP_SERVE_BLOCKS) ||
        !(m_peer->capabilities & CAP_BLOCK_INVENTORY) || first_height == 0 ||
        max_blocks == 0 || max_blocks > MAX_BLOCK_INVENTORY ||
        first_height > std::numeric_limits<uint64_t>::max() - max_blocks + 1) {
        return {.status = BlockRequestStatus::INVALID_REQUEST, .blocks = {}};
    }
    std::vector<unsigned char> request;
    Put64(request, first_height);
    request.push_back(max_blocks);
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::GET_BLOCKS, request}, deadline)) {
        return {.status = BlockRequestStatus::UNAVAILABLE, .blocks = {}};
    }
    const auto response = Read(deadline);
    if (!response) return {.status = m_last_read_status == ReadStatus::INVALID_FRAME ?
        BlockRequestStatus::INVALID_RESPONSE : BlockRequestStatus::UNAVAILABLE, .blocks = {}};
    if (response->type != MessageType::BLOCK_INV || response->payload.empty()) {
        return {.status = BlockRequestStatus::INVALID_RESPONSE, .blocks = {}};
    }
    const uint8_t count = response->payload[0];
    if (count > max_blocks || response->payload.size() != 1U + size_t{count} * 40U) {
        return {.status = BlockRequestStatus::INVALID_RESPONSE, .blocks = {}};
    }
    if (count == 0) return {.status = BlockRequestStatus::NOT_FOUND, .blocks = {}};
    BlockInventoryResult result{.status = BlockRequestStatus::OK, .blocks = {}};
    result.blocks.reserve(count);
    for (uint8_t index{0}; index < count; ++index) {
        const size_t offset = 1U + size_t{index} * 40U;
        const uint64_t height = Read64(response->payload.data() + offset);
        uint256 block_id;
        std::copy_n(response->payload.begin() + offset + 8, 32, block_id.begin());
        if (height != first_height + index || block_id.IsNull()) {
            return {.status = BlockRequestStatus::INVALID_RESPONSE, .blocks = {}};
        }
        result.blocks.push_back({height, block_id});
    }
    return result;
}

std::optional<BlockAnnounceResult> PeerSession::AdvertiseBlock(
    const BlockAnnouncement& announcement, const FinalizedBlock& block, uint64_t& peer_finalized_height)
{
    peer_finalized_height = 0;
    if (!m_peer || !(m_peer->capabilities & CAP_BLOCK_ANNOUNCEMENTS) ||
        announcement.height == 0 || announcement.block_id.IsNull() ||
        block.block.height != announcement.height || ComputeBlockId(block.block) != announcement.block_id) return std::nullopt;
    std::vector<unsigned char> inventory{1};
    Put64(inventory, announcement.height);
    inventory.insert(inventory.end(), announcement.block_id.begin(), announcement.block_id.end());
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::BLOCK_INV, inventory}, deadline)) return std::nullopt;
    const auto answer = Read(deadline);
    if (!answer) return std::nullopt;
    if (answer->type == MessageType::GET_BLOCK && answer->payload.size() == 8 &&
        Read64(answer->payload.data()) == announcement.height) {
        const auto encoded = SerializeFinalizedBlock(block);
        if (!encoded || encoded->empty() || encoded->size() > MAX_FINALIZED_BLOCK_FEED_BYTES) return std::nullopt;
        std::vector<unsigned char> meta;
        Put32(meta, static_cast<uint32_t>(encoded->size()));
        if (!Write(Frame{MessageType::BLOCK_META, meta}, deadline)) return std::nullopt;
        for (size_t offset = 0; offset < encoded->size(); offset += MAX_FRAME_PAYLOAD) {
            const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, encoded->size() - offset);
            if (!Write(Frame{MessageType::BLOCK_CHUNK,
                {encoded->begin() + offset, encoded->begin() + offset + count}}, deadline)) return std::nullopt;
        }
    } else if (answer->type != MessageType::BLOCK_RESULT) {
        return std::nullopt;
    }
    const auto result = answer->type == MessageType::BLOCK_RESULT ? answer : Read(deadline);
    // Payload is [result:1][height:8][id:32] plus, from newer peers, the
    // acker's finalized height [peer_height:8] (49 bytes total).
    if (!result || result->type != MessageType::BLOCK_RESULT ||
        (result->payload.size() != 41 && result->payload.size() != 49) ||
        result->payload[0] > static_cast<uint8_t>(BlockAnnounceResult::GAP) ||
        Read64(result->payload.data() + 1) != announcement.height ||
        !std::equal(announcement.block_id.begin(), announcement.block_id.end(), result->payload.begin() + 9)) return std::nullopt;
    if (result->payload.size() == 49) peer_finalized_height = Read64(result->payload.data() + 41);
    return static_cast<BlockAnnounceResult>(result->payload[0]);
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

std::optional<OperationSubmitResult> PeerSession::AdvertiseOperation(const ProtocolOperation& operation)
{
    if (!m_peer || !(m_peer->capabilities & CAP_ACCEPT_OPERATIONS) ||
        !(m_peer->capabilities & CAP_OP_INVENTORY)) return std::nullopt;
    const auto id = ComputeOperationId(operation);
    const auto bytes = SerializeProtocolOperation(operation);
    if (!id || id->IsNull() || !bytes || bytes->empty() ||
        bytes->size() > MAX_OPERATION_PAYLOAD_BYTES) return std::nullopt;
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::OP_INV, {id->begin(), id->end()}}, deadline)) return std::nullopt;
    const auto answer = Read(deadline);
    if (!answer) return std::nullopt;
    if (answer->type == MessageType::OP_RESULT) {
        if (answer->payload.size() != 33 ||
            !std::equal(id->begin(), id->end(), answer->payload.begin() + 1) ||
            (answer->payload[0] != static_cast<uint8_t>(OperationSubmitStatus::ALREADY_PENDING) &&
             answer->payload[0] != static_cast<uint8_t>(OperationSubmitStatus::ALREADY_FINALIZED))) return std::nullopt;
        return OperationSubmitResult{.status = static_cast<OperationSubmitStatus>(answer->payload[0]), .op_id = *id};
    }
    if (answer->type != MessageType::GET_OP || answer->payload.size() != 32 ||
        !std::equal(id->begin(), id->end(), answer->payload.begin())) return std::nullopt;

    std::vector<unsigned char> first;
    Put32(first, static_cast<uint32_t>(bytes->size()));
    const size_t first_count = std::min<size_t>(bytes->size(), MAX_FRAME_PAYLOAD - 4);
    first.insert(first.end(), bytes->begin(), bytes->begin() + first_count);
    if (!Write(Frame{MessageType::OP, std::move(first)}, deadline)) return std::nullopt;
    for (size_t offset = first_count; offset < bytes->size(); offset += MAX_FRAME_PAYLOAD) {
        const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, bytes->size() - offset);
        if (!Write(Frame{MessageType::OP,
            {bytes->begin() + offset, bytes->begin() + offset + count}}, deadline)) return std::nullopt;
    }
    const auto result = Read(deadline);
    if (!result || result->type != MessageType::OP_RESULT || result->payload.size() != 33 ||
        result->payload[0] > static_cast<uint8_t>(OperationSubmitStatus::NETWORK_MISMATCH) ||
        !std::equal(id->begin(), id->end(), result->payload.begin() + 1)) return std::nullopt;
    return OperationSubmitResult{.status = static_cast<OperationSubmitStatus>(result->payload[0]), .op_id = *id};
}

bool PeerSession::SendPrevote(const BftPrevoteMsg& prevote)
{
    if (!m_peer) return false;
    const auto serialized = SerializeBftPrevoteMsg(prevote);
    if (!serialized || serialized->size() > MAX_FRAME_PAYLOAD) return false;
    return Write(Frame{MessageType::CONSENSUS_PREVOTE, *serialized});
}

bool PeerSession::SendPrecommit(const BftPrecommitMsg& precommit)
{
    if (!m_peer) return false;
    const auto serialized = SerializeBftPrecommitMsg(precommit);
    if (!serialized || serialized->size() > MAX_FRAME_PAYLOAD) return false;
    return Write(Frame{MessageType::CONSENSUS_PRECOMMIT, *serialized});
}

bool PeerSession::SendProposal(const BftProposalMsg& proposal)
{
    if (!m_peer) return false;
    const auto serialized = SerializeBftProposalMsg(proposal);
    if (!serialized || serialized->empty()) return false;
    std::vector<unsigned char> first;
    Put32(first, static_cast<uint32_t>(serialized->size()));
    const size_t first_count = std::min<size_t>(serialized->size(), MAX_FRAME_PAYLOAD - 4);
    first.insert(first.end(), serialized->begin(), serialized->begin() + first_count);
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::CONSENSUS_PROPOSAL, std::move(first)}, deadline)) return false;
    for (size_t offset = first_count; offset < serialized->size(); offset += MAX_FRAME_PAYLOAD) {
        const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, serialized->size() - offset);
        if (!Write(Frame{MessageType::CONSENSUS_PROPOSAL,
            {serialized->begin() + offset, serialized->begin() + offset + count}}, deadline)) return false;
    }
    return true;
}

std::optional<BftProposalMsg> PeerSession::ReadProposal(std::chrono::steady_clock::time_point deadline)
{
    if (!m_peer) return std::nullopt;
    const auto first = Read(deadline);
    if (!first || first->type != MessageType::CONSENSUS_PROPOSAL || first->payload.size() < 5) return std::nullopt;
    const uint32_t size = Read32(first->payload.data());
    if (size == 0 || size > MAX_AUTHORITY_SERIALIZED_BLOCK_BYTES + 4096 || first->payload.size() - 4 > size) return std::nullopt;
    std::vector<unsigned char> bytes{first->payload.begin() + 4, first->payload.end()};
    while (bytes.size() < size) {
        const auto chunk = Read(deadline);
        if (!chunk || chunk->type != MessageType::CONSENSUS_PROPOSAL || chunk->payload.empty() ||
            chunk->payload.size() > size - bytes.size()) return std::nullopt;
        bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
    }
    return DeserializeBftProposalMsg(bytes);
}

std::optional<BftPrevoteMsg> PeerSession::ReadPrevote(std::chrono::steady_clock::time_point deadline)
{
    if (!m_peer) return std::nullopt;
    const auto frame = Read(deadline);
    if (!frame || frame->type != MessageType::CONSENSUS_PREVOTE) return std::nullopt;
    return DeserializeBftPrevoteMsg(frame->payload);
}

std::optional<BftPrecommitMsg> PeerSession::ReadPrecommit(std::chrono::steady_clock::time_point deadline)
{
    if (!m_peer) return std::nullopt;
    const auto frame = Read(deadline);
    if (!frame || frame->type != MessageType::CONSENSUS_PRECOMMIT) return std::nullopt;
    return DeserializeBftPrecommitMsg(frame->payload);
}

std::vector<std::pair<std::string, uint16_t>> PeerSession::RequestPeers(std::chrono::steady_clock::time_point deadline)
{
    if (!m_peer || !(m_peer->capabilities & CAP_PEER_DISCOVERY)) return {};
    if (!Write(Frame{MessageType::GET_PEERS, {}}, deadline)) return {};
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::PEERS) return {};
    const auto peers = DecodePeersPayload(response->payload);
    return peers ? *peers : std::vector<std::pair<std::string, uint16_t>>{};
}

bool PeerSession::SendPeers(const std::vector<std::pair<std::string, uint16_t>>& peers,
    std::chrono::steady_clock::time_point deadline)
{
    const auto payload = EncodePeersPayload(peers);
    return Write(Frame{MessageType::PEERS, payload}, deadline);
}

bool PeerSession::ServeNext(CybouNodeRuntime& runtime)
{
    if (!m_peer) return false;
    const auto request = Read();
    if (!request) {
        return m_socket.is_open();
    }
    if (request->type == MessageType::GET_PEERS) {
        if (!(m_local_capabilities & CAP_PEER_DISCOVERY)) return false;
        const auto endpoints = runtime.GetPeerEndpointsForGossip();
        return SendPeers(endpoints);
    }
    if (request->type == MessageType::PING) {
        return request->payload.size() == 8 && Write(Frame{MessageType::PONG, request->payload});
    }
    if (request->type == MessageType::OP_INV) {
        if (!(m_local_capabilities & CAP_ACCEPT_OPERATIONS) ||
            !(m_local_capabilities & CAP_OP_INVENTORY) || request->payload.size() != 32) return false;
        uint256 id;
        std::copy(request->payload.begin(), request->payload.end(), id.begin());
        if (id.IsNull()) return false;
        const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        if (const auto known = runtime.KnownOperationStatus(id)) {
            std::vector<unsigned char> response{static_cast<unsigned char>(*known)};
            response.insert(response.end(), id.begin(), id.end());
            return Write(Frame{MessageType::OP_RESULT, response}, deadline);
        }
        if (!Write(Frame{MessageType::GET_OP, request->payload}, deadline)) return false;
        const auto first = Read(deadline);
        if (!first || first->type != MessageType::OP || first->payload.size() < 5) return false;
        const uint32_t size = Read32(first->payload.data());
        if (size == 0 || size > MAX_OPERATION_PAYLOAD_BYTES || first->payload.size() - 4 > size) return false;
        std::vector<unsigned char> bytes{first->payload.begin() + 4, first->payload.end()};
        while (bytes.size() < size) {
            const auto chunk = Read(deadline);
            if (!chunk || chunk->type != MessageType::OP || chunk->payload.empty() ||
                chunk->payload.size() > size - bytes.size()) return false;
            bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
        }
        const auto operation = DeserializeProtocolOperation(bytes);
        if (!operation || ComputeOperationId(*operation) != id) return false;
        boost::system::error_code endpoint_error;
        const auto endpoint = m_socket.remote_endpoint(endpoint_error);
        if (endpoint_error) return false;
        const auto result = runtime.SubmitPeerOperation(*operation, endpoint.address().to_string());
        std::vector<unsigned char> response{static_cast<unsigned char>(result.status)};
        response.insert(response.end(), result.op_id.begin(), result.op_id.end());
        return Write(Frame{MessageType::OP_RESULT, response}, deadline);
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
        boost::system::error_code endpoint_error;
        const auto endpoint = m_socket.remote_endpoint(endpoint_error);
        if (endpoint_error) return false;
        const auto result = runtime.SubmitPeerOperation(*operation, endpoint.address().to_string());
        std::vector<unsigned char> response{static_cast<unsigned char>(result.status)};
        response.insert(response.end(), result.op_id.begin(), result.op_id.end());
        return Write(Frame{MessageType::OP_RESULT, response});
    }
    if (request->type == MessageType::BLOCK_INV) {
        if (!(m_local_capabilities & CAP_BLOCK_ANNOUNCEMENTS) ||
            request->payload.size() != 41 || request->payload[0] != 1) return false;
        const uint64_t height = Read64(request->payload.data() + 1);
        uint256 id;
        std::copy_n(request->payload.begin() + 9, 32, id.begin());
        if (height == 0 || id.IsNull()) return false;
        auto status = runtime.GetStatus();
        if (!status.is_initialized) return false;
        // Fresh deadline per message: a bulk catch-up below performs many
        // round trips and must not inherit a stale timeout.
        auto acknowledge = [&](BlockAnnounceResult result) {
            std::vector<unsigned char> payload{static_cast<unsigned char>(result)};
            Put64(payload, height);
            payload.insert(payload.end(), id.begin(), id.end());
            // Report our current finalized height so the offerer can skip
            // heads we already have on its next fanout cycle.
            Put64(payload, runtime.GetStatus().finalized_height);
            const auto ack_deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
            return Write(Frame{MessageType::BLOCK_RESULT, payload}, ack_deadline);
        };
        // Pulls one block by height over this session and commits it after
        // canonical verification. Returns std::nullopt on transport/protocol
        // failure (the session must die); GAP when the peer cannot serve the
        // block; APPLIED on success. An expected_id of zero skips the id check
        // (used for catch-up pulls where we only know the height).
        auto fetch_and_commit = [&](uint64_t h, const uint256& expected_id) ->
            std::optional<BlockAnnounceResult> {
            const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
            std::vector<unsigned char> query;
            Put64(query, h);
            if (!Write(Frame{MessageType::GET_BLOCK, query}, deadline)) return std::nullopt;
            const auto meta = Read(deadline);
            if (!meta || meta->type != MessageType::BLOCK_META || meta->payload.size() != 4) return std::nullopt;
            const uint32_t size = Read32(meta->payload.data());
            if (size == 0) {
                return BlockAnnounceResult::GAP; // peer does not have this height
            }
            if (size > MAX_FINALIZED_BLOCK_FEED_BYTES) return std::nullopt;
            std::vector<unsigned char> bytes;
            bytes.reserve(size);
            while (bytes.size() < size) {
                const auto chunk = Read(deadline);
                if (!chunk || chunk->type != MessageType::BLOCK_CHUNK || chunk->payload.empty() ||
                    chunk->payload.size() > size - bytes.size()) return std::nullopt;
                bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
            }
            const auto block = DeserializeFinalizedBlock(bytes);
            if (!block || block->block.height != h ||
                block->certificate.network_id != status.network_id ||
                block->certificate.height != h) return std::nullopt;
            if (!expected_id.IsNull() &&
                (ComputeBlockId(block->block) != expected_id || block->certificate.block_id != expected_id)) {
                return std::nullopt;
            }
            // A failed commit (duplicate race or state conflict) must not kill
            // the session; the offered height simply stays unavailable.
            const bool committed = static_cast<bool>(runtime.CommitBlock(*block));
            return committed ? BlockAnnounceResult::APPLIED : BlockAnnounceResult::GAP;
        };
        auto have_height = [&](uint64_t h, const uint256& expected) {
            const auto known = runtime.GetBlockAtHeight(h);
            return known && ComputeBlockId(known->block) == expected;
        };
        if (height <= status.finalized_height) {
            return have_height(height, id) && acknowledge(BlockAnnounceResult::ALREADY_HAVE);
        }
        if (status.finalized_height == std::numeric_limits<uint64_t>::max() ||
            height != status.finalized_height + 1) {
            // The offered height is not our next block. We cannot pull over
            // this accepted session: the peer's side of this connection is a
            // client session that never reads (only the accepted side runs
            // ServeNext), so an unsolicited GET_BLOCK here would deadlock and
            // kill the session. Historical catch-up is instead driven by the
            // gossip worker through the client-side SyncFromPeer path.
            return acknowledge(BlockAnnounceResult::GAP);
        }
        const auto applied = fetch_and_commit(height, id);
        if (!applied) return false;
        return acknowledge(*applied);
    }
    if (request->type == MessageType::GET_BLOCKS) {
        if (!(m_local_capabilities & CAP_SERVE_BLOCKS) ||
            !(m_local_capabilities & CAP_BLOCK_INVENTORY) || request->payload.size() != 9) return false;
        const uint64_t first_height = Read64(request->payload.data());
        const uint8_t count = request->payload[8];
        if (first_height == 0 || count == 0 || count > MAX_BLOCK_INVENTORY ||
            first_height > std::numeric_limits<uint64_t>::max() - count + 1) return false;
        std::vector<unsigned char> inventory{0};
        for (uint8_t index{0}; index < count; ++index) {
            const uint64_t height = first_height + index;
            const auto finalized = runtime.GetBlockAtHeight(height);
            if (!finalized) break;
            const auto id = ComputeBlockId(finalized->block);
            if (id.IsNull()) return false;
            Put64(inventory, height);
            inventory.insert(inventory.end(), id.begin(), id.end());
            ++inventory[0];
        }
        return Write(Frame{MessageType::BLOCK_INV, inventory});
    }
    if (request->type == MessageType::CONSENSUS_PROPOSAL) {
        if (!(m_local_capabilities & CAP_CONSENSUS) || request->payload.size() < 5) return false;
        const uint32_t size = Read32(request->payload.data());
        if (size == 0 || size > MAX_AUTHORITY_SERIALIZED_BLOCK_BYTES + 4096 ||
            request->payload.size() - 4 > size) return false;
        std::vector<unsigned char> bytes{request->payload.begin() + 4, request->payload.end()};
        const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        while (bytes.size() < size) {
            const auto chunk = Read(deadline);
            if (!chunk || chunk->type != MessageType::CONSENSUS_PROPOSAL || chunk->payload.empty() ||
                chunk->payload.size() > size - bytes.size()) return false;
            bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
        }
        const auto proposal = DeserializeBftProposalMsg(bytes);
        if (!proposal) return false;
        runtime.ReceiveConsensusProposal(*proposal);
        return true;
    }
    if (request->type == MessageType::CONSENSUS_PREVOTE) {
        if (!(m_local_capabilities & CAP_CONSENSUS)) return false;
        const auto prevote = DeserializeBftPrevoteMsg(request->payload);
        if (!prevote) return false;
        runtime.ReceiveConsensusPrevote(*prevote);
        return true;
    }
    if (request->type == MessageType::CONSENSUS_PRECOMMIT) {
        if (!(m_local_capabilities & CAP_CONSENSUS)) return false;
        const auto precommit = DeserializeBftPrecommitMsg(request->payload);
        if (!precommit) return false;
        runtime.ReceiveConsensusPrecommit(*precommit);
        return true;
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
