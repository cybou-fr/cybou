// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/block.h>

#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <string_view>

namespace cybou {

namespace {

inline void AppendUint64LE(std::vector<unsigned char>& out, uint64_t val)
{
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

inline void AppendUint32LE(std::vector<unsigned char>& out, uint32_t val)
{
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

inline uint64_t ReadUint64LE(const std::span<const unsigned char>& bytes, size_t offset)
{
    uint64_t val{0};
    for (int i = 0; i < 8; ++i) {
        val |= uint64_t{bytes[offset + i]} << (8 * i);
    }
    return val;
}

inline uint32_t ReadUint32LE(const std::span<const unsigned char>& bytes, size_t offset)
{
    uint32_t val{0};
    for (int i = 0; i < 4; ++i) {
        val |= uint32_t{bytes[offset + i]} << (8 * i);
    }
    return val;
}

} // namespace

cybou::Hash256 ComputeOperationsRootFromHashes(std::span<const cybou::Hash256> hashes)
{
    static constexpr std::string_view DOMAIN{"CYBOU/OPS-ROOT"};
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());

    unsigned char count_bytes[4];
    for (int i = 0; i < 4; ++i) {
        count_bytes[i] = static_cast<unsigned char>(hashes.size() >> (8 * i));
    }
    hasher.Write(count_bytes, sizeof(count_bytes));

    for (const auto& op_hash : hashes) {
        hasher.Write(op_hash.begin(), op_hash.size());
    }

    cybou::Hash256 root;
    hasher.Finalize(root.begin());
    return root;
}

cybou::Hash256 ComputeOperationsRoot(const std::vector<ProtocolOperation>& operations)
{
    std::vector<cybou::Hash256> hashes;
    hashes.reserve(operations.size());
    for (const auto& op : operations) {
        const auto serialized = SerializeProtocolOperation(op);
        if (!serialized) continue;
        cybou::Hash256 op_hash;
        ::cybou::crypto::Sha256().Write(serialized->data(), serialized->size()).Finalize(op_hash.begin());
        hashes.push_back(op_hash);
    }
    return ComputeOperationsRootFromHashes(hashes);
}

cybou::Hash256 ComputeBlockHeaderId(const CybouBlockHeader& header)
{
    static constexpr std::string_view DOMAIN{"CYBOU/BLOCK"};
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(header.parent_block_id.begin(), header.parent_block_id.size());

    unsigned char height_bytes[8];
    for (int i = 0; i < 8; ++i) {
        height_bytes[i] = static_cast<unsigned char>(header.height >> (8 * i));
    }
    hasher.Write(height_bytes, sizeof(height_bytes));
    hasher.Write(header.operations_root.begin(), header.operations_root.size());
    hasher.Write(header.resulting_state_root.begin(), header.resulting_state_root.size());

    cybou::Hash256 block_id;
    hasher.Finalize(block_id.begin());
    return block_id;
}

CybouBlockHeader ExtractBlockHeader(const CybouBlock& block)
{
    return CybouBlockHeader{
        .parent_block_id = block.parent_block_id,
        .height = block.height,
        .operations_root = ComputeOperationsRoot(block.operations),
        .resulting_state_root = block.resulting_state_root,
    };
}

cybou::Hash256 ComputeBlockId(const CybouBlock& block)
{
    return ComputeBlockHeaderId(ExtractBlockHeader(block));
}

std::optional<std::vector<unsigned char>> SerializeBlock(const CybouBlock& block)
{
    std::vector<unsigned char> out;
    out.reserve(76); // base header size

    out.insert(out.end(), block.parent_block_id.begin(), block.parent_block_id.end());
    AppendUint64LE(out, block.height);
    out.insert(out.end(), block.resulting_state_root.begin(), block.resulting_state_root.end());

    AppendUint32LE(out, static_cast<uint32_t>(block.operations.size()));

    for (const auto& op : block.operations) {
        const auto serialized_op = SerializeProtocolOperation(op);
        if (!serialized_op) return std::nullopt;
        AppendUint32LE(out, static_cast<uint32_t>(serialized_op->size()));
        out.insert(out.end(), serialized_op->begin(), serialized_op->end());
    }

    return out;
}

std::optional<CybouBlock> DeserializeBlock(std::span<const unsigned char> bytes)
{
    static constexpr size_t HEADER_SIZE{32 + 8 + 32 + 4}; // 76 bytes
    if (bytes.size() < HEADER_SIZE) {
        return std::nullopt;
    }

    CybouBlock block;

    size_t offset{0};
    std::copy_n(bytes.begin() + offset, 32, block.parent_block_id.begin());
    offset += 32;

    block.height = ReadUint64LE(bytes, offset);
    offset += 8;

    std::copy_n(bytes.begin() + offset, 32, block.resulting_state_root.begin());
    offset += 32;

    const uint32_t op_count = ReadUint32LE(bytes, offset);
    offset += 4;

    // Every operation needs at least a length prefix. Bound allocation by input,
    // before trusting an attacker-controlled operation count.
    if (op_count > (bytes.size() - offset) / 4) return std::nullopt;
    block.operations.reserve(op_count);
    for (uint32_t i = 0; i < op_count; ++i) {
        if (offset + 4 > bytes.size()) {
            return std::nullopt;
        }
        const uint32_t op_len = ReadUint32LE(bytes, offset);
        offset += 4;

        if (offset + op_len > bytes.size()) {
            return std::nullopt;
        }

        const auto op = DeserializeProtocolOperation(bytes.subspan(offset, op_len));
        if (!op) {
            return std::nullopt;
        }
        block.operations.push_back(std::move(*op));
        offset += op_len;
    }

    if (offset != bytes.size()) {
        return std::nullopt;
    }

    return block;
}

std::optional<std::vector<unsigned char>> SerializeFinalizedBlock(const FinalizedBlock& finalized_block)
{
    const auto serialized_block = SerializeBlock(finalized_block.block);
    if (!serialized_block) return std::nullopt;
    const auto serialized_cert = SerializePoaFinalityCertificate(finalized_block.certificate);
    if (!serialized_cert) return std::nullopt;

    std::vector<unsigned char> out;
    out.reserve(8 + serialized_block->size() + serialized_cert->size());

    AppendUint32LE(out, static_cast<uint32_t>(serialized_block->size()));
    out.insert(out.end(), serialized_block->begin(), serialized_block->end());

    AppendUint32LE(out, static_cast<uint32_t>(serialized_cert->size()));
    out.insert(out.end(), serialized_cert->begin(), serialized_cert->end());

    return out;
}

std::optional<FinalizedBlock> DeserializeFinalizedBlock(std::span<const unsigned char> bytes)
{
    if (bytes.size() < 8) {
        return std::nullopt;
    }

    size_t offset{0};
    const uint32_t block_len = ReadUint32LE(bytes, offset);
    offset += 4;

    if (offset + block_len > bytes.size()) {
        return std::nullopt;
    }

    const auto block = DeserializeBlock(bytes.subspan(offset, block_len));
    if (!block) {
        return std::nullopt;
    }
    offset += block_len;

    if (offset + 4 > bytes.size()) {
        return std::nullopt;
    }
    const uint32_t cert_len = ReadUint32LE(bytes, offset);
    offset += 4;

    if (offset + cert_len != bytes.size()) {
        return std::nullopt;
    }

    const auto cert = DeserializePoaFinalityCertificate(bytes.subspan(offset, cert_len));
    if (!cert) {
        return std::nullopt;
    }

    return FinalizedBlock{
        .block = std::move(*block),
        .certificate = std::move(*cert),
    };
}

} // namespace cybou
