// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/encrypted_chunk_graph.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <unordered_set>
#include <utility>

namespace cybou {
namespace {

constexpr std::uint64_t NODE_SCHEMA{1};
constexpr std::uint64_t LEAF_NODE{0};
constexpr std::uint64_t BRANCH_NODE{1};

struct ChildRef {
    ChunkId id{};
    std::size_t bytes{0};
};

struct ChunkIdHash {
    std::size_t operator()(const ChunkId& id) const noexcept
    {
        std::size_t result{0};
        for (const auto byte : id) result = (result * 131) ^ byte;
        return result;
    }
};

CborValue MakeLeaf(const std::span<const unsigned char> bytes)
{
    return CborValue::MapValue({
        {CborValue::Unsigned(0), CborValue::Unsigned(NODE_SCHEMA)},
        {CborValue::Unsigned(1), CborValue::Unsigned(LEAF_NODE)},
        {CborValue::Unsigned(2), CborValue::Unsigned(bytes.size())},
        {CborValue::Unsigned(3), CborValue::Bytes(CborValue::ByteString{bytes.begin(), bytes.end()})},
    });
}

CborValue MakeBranch(const std::span<const ChildRef> children)
{
    CborValue::Array child_ids;
    child_ids.reserve(children.size());
    std::uint64_t total_bytes{0};
    for (const auto& child : children) {
        total_bytes += child.bytes;
        child_ids.push_back(CborValue::Bytes(CborValue::ByteString{child.id.begin(), child.id.end()}));
    }
    return CborValue::MapValue({
        {CborValue::Unsigned(0), CborValue::Unsigned(NODE_SCHEMA)},
        {CborValue::Unsigned(1), CborValue::Unsigned(BRANCH_NODE)},
        {CborValue::Unsigned(2), CborValue::Unsigned(total_bytes)},
        {CborValue::Unsigned(3), CborValue::ArrayValue(std::move(child_ids))},
    });
}

std::optional<std::uint64_t> Unsigned(const CborValue& value)
{
    if (const auto* number = std::get_if<std::uint64_t>(&value.value)) return *number;
    return std::nullopt;
}

std::optional<std::size_t> ReadNode(
    const std::span<const unsigned char, 32> network_id,
    const std::span<const unsigned char, 32> content_key,
    const ChunkId& node_id,
    const EncryptedChunkLookup& lookup,
    std::unordered_set<ChunkId, ChunkIdHash>& visited,
    std::size_t depth,
    std::vector<unsigned char>& output)
{
    if (depth >= ENCRYPTED_GRAPH_MAX_DEPTH || visited.size() >= ENCRYPTED_GRAPH_MAX_CHUNKS ||
        !visited.insert(node_id).second) return std::nullopt;

    const auto stored = lookup(node_id);
    if (!stored) return std::nullopt;
    const auto decoded = DecryptGraphChunk(network_id, content_key, node_id, *stored);
    if (!decoded) return std::nullopt;
    const auto* fields = std::get_if<CborValue::Map>(&decoded->value);
    if (fields == nullptr || fields->size() != 4) return std::nullopt;
    for (std::size_t i = 0; i < fields->size(); ++i) {
        const auto key = Unsigned((*fields)[i].first);
        if (!key || *key != i) return std::nullopt;
    }
    const auto schema = Unsigned((*fields)[0].second);
    const auto kind = Unsigned((*fields)[1].second);
    const auto declared_bytes = Unsigned((*fields)[2].second);
    if (!schema || *schema != NODE_SCHEMA || !kind || !declared_bytes ||
        *declared_bytes > ENCRYPTED_GRAPH_MAX_PLAINTEXT_BYTES) return std::nullopt;

    if (*kind == LEAF_NODE) {
        const auto* payload = std::get_if<CborValue::ByteString>(&(*fields)[3].second.value);
        if (payload == nullptr || payload->size() > ENCRYPTED_GRAPH_LEAF_BYTES ||
            *declared_bytes != payload->size() ||
            output.size() > ENCRYPTED_GRAPH_MAX_PLAINTEXT_BYTES - payload->size()) return std::nullopt;
        output.insert(output.end(), payload->begin(), payload->end());
        return payload->size();
    }

    if (*kind != BRANCH_NODE) return std::nullopt;
    const auto* children = std::get_if<CborValue::Array>(&(*fields)[3].second.value);
    if (*declared_bytes == 0 || children == nullptr || children->size() < 2 ||
        children->size() > ENCRYPTED_GRAPH_MAX_CHILDREN) return std::nullopt;
    const auto bytes_before = output.size();
    for (const auto& child_value : *children) {
        const auto* child_bytes = std::get_if<CborValue::ByteString>(&child_value.value);
        if (child_bytes == nullptr || child_bytes->size() != ChunkId{}.size()) return std::nullopt;
        ChunkId child_id{};
        std::copy(child_bytes->begin(), child_bytes->end(), child_id.begin());
        if (!ReadNode(network_id, content_key, child_id, lookup, visited, depth + 1, output) ||
            output.size() - bytes_before > *declared_bytes) return std::nullopt;
    }
    const auto reconstructed = output.size() - bytes_before;
    if (reconstructed != *declared_bytes) return std::nullopt;
    return reconstructed;
}

} // namespace

std::optional<EncryptedChunkGraph> BuildEncryptedChunkGraph(
    const std::span<const unsigned char, 32> network_id,
    const std::span<const unsigned char> plaintext)
{
    if (plaintext.size() > ENCRYPTED_GRAPH_MAX_PLAINTEXT_BYTES) return std::nullopt;
    const auto content_key = GenerateGraphContentKey();
    if (!content_key) return std::nullopt;

    try {
        EncryptedChunkGraph graph;
        graph.content_key = *content_key;
        graph.plaintext_bytes = plaintext.size();
        std::vector<ChildRef> layer;
        const auto leaf_count = std::max<std::size_t>(1,
            (plaintext.size() + ENCRYPTED_GRAPH_LEAF_BYTES - 1) / ENCRYPTED_GRAPH_LEAF_BYTES);
        if (leaf_count > ENCRYPTED_GRAPH_MAX_CHUNKS) return std::nullopt;
        layer.reserve(leaf_count);
        graph.chunks.reserve(leaf_count);

        for (std::size_t offset = 0; offset < plaintext.size() || (plaintext.empty() && offset == 0);) {
            const auto bytes = plaintext.subspan(offset,
                std::min(ENCRYPTED_GRAPH_LEAF_BYTES, plaintext.size() - offset));
            const auto encrypted = EncryptGraphChunk(network_id, graph.content_key, MakeLeaf(bytes));
            if (!encrypted) return std::nullopt;
            layer.push_back({encrypted->id, bytes.size()});
            graph.chunks.push_back(*encrypted);
            if (plaintext.empty()) break;
            offset += bytes.size();
        }

        std::size_t depth{1};
        while (layer.size() > 1) {
            if (depth >= ENCRYPTED_GRAPH_MAX_DEPTH) return std::nullopt;
            ++depth;
            std::vector<ChildRef> next;
            const auto group_count = (layer.size() + ENCRYPTED_GRAPH_MAX_CHILDREN - 1) / ENCRYPTED_GRAPH_MAX_CHILDREN;
            const auto base_group_size = layer.size() / group_count;
            const auto larger_groups = layer.size() % group_count;
            next.reserve(group_count);
            std::size_t offset{0};
            for (std::size_t group = 0; group < group_count; ++group) {
                const auto count = base_group_size + (group < larger_groups ? 1 : 0);
                const auto children = std::span<const ChildRef>{layer}.subspan(offset, count);
                offset += count;
                const auto total = std::accumulate(children.begin(), children.end(), std::size_t{0},
                    [](const auto sum, const auto& child) { return sum + child.bytes; });
                const auto encrypted = EncryptGraphChunk(network_id, graph.content_key, MakeBranch(children));
                if (!encrypted || graph.chunks.size() >= ENCRYPTED_GRAPH_MAX_CHUNKS) return std::nullopt;
                next.push_back({encrypted->id, total});
                graph.chunks.push_back(*encrypted);
            }
            layer = std::move(next);
        }
        graph.root_id = layer.front().id;
        return graph;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::vector<unsigned char>> FetchEncryptedChunkGraph(
    const std::span<const unsigned char, 32> network_id,
    const std::span<const unsigned char, 32> content_key,
    const ChunkId& root_id,
    const EncryptedChunkLookup& lookup)
{
    if (!lookup) return std::nullopt;
    try {
        std::unordered_set<ChunkId, ChunkIdHash> visited;
        std::vector<unsigned char> output;
        output.reserve(1024);
        const auto reconstructed = ReadNode(network_id, content_key, root_id, lookup, visited, 0, output);
        if (!reconstructed || *reconstructed != output.size() ||
            output.size() > ENCRYPTED_GRAPH_MAX_PLAINTEXT_BYTES) return std::nullopt;
        return output;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace cybou
