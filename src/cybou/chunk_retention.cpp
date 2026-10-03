// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Реализация локального pin/cache-реестра для общего ChunkStore.

#include <cybou/chunk_retention.h>

#include <cybou/chunk_blob_store.h>
#include <cybou/kv_store.h>

#include <algorithm>
#include <string>
#include <tuple>

namespace cybou {
namespace {

constexpr std::string_view PIN{"pin/"};   // pin/<chunk><holder><reference>
constexpr std::string_view REF{"ref/"};   // ref/<holder><reference><chunk>
constexpr std::string_view CACHE{"cch/"}; // cch/<chunk> -> last_use_ms
constexpr std::size_t HEX{64};
constexpr std::size_t PIN_KEY{4 + 3 * HEX};
constexpr std::size_t CACHE_KEY{4 + HEX};

std::span<const unsigned char> Bytes(const std::string_view text)
{
    return {reinterpret_cast<const unsigned char*>(text.data()), text.size()};
}

std::string Hex(std::span<const unsigned char> bytes)
{
    static constexpr char DIGITS[]{"0123456789abcdef"};
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        out.push_back(DIGITS[byte >> 4]);
        out.push_back(DIGITS[byte & 0x0f]);
    }
    return out;
}

std::optional<ChunkId> ParseHex(std::string_view hex)
{
    if (hex.size() != HEX) return std::nullopt;
    ChunkId id{};
    for (std::size_t i{0}; i < id.size(); ++i) {
        const auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        const int high = nibble(hex[2 * i]);
        const int low = nibble(hex[2 * i + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        id[i] = static_cast<unsigned char>((high << 4) | low);
    }
    return id;
}

std::string KeyString(const RetentionKey& key) { return Hex(key.holder) + Hex(key.reference); }

} // namespace

std::array<unsigned char, 32> RetentionTag(const std::string_view domain, const std::span<const unsigned char> bytes)
{
    static constexpr unsigned char separator{0};
    const std::array parts{Bytes(domain), std::span<const unsigned char>{&separator, 1}, bytes};
    return ComputeBlake3Digest(parts);
}

ChunkRetentionRegistry::ChunkRetentionRegistry(const std::filesystem::path& path, const bool memory_only,
    const bool wipe_data)
    : m_db{std::make_unique<KVStore>(KVStoreOptions{.path = path, .memory_only = memory_only, .wipe_data = wipe_data})}
{
}

ChunkRetentionRegistry::~ChunkRetentionRegistry() = default;

bool ChunkRetentionRegistry::Pin(const RetentionKey& key, const std::span<const ChunkId> chunks)
{
    std::lock_guard lock{m_mutex};
    try {
        KVStore::Batch batch;
        const auto owner = KeyString(key);
        for (const auto& chunk : chunks) {
            const auto id = Hex(chunk);
            batch.Write(std::string{PIN} + id + owner, std::string{});
            batch.Write(std::string{REF} + owner + id, std::string{});
        }
        m_db->WriteBatch(batch, true);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ChunkRetentionRegistry::Release(const RetentionKey& key, const std::uint64_t now_ms)
{
    std::lock_guard lock{m_mutex};
    try {
        const auto owner = KeyString(key);
        std::vector<std::string> chunks;
        m_db->ForEachStringPrefix(std::string{REF} + owner, PIN_KEY,
            [&](const std::string& k, const std::string&) { chunks.push_back(k.substr(4 + 2 * HEX)); });
        KVStore::Batch batch;
        for (const auto& id : chunks) {
            batch.Erase(std::string{PIN} + id + owner);
            batch.Erase(std::string{REF} + owner + id);
            // После release локальный blob ещё полезен и остаётся в cache до реальной нехватки места.
            batch.Write(std::string{CACHE} + id, now_ms);
        }
        m_db->WriteBatch(batch, true);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ChunkRetentionRegistry::IsPinnedLocked(const ChunkId& chunk) const
{
    bool pinned{false};
    m_db->ForEachStringPrefix(std::string{PIN} + Hex(chunk), PIN_KEY,
        [&](const std::string&, const std::string&) { pinned = true; });
    return pinned;
}

bool ChunkRetentionRegistry::IsPinned(const ChunkId& chunk) const
{
    std::lock_guard lock{m_mutex};
    try {
        return IsPinnedLocked(chunk);
    } catch (const std::exception&) {
        return true; // unknown means keep
    }
}

std::vector<ChunkId> ChunkRetentionRegistry::Pinned(const RetentionKey& key) const
{
    std::lock_guard lock{m_mutex};
    std::vector<ChunkId> out;
    m_db->ForEachStringPrefix(std::string{REF} + KeyString(key), PIN_KEY, [&](const std::string& k, const std::string&) {
        if (const auto id = ParseHex(std::string_view{k}.substr(4 + 2 * HEX))) out.push_back(*id);
    });
    return out;
}

bool ChunkRetentionRegistry::NoteCacheUse(const ChunkId& chunk, const std::uint64_t now_ms)
{
    std::lock_guard lock{m_mutex};
    try {
        m_db->Write(std::string{CACHE} + Hex(chunk), now_ms);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

ChunkRetentionRegistry::CollectResult ChunkRetentionRegistry::Collect(const ChunkBlobStore& blobs,
    const std::uint64_t cache_budget_bytes, const std::uint64_t now_ms, const std::uint64_t grace_ms,
    const std::size_t max_removals, const std::function<bool(const ChunkId&)>& remove)
{
    std::lock_guard lock{m_mutex};
    CollectResult result;
    try {
        std::vector<std::pair<ChunkId, std::uint64_t>> entries;
        m_db->ForEachStringPrefixRaw(std::string{CACHE}, CACHE_KEY, [&](const std::string& k, const std::string& v) {
            const auto id = ParseHex(std::string_view{k}.substr(4));
            std::uint64_t used{0};
            for (std::size_t i{0}; i < v.size() && i < 8; ++i) {
                used |= std::uint64_t{static_cast<unsigned char>(v[i])} << (8 * i);
            }
            if (id) entries.emplace_back(*id, used);
        });
        // Вытесняем только cache-записи с реально существующим blob без pin.
        std::vector<std::tuple<std::uint64_t, ChunkId, std::uint64_t>> evictable;
        KVStore::Batch forget;
        for (const auto& [id, used] : entries) {
            const auto size = blobs.StoredSize(id);
            if (!size) {
                forget.Erase(std::string{CACHE} + Hex(id));
                continue;
            }
            if (IsPinnedLocked(id)) continue;
            result.cache_bytes += *size;
            evictable.emplace_back(used, id, *size);
        }
        std::sort(evictable.begin(), evictable.end());
        for (const auto& [used, id, size] : evictable) {
            if (result.cache_bytes <= cache_budget_bytes || result.removed >= max_removals) break;
            if (used + grace_ms > now_ms) break; // Дальше идут только более свежие записи.
            if (!remove(id)) continue;           // Blob удержан provider-обязательством.
            forget.Erase(std::string{CACHE} + Hex(id));
            result.cache_bytes -= size;
            result.removed_bytes += size;
            ++result.removed;
        }
        m_db->WriteBatch(forget, true);
    } catch (const std::exception&) {
    }
    return result;
}

} // namespace cybou
