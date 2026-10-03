// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/encrypted_chunk_tree.h>

#include <cybou/binary_codec.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace cybou {
namespace {

constexpr std::uint64_t ROOT_KIND{0};
constexpr std::uint64_t INDEX_KIND{1};
constexpr std::uint64_t DATA_KIND{2};

class CleanseOnExit final
{
public:
    explicit CleanseOnExit(const std::span<unsigned char> bytes) : m_bytes{bytes} {}
    ~CleanseOnExit() { if (!m_bytes.empty()) OPENSSL_cleanse(m_bytes.data(), m_bytes.size()); }
    CleanseOnExit(const CleanseOnExit&) = delete;
    CleanseOnExit& operator=(const CleanseOnExit&) = delete;

private:
    std::span<unsigned char> m_bytes;
};

struct ChildRef {
    ChunkId id{};
    std::uint64_t kind{DATA_KIND};
};

struct ChunkIdHash {
    std::size_t operator()(const ChunkId& id) const noexcept
    {
        std::size_t result{0};
        for (const auto byte : id) result = (result * 131) ^ byte;
        return result;
    }
};

std::vector<unsigned char> MakeMetadata(const std::uint8_t kind, std::span<const ChildRef> children,
    std::span<const unsigned char> private_metadata = {})
{
    if (children.size() > ENCRYPTED_TREE_MAX_CHILDREN || (kind == INDEX_KIND && children.empty()))
        throw std::invalid_argument{"invalid encrypted tree children"};
    BinaryWriter writer{ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES};
    writer.U8(kind);
    writer.U8(children.empty() ? DATA_KIND : children.front().kind);
    writer.U16(static_cast<std::uint16_t>(children.size()));
    for (const auto& child : children) writer.Fixed(child.id);
    if (kind == ROOT_KIND) writer.Bytes(private_metadata, ENCRYPTED_TREE_ROOT_PRIVATE_METADATA_MAX_BYTES);
    return writer.Take();
}
std::optional<std::vector<ChildRef>> ParseMetadata(std::span<const unsigned char> bytes,
    const std::uint64_t expected_kind, std::uint64_t& child_kind,
    std::vector<unsigned char>* root_private_metadata = nullptr)
{
    try {
        BinaryReader reader{bytes, ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES};
        if (reader.U8() != expected_kind) return std::nullopt;
        const auto kind = reader.U8();
        const auto count = reader.U16();
        if ((kind != INDEX_KIND && kind != DATA_KIND) || count > ENCRYPTED_TREE_MAX_CHILDREN ||
            (expected_kind == INDEX_KIND && count == 0)) return std::nullopt;
        std::vector<ChildRef> children;
        children.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i) children.push_back({reader.Fixed<ChunkId>(), kind});
        if (expected_kind == ROOT_KIND) {
            if (!root_private_metadata) return std::nullopt;
            const auto metadata = reader.Bytes(ENCRYPTED_TREE_ROOT_PRIVATE_METADATA_MAX_BYTES);
            root_private_metadata->assign(metadata.begin(), metadata.end());
        }
        reader.Finish(); child_kind = kind; return children;
    } catch (...) { return std::nullopt; }
}

std::optional<std::size_t> RandomDataTarget()
{
    std::array<unsigned char, 4> random{};
    if (RAND_bytes(random.data(), static_cast<int>(random.size())) != 1) return std::nullopt;
    const auto value = (std::uint32_t{random[0]} << 24) | (std::uint32_t{random[1]} << 16) |
        (std::uint32_t{random[2]} << 8) | random[3];
    constexpr auto width = ENCRYPTED_TREE_DATA_MAX_BYTES - ENCRYPTED_TREE_DATA_MIN_BYTES + 1;
    return ENCRYPTED_TREE_DATA_MIN_BYTES + (value % width);
}

bool CheckedAdd(std::uint64_t& value, const std::uint64_t delta)
{
    if (delta > std::numeric_limits<std::uint64_t>::max() - value) return false;
    value += delta;
    return true;
}

