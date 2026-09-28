// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/encrypted_chunk_tree.h>

#include <cybou/canonical_cbor.h>

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

constexpr std::uint64_t TREE_SCHEMA{1};
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
    std::uint64_t bytes{0};
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

std::optional<std::uint64_t> AsUnsigned(const CborValue& value)
{
    if (const auto* number = std::get_if<std::uint64_t>(&value.value)) return *number;
    return std::nullopt;
}

CborValue EncodeChildren(const std::span<const ChildRef> children)
{
    CborValue::Array entries;
    entries.reserve(children.size());
    for (const auto& child : children) {
        entries.push_back(CborValue::ArrayValue({
            CborValue::Unsigned(child.kind),
            CborValue::Bytes(CborValue::ByteString{child.id.begin(), child.id.end()}),
            CborValue::Unsigned(child.bytes),
        }));
    }
    return CborValue::ArrayValue(std::move(entries));
}

CborValue MakeTreeMetadata(const std::uint64_t kind, const std::span<const ChildRef> children)
{
    std::uint64_t total{0};
    for (const auto& child : children) {
        if (child.bytes > std::numeric_limits<std::uint64_t>::max() - total) throw std::overflow_error{"tree byte count overflow"};
        total += child.bytes;
    }
    return CborValue::MapValue({
        {CborValue::Unsigned(0), CborValue::Unsigned(TREE_SCHEMA)},
        {CborValue::Unsigned(1), CborValue::Unsigned(kind)},
        {CborValue::Unsigned(2), CborValue::Unsigned(total)},
        {CborValue::Unsigned(3), EncodeChildren(children)},
    });
}

CborValue MakeRootMetadata(
    const std::span<const ChildRef> children,
    const std::span<const unsigned char> private_metadata)
{
    auto root = MakeTreeMetadata(ROOT_KIND, children);
    auto fields = std::get<CborValue::Map>(std::move(root.value));
    fields.emplace_back(CborValue::Unsigned(4),
        CborValue::Bytes(CborValue::ByteString{private_metadata.begin(), private_metadata.end()}));
    return CborValue::MapValue(std::move(fields));
}

