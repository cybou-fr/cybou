// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/chunk_authorization_proof_index.h>
#include <cybou/kv_store.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace cybou {
namespace {

constexpr unsigned char DISK_INDEX_VERSION{1};
constexpr size_t DISK_INDEX_STATE_SIZE{1 + 1 + 4 + 32};

struct DiskIndexState {
    std::uint32_t count{0};
    bool finished{false};
    ChunkId root{};
};

std::string HexBytes(const std::span<const unsigned char> bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(bytes.size() * 2, '0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        out[2 * i] = digits[bytes[i] >> 4];
        out[2 * i + 1] = digits[bytes[i] & 0x0f];
    }
    return out;
}

std::string HexIndex(const std::uint64_t value)
{
    std::array<unsigned char, 8> bytes{};
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<unsigned char>(value >> (8 * i));
    return HexBytes(bytes);
}

std::optional<DiskIndexState> DecodeDiskIndexState(const std::span<const unsigned char> bytes)
{
    if (bytes.size() != DISK_INDEX_STATE_SIZE || bytes[0] != DISK_INDEX_VERSION || bytes[1] > 1) {
        return std::nullopt;
    }
    DiskIndexState state;
    for (size_t i = 0; i < 4; ++i) state.count |= uint32_t{bytes[2 + i]} << (8 * i);
    state.finished = bytes[1] == 1;
    std::copy_n(bytes.begin() + 6, state.root.size(), state.root.begin());
    if ((!state.finished && std::any_of(state.root.begin(), state.root.end(), [](unsigned char b) { return b != 0; })) ||
        (state.finished && (state.count == 0 ||
            std::all_of(state.root.begin(), state.root.end(), [](unsigned char b) { return b == 0; })))) return std::nullopt;
    return state;
}

bool IsZero(const ChunkId& id)
{
    return std::all_of(id.begin(), id.end(), [](const auto byte) { return byte == 0; });
}

bool ValidIndexNamespace(const std::string_view value)
{
    if (value.empty() || value.size() > 64) return false;
    return std::all_of(value.begin(), value.end(), [](const char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-';
    });
}

} // namespace

ChunkAuthorizationProofIndex::ChunkAuthorizationProofIndex(KVStore& db, std::string index_id)
    : m_db{db}, m_prefix{"chunk-auth-proof-index/" + index_id + "/"}
{
    if (!ValidIndexNamespace(index_id)) throw std::invalid_argument{"invalid chunk authorization index namespace"};
    std::vector<unsigned char> state_bytes;
    const auto state_key = m_prefix + "state";
    if (m_db.Read(state_key, state_bytes)) {
        const auto state = DecodeDiskIndexState(state_bytes);
        if (!state) throw std::runtime_error{"corrupt chunk authorization proof index state"};
        m_chunk_count = state->count;
        m_finished = state->finished;
        m_root = state->root;
        if (m_finished) {
            auto width = static_cast<uint64_t>(m_chunk_count);
            uint32_t level{0};
            while (width > 1) { width = (width + 1) / 2; ++level; }
            ChunkId stored_root;
            if (!ReadNode(level, 0, stored_root) || stored_root != m_root) {
                throw std::runtime_error{"chunk authorization proof index root mismatch"};
            }
        }
    } else if (m_db.Exists(state_key)) {
        throw std::runtime_error{"corrupt chunk authorization proof index state"};
    } else {
        m_db.Write(state_key, EncodeState(0, false, {}), true);
    }
}

bool ChunkAuthorizationProofIndex::Add(const std::uint32_t leaf_index, const AuthorizedChunk& chunk)
{
    std::lock_guard lock{m_mutex};
    if (m_failed || m_discarded || m_finished || leaf_index != m_chunk_count ||
        m_chunk_count >= ROOT_PUBLICATION_MAX_CHUNKS || IsZero(chunk.id)) return false;
    try {
        if (m_db.Exists(SeenKey(chunk.id))) return false;
        KVStore::Batch batch;
        const std::vector<unsigned char> id_bytes{chunk.id.begin(), chunk.id.end()};
        batch.Write(LeafKey(leaf_index), id_bytes);
        batch.Write(SeenKey(chunk.id), uint8_t{1});
        batch.Write(m_prefix + "state", EncodeState(m_chunk_count + 1, false, {}));
        m_db.WriteBatch(batch, true);
        ++m_chunk_count;
        return true;
    } catch (...) {
        m_failed = true;
        return false;
    }
}