class TreeBuilder final
{
public:
    TreeBuilder(const std::span<const unsigned char, 32> network, const ContentKey& key,
        const EncryptedTreeStage& stage, EncryptedTreeSummary& summary,
        const std::span<const unsigned char> private_root_metadata)
        : m_network{network}, m_key{key}, m_stage{stage}, m_summary{summary},
          m_private_root_metadata{private_root_metadata.begin(), private_root_metadata.end()}
    {
        m_levels.resize(ENCRYPTED_TREE_MAX_DEPTH - 2);
    }

    ~TreeBuilder()
    {
        if (!m_private_root_metadata.empty()) OPENSSL_cleanse(m_private_root_metadata.data(), m_private_root_metadata.size());
    }

    bool AddData(const std::span<const unsigned char> plaintext)
    {
        const auto encrypted = EncryptChunk(m_network, m_key, plaintext);
        if (!encrypted || !Store(*encrypted)) return false;
        return Push(0, ChildRef{encrypted->id, DATA_KIND});
    }

    bool Finish(ChunkId& root_id)
    {
        const bool has_children = std::any_of(m_levels.begin(), m_levels.end(),
            [](const auto& children) { return !children.empty(); });
        if (!has_children) {
            const auto root_bytes = MakeMetadata(ROOT_KIND, {}, m_private_root_metadata);
            const auto root = EncryptChunk(m_network, m_key, root_bytes);
            if (!root || !Store(*root)) return false;
            root_id = root->id;
            const auto authorization = m_authorization.Finish();
            if (!authorization || authorization->chunk_count != m_summary.chunk_count) return false;
            m_summary.chunk_authorization_root = authorization->root;
            return true;
        }
        for (std::size_t level = 0; level < m_levels.size(); ++level) {
            auto& pending = m_levels[level];
            if (pending.empty()) continue;
            const bool higher_pending = std::any_of(m_levels.begin() + level + 1, m_levels.end(),
                [](const auto& children) { return !children.empty(); });
            if (!higher_pending) {
                auto root_bytes = MakeMetadata(ROOT_KIND, pending, m_private_root_metadata);
                if (root_bytes.size() > ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES) {
                    if (!root_bytes.empty()) OPENSSL_cleanse(root_bytes.data(), root_bytes.size());
                    return false;
                }
                const auto root = EncryptChunk(m_network, m_key, root_bytes);
                if (!root_bytes.empty()) OPENSSL_cleanse(root_bytes.data(), root_bytes.size());
                if (!root || !Store(*root)) return false;
                root_id = root->id;
                const auto authorization = m_authorization.Finish();
                if (!authorization || authorization->chunk_count != m_summary.chunk_count) return false;
                m_summary.chunk_authorization_root = authorization->root;
                return true;
            }
            if (!Flush(level)) return false;
        }
        return false;
    }

private:
    bool Store(const EncryptedChunk& chunk)
    {
        if (m_summary.chunk_count >= MAX_PUBLICATION_CHUNKS ||
            !m_stage(static_cast<std::uint32_t>(m_summary.chunk_count), chunk) ||
            !m_authorization.Add(AuthorizedChunk{chunk.id}) ||
            !CheckedAdd(m_summary.chunk_count, 1)) return false;
        return true;
    }

    bool Push(const std::size_t level, const ChildRef& child)
    {
        if (level >= m_levels.size()) return false;
        auto& pending = m_levels[level];
        if (pending.size() == ENCRYPTED_TREE_MAX_CHILDREN && !Flush(level)) return false;
        pending.push_back(child);
        return true;
    }

    bool Flush(const std::size_t level)
    {
        if (level + 1 >= m_levels.size() || m_levels[level].empty()) return false;
        const auto metadata = MakeMetadata(INDEX_KIND, m_levels[level]);
        if (metadata.size() > ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES) return false;
        const auto encrypted = EncryptChunk(m_network, m_key, metadata);
        if (!encrypted || !Store(*encrypted)) return false;
        m_levels[level].clear();
        return Push(level + 1, ChildRef{encrypted->id, INDEX_KIND});
    }

    std::span<const unsigned char, 32> m_network;
    const ContentKey& m_key;
    const EncryptedTreeStage& m_stage;
    EncryptedTreeSummary& m_summary;
    std::vector<unsigned char> m_private_root_metadata;
    std::vector<std::vector<ChildRef>> m_levels;
    ChunkAuthorizationAccumulator m_authorization;
};