std::optional<std::vector<ChildRef>> ParseMetadata(
    const std::span<const unsigned char> bytes,
    const std::uint64_t expected_kind,
    std::uint64_t& declared_total,
    std::vector<unsigned char>* root_private_metadata = nullptr)
{
    try {
        const auto value = DecodeCanonicalCbor(bytes);
        const auto* fields = std::get_if<CborValue::Map>(&value.value);
        const auto expected_fields = expected_kind == ROOT_KIND ? std::size_t{5} : std::size_t{4};
        if (fields == nullptr || fields->size() != expected_fields) return std::nullopt;
        for (std::size_t i = 0; i < fields->size(); ++i) {
            const auto key = AsUnsigned((*fields)[i].first);
            if (!key || *key != i) return std::nullopt;
        }
        if (expected_kind == ROOT_KIND) {
            const auto* metadata = std::get_if<CborValue::ByteString>(&(*fields)[4].second.value);
            if (metadata == nullptr || root_private_metadata == nullptr) return std::nullopt;
            *root_private_metadata = *metadata;
        }
        const auto schema = AsUnsigned((*fields)[0].second);
        const auto kind = AsUnsigned((*fields)[1].second);
        const auto total = AsUnsigned((*fields)[2].second);
        const auto* entries = std::get_if<CborValue::Array>(&(*fields)[3].second.value);
        if (!schema || *schema != TREE_SCHEMA || !kind || *kind != expected_kind || !total ||
            entries == nullptr || entries->empty() || entries->size() > ENCRYPTED_TREE_MAX_CHILDREN) return std::nullopt;

        std::vector<ChildRef> children;
        children.reserve(entries->size());
        std::uint64_t sum{0};
        std::optional<std::uint64_t> child_layer_kind;
        for (const auto& entry : *entries) {
            const auto* tuple = std::get_if<CborValue::Array>(&entry.value);
            if (tuple == nullptr || tuple->size() != 3) return std::nullopt;
            const auto child_kind = AsUnsigned((*tuple)[0]);
            const auto child_bytes = AsUnsigned((*tuple)[2]);
            const auto* id_bytes = std::get_if<CborValue::ByteString>(&(*tuple)[1].value);
            if (!child_kind || (*child_kind != INDEX_KIND && *child_kind != DATA_KIND) ||
                !child_bytes ||
                id_bytes == nullptr || id_bytes->size() != ChunkId{}.size() ||
                *child_bytes > std::numeric_limits<std::uint64_t>::max() - sum) return std::nullopt;
            if (child_layer_kind && *child_layer_kind != *child_kind) return std::nullopt;
            child_layer_kind = *child_kind;
            ChildRef child;
            child.kind = *child_kind;
            child.bytes = *child_bytes;
            std::copy(id_bytes->begin(), id_bytes->end(), child.id.begin());
            children.push_back(child);
            sum += *child_bytes;
        }
        if (sum != *total) return std::nullopt;
        declared_total = *total;
        return children;
    } catch (...) {
        return std::nullopt;
    }
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
        return Push(0, ChildRef{encrypted->id, static_cast<std::uint64_t>(plaintext.size()), DATA_KIND});
    }

    bool Finish(ChunkId& root_id)
    {
        for (std::size_t level = 0; level < m_levels.size(); ++level) {
            auto& pending = m_levels[level];
            if (pending.empty()) continue;
            const bool higher_pending = std::any_of(m_levels.begin() + level + 1, m_levels.end(),
                [](const auto& children) { return !children.empty(); });
            if (!higher_pending) {
                auto root_bytes = EncodeCanonicalCbor(MakeRootMetadata(pending, m_private_root_metadata));
                if (root_bytes.size() > ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES) {
                    if (!root_bytes.empty()) OPENSSL_cleanse(root_bytes.data(), root_bytes.size());
                    return false;
                }
                const auto root = EncryptChunk(m_network, m_key, root_bytes);
                if (!root_bytes.empty()) OPENSSL_cleanse(root_bytes.data(), root_bytes.size());
                if (!root || !Store(*root)) return false;
                root_id = root->id;
                const auto authorization = m_authorization.Finish();
                if (!authorization || authorization->chunk_count != m_summary.chunk_count ||
                    authorization->authorized_stored_bytes != m_summary.authorized_stored_bytes) return false;
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
        if (m_summary.chunk_count >= ENCRYPTED_TREE_MAX_CHUNKS ||
            !m_stage(static_cast<std::uint32_t>(m_summary.chunk_count), chunk) ||
            !m_authorization.Add(AuthorizedChunk{chunk.id, chunk.stored_bytes.size()}) ||
            !CheckedAdd(m_summary.chunk_count, 1) ||
            !CheckedAdd(m_summary.authorized_stored_bytes, chunk.stored_bytes.size())) return false;
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
        std::uint64_t total{0};
        for (const auto& child : m_levels[level]) {
            if (child.bytes > std::numeric_limits<std::uint64_t>::max() - total) return false;
            total += child.bytes;
        }
        const auto metadata = EncodeCanonicalCbor(MakeTreeMetadata(INDEX_KIND, m_levels[level]));
        if (metadata.size() > ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES) return false;
        const auto encrypted = EncryptChunk(m_network, m_key, metadata);
        if (!encrypted || !Store(*encrypted)) return false;
        m_levels[level].clear();
        return Push(level + 1, ChildRef{encrypted->id, total, INDEX_KIND});
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
        const bool valid = output_bytes <= max_output && plaintext->size() == node.bytes && node.bytes <= max_output - output_bytes &&
            sink(*plaintext);
        if (valid) output_bytes += node.bytes;
        if (!plaintext->empty()) OPENSSL_cleanse(plaintext->data(), plaintext->size());
        path.erase(node.id);
        return valid;
    }
    std::uint64_t total{0};
    const auto children = ParseMetadata(*plaintext, INDEX_KIND, total);
    if (!plaintext->empty()) OPENSSL_cleanse(plaintext->data(), plaintext->size());
    if (!children || total != node.bytes) { path.erase(node.id); return false; }
    for (const auto& child : *children) {
        if (child.kind == INDEX_KIND && !ReadTreeNode(network, key, child, lookup, visit, sink,
                max_output, depth + 1, path, output_bytes)) { path.erase(node.id); return false; }
        if (child.kind == DATA_KIND && !ReadTreeNode(network, key, child, lookup, visit, sink,
                max_output, depth + 1, path, output_bytes)) { path.erase(node.id); return false; }
    }
    path.erase(node.id);
    return true;
}

} // namespace

std::optional<EncryptedTreeSummary> BuildEncryptedChunkTree(
    const std::span<const unsigned char, 32> network_id,
    const EncryptedTreeSource& source,
    const EncryptedTreeStage& stage,
    const std::span<const unsigned char> private_root_metadata)
{
    if (!source || !stage || private_root_metadata.size() > ENCRYPTED_TREE_ROOT_PRIVATE_METADATA_MAX_BYTES) return std::nullopt;
    if (!private_root_metadata.empty()) {
        try { (void)DecodeCanonicalCbor(private_root_metadata); }
        catch (...) { return std::nullopt; }
    }
    auto key = GenerateContentKey();
    if (!key) return std::nullopt;
    CleanseOnExit cleanse_generated_key{*key};
    EncryptedTreeSummary summary;
    summary.content_key = *key;
    CleanseOnExit cleanse_summary_key{summary.content_key};
    try {
        TreeBuilder builder(network_id, summary.content_key, stage, summary, private_root_metadata);
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
            if (filled != 0 || summary.chunk_count == 0) {
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
    const std::span<const unsigned char, 32> network_id,
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
        auto root_plaintext = DecryptChunk(network_id, content_key, root_chunk_id, *stored_root);
        if (!root_plaintext) return std::nullopt;
        std::uint64_t declared_total{0};
        std::vector<unsigned char> app_metadata;
        const auto children = ParseMetadata(*root_plaintext, ROOT_KIND, declared_total, &app_metadata);
        if (!root_plaintext->empty()) OPENSSL_cleanse(root_plaintext->data(), root_plaintext->size());
        if (!children || declared_total > max_output_bytes) return std::nullopt;
        if (!app_metadata.empty()) {
            try { (void)DecodeCanonicalCbor(app_metadata); }
            catch (...) { return std::nullopt; }
        }
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
            if (!ReadTreeNode(network_id, content_key, child, lookup, visit, sink,
                    max_output_bytes, 1, path, written)) return std::nullopt;
        }
        if (written != declared_total) return std::nullopt;
        return written;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace cybou