std::optional<ChunkAuthorizationSummary> ChunkAuthorizationProofIndex::Finish()
{
    std::lock_guard lock{m_mutex};
    if (m_failed || m_discarded || m_chunk_count == 0) return std::nullopt;
    if (m_finished) return ChunkAuthorizationSummary{m_root, m_chunk_count};

    try {
        constexpr size_t WRITE_BATCH_NODES{256};
        KVStore::Batch batch;
        size_t pending{0};
        const auto write_node = [&](const std::uint32_t level, const std::uint64_t index,
                                    const ChunkId& hash, KVStore::Batch& write_batch,
                                    size_t& pending_count) {
            const std::vector<unsigned char> bytes{hash.begin(), hash.end()};
            write_batch.Write(NodeKey(level, index), bytes);
            ++pending_count;
            if (pending_count == WRITE_BATCH_NODES) {
                m_db.WriteBatch(write_batch, false);
                write_batch = KVStore::Batch{};
                pending_count = 0;
            }
        };

        for (uint64_t index = 0; index < m_chunk_count; ++index) {
            ChunkId id;
            if (!ReadLeaf(static_cast<uint32_t>(index), id)) throw std::runtime_error{"missing chunk authorization leaf"};
            write_node(0, index, ChunkAuthorizationLeafHash(id), batch, pending);
        }
        if (pending != 0) m_db.WriteBatch(batch, false);

        uint64_t width = m_chunk_count;
        uint32_t level{0};
        while (width > 1) {
            KVStore::Batch next_batch;
            pending = 0;
            const uint64_t next_width = (width + 1) / 2;
            for (uint64_t index = 0; index < next_width; ++index) {
                ChunkId left;
                if (!ReadNode(level, 2 * index, left)) throw std::runtime_error{"missing chunk authorization node"};
                ChunkId right = left;
                if (2 * index + 1 < width && !ReadNode(level, 2 * index + 1, right)) {
                    throw std::runtime_error{"missing chunk authorization sibling"};
                }
                write_node(level + 1, index, ChunkAuthorizationNodeHash(left, right), next_batch, pending);
            }
            if (pending != 0) m_db.WriteBatch(next_batch, false);
            width = next_width;
            ++level;
        }

        if (!ReadNode(level, 0, m_root) || IsZero(m_root)) throw std::runtime_error{"missing chunk authorization root"};
        m_db.Write(m_prefix + "state", EncodeState(m_chunk_count, true, m_root), true);
        m_finished = true;
        return ChunkAuthorizationSummary{m_root, m_chunk_count};
    } catch (...) {
        m_failed = true;
        return std::nullopt;
    }
}

std::optional<ChunkAuthorizationProof> ChunkAuthorizationProofIndex::GetProof(
    const std::uint32_t leaf_index) const
{
    std::lock_guard lock{m_mutex};
    if (m_failed || m_discarded || !m_finished || leaf_index >= m_chunk_count) return std::nullopt;
    try {
        ChunkAuthorizationProof proof;
        proof.leaf_index = leaf_index;
        auto index = static_cast<uint64_t>(leaf_index);
        auto width = static_cast<uint64_t>(m_chunk_count);
        uint32_t level{0};
        while (width > 1) {
            const auto sibling_index = (index & 1U) != 0 ? index - 1 :
                (index + 1 < width ? index + 1 : index);
            ChunkId sibling;
            if (!ReadNode(level, sibling_index, sibling)) return std::nullopt;
            proof.siblings.push_back(sibling);
            index /= 2;
            width = (width + 1) / 2;
            ++level;
        }
        return proof;
    } catch (...) {
        return std::nullopt;
    }
}

bool ChunkAuthorizationProofIndex::Discard()
{
    std::lock_guard lock{m_mutex};
    if (m_failed || m_discarded) return false;
    try {
        constexpr size_t ERASE_BATCH_KEYS{256};
        KVStore::Batch batch;
        size_t pending{0};
        const auto erase = [&](const std::string& key) {
            batch.Erase(key);
            ++pending;
            if (pending == ERASE_BATCH_KEYS) {
                m_db.WriteBatch(batch, false);
                batch = KVStore::Batch{};
                pending = 0;
            }
        };
        for (uint64_t index = 0; index < m_chunk_count; ++index) {
            ChunkId id;
            if (ReadLeaf(static_cast<uint32_t>(index), id)) {
                erase(LeafKey(static_cast<uint32_t>(index)));
                erase(SeenKey(id));
            }
        }
        auto width = static_cast<uint64_t>(m_chunk_count);
        uint32_t level{0};
        while (width != 0) {
            for (uint64_t index = 0; index < width; ++index) erase(NodeKey(level, index));
            if (width == 1) break;
            width = (width + 1) / 2;
            ++level;
        }
        erase(m_prefix + "state");
        if (pending != 0) m_db.WriteBatch(batch, true);
        m_discarded = true;
        return true;
    } catch (...) {
        m_failed = true;
        return false;
    }
}

std::string ChunkAuthorizationProofIndex::LeafKey(const std::uint32_t index) const
{
    return m_prefix + "leaf/" + HexIndex(index);
}

std::string ChunkAuthorizationProofIndex::SeenKey(const ChunkId& id) const
{
    return m_prefix + "seen/" + HexBytes(id);
}

std::string ChunkAuthorizationProofIndex::NodeKey(const std::uint32_t level, const std::uint64_t index) const
{
    return m_prefix + "node/" + HexIndex(level) + "/" + HexIndex(index);
}

bool ChunkAuthorizationProofIndex::ReadLeaf(const std::uint32_t index, ChunkId& id) const
{
    std::vector<unsigned char> bytes;
    if (!m_db.Read(LeafKey(index), bytes) || bytes.size() != id.size()) return false;
    std::copy(bytes.begin(), bytes.end(), id.begin());
    return !IsZero(id);
}

bool ChunkAuthorizationProofIndex::ReadNode(
    const std::uint32_t level, const std::uint64_t index, ChunkId& hash) const
{
    std::vector<unsigned char> bytes;
    if (!m_db.Read(NodeKey(level, index), bytes) || bytes.size() != hash.size()) return false;
    std::copy(bytes.begin(), bytes.end(), hash.begin());
    return !IsZero(hash);
}

std::vector<unsigned char> ChunkAuthorizationProofIndex::EncodeState(
    const std::uint32_t count, const bool finished, const ChunkId& root) const
{
    std::vector<unsigned char> bytes;
    bytes.reserve(DISK_INDEX_STATE_SIZE);
    bytes.push_back(DISK_INDEX_VERSION);
    bytes.push_back(finished ? 1 : 0);
    for (size_t i = 0; i < 4; ++i) bytes.push_back(static_cast<unsigned char>(count >> (8 * i)));
    bytes.insert(bytes.end(), root.begin(), root.end());
    return bytes;
}

} // namespace cybou