bool ReadTreeNode(
    const std::span<const unsigned char, 32> network,
    const std::span<const unsigned char, 32> key,
    const ChildRef& node,
    const EncryptedChunkLookup& lookup,
    const EncryptedTreeVisit& visit,
    const EncryptedTreeSink& sink,
    const std::uint64_t max_output,
    const std::size_t depth,
    std::unordered_set<ChunkId, ChunkIdHash>& path,
    std::uint64_t& output_bytes)
{
    if (depth >= ENCRYPTED_TREE_MAX_DEPTH || !visit(node.id) || !path.insert(node.id).second) return false;
    const auto stored = lookup(node.id);
    if (!stored) { path.erase(node.id); return false; }
    auto plaintext = DecryptChunk(network, key, node.id, *stored);
    if (!plaintext) { path.erase(node.id); return false; }
    if (node.kind == DATA_KIND) {
        const bool valid = output_bytes <= max_output && plaintext->size() <= max_output - output_bytes && sink(*plaintext);
        if (valid) output_bytes += plaintext->size();
        if (!plaintext->empty()) OPENSSL_cleanse(plaintext->data(), plaintext->size());
        path.erase(node.id);
        return valid;
    }
    std::uint64_t child_kind{0};
    const auto children = ParseMetadata(*plaintext, INDEX_KIND, child_kind);
    if (!plaintext->empty()) OPENSSL_cleanse(plaintext->data(), plaintext->size());
    if (!children) { path.erase(node.id); return false; }
    for (const auto& child : *children) {
        ChildRef typed_child = child;
        typed_child.kind = child_kind;
        if (typed_child.kind == INDEX_KIND && !ReadTreeNode(network, key, typed_child, lookup, visit, sink,
                max_output, depth + 1, path, output_bytes)) { path.erase(node.id); return false; }
        if (typed_child.kind == DATA_KIND && !ReadTreeNode(network, key, typed_child, lookup, visit, sink,
                max_output, depth + 1, path, output_bytes)) { path.erase(node.id); return false; }
    }
    path.erase(node.id);
    return true;
}

bool EnumerateTreeNode(const std::span<const unsigned char, 32> network,
    const std::span<const unsigned char, 32> key, const ChildRef& node,
    const EncryptedChunkLookup& lookup, const EncryptedTreeVisit& visit,
    const std::size_t depth, std::unordered_set<ChunkId, ChunkIdHash>& seen)
{
    if (depth >= ENCRYPTED_TREE_MAX_DEPTH || !seen.insert(node.id).second || !visit(node.id)) return false;
    if (node.kind == DATA_KIND) return true;
    const auto stored = lookup(node.id);
    if (!stored) return false;
    auto plaintext = DecryptChunk(network, key, node.id, *stored);
    if (!plaintext) return false;
    std::uint64_t child_kind{0};
    const auto children = ParseMetadata(*plaintext, INDEX_KIND, child_kind);
    if (!plaintext->empty()) OPENSSL_cleanse(plaintext->data(), plaintext->size());
    if (!children) return false;
    for (const auto& child : *children) {
        auto typed = child;
        typed.kind = child_kind;
        if (!EnumerateTreeNode(network, key, typed, lookup, visit, depth + 1, seen)) return false;
    }
    return true;
}

} // namespace

std::optional<EncryptedTreeSummary> BuildEncryptedChunkTree(
    const std::span<const unsigned char, 32> network_binding,
    const EncryptedTreeSource& source,
    const EncryptedTreeStage& stage,
    const std::span<const unsigned char> private_root_metadata)
{
    if (!source || !stage || private_root_metadata.size() > ENCRYPTED_TREE_ROOT_PRIVATE_METADATA_MAX_BYTES) return std::nullopt;
    auto key = GenerateContentKey();
    if (!key) return std::nullopt;
    CleanseOnExit cleanse_generated_key{*key};
    EncryptedTreeSummary summary;
    summary.content_key = *key;
    CleanseOnExit cleanse_summary_key{summary.content_key};
    try {
        TreeBuilder builder(network_binding, summary.content_key, stage, summary, private_root_metadata);
        bool eof{false};
        while (!eof) {
            const auto target = RandomDataTarget();
            if (!target) return std::nullopt;
            std::vector<unsigned char> plaintext(*target);
            CleanseOnExit cleanse_plaintext{plaintext};
            std::size_t filled{0};
            while (filled < plaintext.size()) {
                const auto read = source(std::span<unsigned char>{plaintext}.subspan(filled));
                if (!read || *read > plaintext.size() - filled) return std::nullopt;
                if (*read == 0) { eof = true; break; }
                filled += *read;
                if (filled > std::numeric_limits<std::uint64_t>::max() - summary.plaintext_bytes) return std::nullopt;
                summary.plaintext_bytes += *read;
            }
            if (filled != 0) {
                if (!builder.AddData(std::span<const unsigned char>{plaintext}.first(filled))) return std::nullopt;
            }
        }
        if (!builder.Finish(summary.root_chunk_id)) return std::nullopt;
        return summary;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::uint64_t> FetchEncryptedChunkTree(
    const std::span<const unsigned char, 32> network_binding,
    const std::span<const unsigned char, 32> content_key,
    const ChunkId& root_chunk_id,
    const EncryptedChunkLookup& lookup,
    const EncryptedTreeRootMetadataSink& root_metadata_sink,
    const EncryptedTreeVisit& visit,
    const EncryptedTreeSink& sink,
    const std::uint64_t max_output_bytes)
{
    if (!lookup || !root_metadata_sink || !visit || !sink || !visit(root_chunk_id)) return std::nullopt;
    try {
        const auto stored_root = lookup(root_chunk_id);
        if (!stored_root) return std::nullopt;
        auto root_plaintext = DecryptChunk(network_binding, content_key, root_chunk_id, *stored_root);
        if (!root_plaintext) return std::nullopt;
        std::uint64_t child_kind{0};
        std::vector<unsigned char> app_metadata;
        const auto children = ParseMetadata(*root_plaintext, ROOT_KIND, child_kind, &app_metadata);
        if (!root_plaintext->empty()) OPENSSL_cleanse(root_plaintext->data(), root_plaintext->size());
        if (!children) return std::nullopt;
        bool root_metadata_accepted{false};
        try { root_metadata_accepted = root_metadata_sink(app_metadata); }
        catch (...) {
            if (!app_metadata.empty()) OPENSSL_cleanse(app_metadata.data(), app_metadata.size());
            throw;
        }
        if (!app_metadata.empty()) OPENSSL_cleanse(app_metadata.data(), app_metadata.size());
        if (!root_metadata_accepted) return std::nullopt;
        std::unordered_set<ChunkId, ChunkIdHash> path;
        path.insert(root_chunk_id);
        std::uint64_t written{0};
        for (const auto& child : *children) {
            auto typed_child = child;
            typed_child.kind = child_kind;
            if (!ReadTreeNode(network_binding, content_key, typed_child, lookup, visit, sink,
                    max_output_bytes, 1, path, written)) return std::nullopt;
        }
        return written;
    } catch (...) {
        return std::nullopt;
    }
}

bool EnumerateEncryptedTreeChunks(const std::span<const unsigned char, 32> network_binding,
    const std::span<const unsigned char, 32> content_key, const ChunkId& root_chunk_id,
    const EncryptedChunkLookup& lookup, const EncryptedTreeVisit& visit)
{
    if (!lookup || !visit || root_chunk_id == ChunkId{} || !visit(root_chunk_id)) return false;
    try {
        const auto stored_root = lookup(root_chunk_id);
        if (!stored_root) return false;
        auto plaintext = DecryptChunk(network_binding, content_key, root_chunk_id, *stored_root);
        if (!plaintext) return false;
        std::uint64_t child_kind{0};
        std::vector<unsigned char> private_metadata;
        const auto children = ParseMetadata(*plaintext, ROOT_KIND, child_kind, &private_metadata);
        if (!plaintext->empty()) OPENSSL_cleanse(plaintext->data(), plaintext->size());
        if (!private_metadata.empty()) OPENSSL_cleanse(private_metadata.data(), private_metadata.size());
        if (!children) return false;
        std::unordered_set<ChunkId, ChunkIdHash> seen{root_chunk_id};
        for (const auto& child : *children) {
            auto typed = child;
            typed.kind = child_kind;
            if (!EnumerateTreeNode(network_binding, content_key, typed, lookup, visit, 1, seen)) return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace cybou
