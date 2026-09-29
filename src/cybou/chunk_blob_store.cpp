// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/chunk_blob_store.h>
#include <cybou/encrypted_chunk.h>

#include <algorithm>
#include <climits>
#include <fstream>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace cybou {
namespace {

constexpr char HEX[] = "0123456789abcdef";

std::string Hex(const std::span<const unsigned char> bytes)
{
    std::string result(bytes.size() * 2, '\0');
    for (std::size_t i{0}; i < bytes.size(); ++i) {
        result[2 * i] = HEX[bytes[i] >> 4];
        result[2 * i + 1] = HEX[bytes[i] & 0x0f];
    }
    return result;
}

std::optional<ChunkId> ParseChunkId(const std::string& hex)
{
    if (hex.size() != 64) return std::nullopt;
    ChunkId id{};
    for (std::size_t i{0}; i < id.size(); ++i) {
        const auto digit = [](const char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        const int high = digit(hex[2 * i]);
        const int low = digit(hex[2 * i + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        id[i] = static_cast<unsigned char>((high << 4) | low);
    }
    return id;
}

std::filesystem::path BlobPath(const std::filesystem::path& root, const ChunkId& id)
{
    const auto hex = Hex(id);
    return root / hex.substr(0, 2) / hex.substr(2, 2) / hex;
}

std::optional<std::vector<unsigned char>> ReadBlob(const std::filesystem::path& path, const ChunkId& id)
{
    try {
        if (std::filesystem::is_symlink(path)) return std::nullopt;
        const auto size = std::filesystem::file_size(path);
        if (size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES) {
            return std::nullopt;
        }
        std::ifstream input(path, std::ios::binary);
        if (!input) return std::nullopt;
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input || input.peek() != std::char_traits<char>::eof() || ComputeChunkId(bytes) != id) {
            return std::nullopt;
        }
        return bytes;
    } catch (...) {
        return std::nullopt;
    }
}

bool WriteAll(const int fd, const std::span<const unsigned char> bytes)
{
    std::size_t offset{0};
    while (offset < bytes.size()) {
#ifdef _WIN32
        const auto remaining = static_cast<unsigned>(std::min<std::size_t>(bytes.size() - offset, INT_MAX));
        const auto written = _write(fd, bytes.data() + offset, remaining);
#else
        const auto written = ::write(fd, bytes.data() + offset, bytes.size() - offset);
#endif
        if (written <= 0) return false;
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

bool SyncDirectory(const std::filesystem::path& directory)
{
#ifdef _WIN32
    (void)directory;
    return true;
#else
    const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) return false;
    const bool synced = ::fsync(fd) == 0;
    ::close(fd);
    return synced;
#endif
}

/** replace: overwrite a damaged existing blob (POSIX rename always replaces). */
bool WriteBlobAtomically(const std::filesystem::path& path, const ChunkId& id,
    const std::span<const unsigned char> bytes, const bool replace = false)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    const auto temp = path.parent_path() / (Hex(id) + ".tmp");
#ifdef _WIN32
    const int fd = _wopen(temp.c_str(), _O_BINARY | _O_WRONLY | _O_CREAT | _O_EXCL,
        _S_IREAD | _S_IWRITE);
#else
    const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
    if (fd < 0) return false;
    const bool flushed = WriteAll(fd, bytes)
#ifdef _WIN32
        && _commit(fd) == 0
#else
        && ::fsync(fd) == 0
#endif
        ;
#ifdef _WIN32
    const bool closed = _close(fd) == 0;
#else
    const bool closed = ::close(fd) == 0;
#endif
    if (!flushed || !closed) {
        std::filesystem::remove(temp, ec);
        return false;
    }
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), path.c_str(),
            MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0))) {
        std::filesystem::remove(temp, ec);
        return false;
    }
#else
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    if (!SyncDirectory(path.parent_path())) return false;
#endif
    return true;
}

std::uint64_t ScanBlobs(const std::filesystem::path& root)
{
    if (!std::filesystem::exists(root)) return 0;
    if (std::filesystem::is_symlink(root)) throw std::runtime_error{"chunk blob root is a symlink"};
    std::uint64_t total{0};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_symlink()) throw std::runtime_error{"chunk blob path is a symlink"};
        if (entry.is_directory()) continue;
        if (!entry.is_regular_file()) throw std::runtime_error{"invalid chunk blob path"};
        const auto filename = entry.path().filename().string();
        if (filename.ends_with(".tmp")) {
            std::filesystem::remove(entry.path());
            continue;
        }
        const auto id = ParseChunkId(filename);
        if (!id || BlobPath(root, *id) != entry.path()) {
            throw std::runtime_error{"invalid chunk blob filename"};
        }
        const auto size = entry.file_size();
        if (size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
            size > std::numeric_limits<std::uint64_t>::max() - total) {
            throw std::runtime_error{"invalid chunk blob size"};
        }
        total += size;
    }
    return total;
}

} // namespace

ChunkBlobStore::ChunkBlobStore(std::filesystem::path root, const bool memory_only, const bool wipe_data)
    : m_root{memory_only ? std::filesystem::path{} : std::move(root)}, m_memory_only{memory_only}
{
    if (m_memory_only) return;
    if (m_root.empty()) throw std::invalid_argument{"chunk blob root is empty"};
    if (wipe_data) {
        std::error_code ec;
        const auto absolute = std::filesystem::absolute(m_root, ec).lexically_normal();
        if (ec || absolute == absolute.root_path() ||
            (std::filesystem::exists(m_root) && std::filesystem::is_symlink(m_root))) {
            throw std::runtime_error{"refusing to wipe an unsafe chunk blob root"};
        }
        std::filesystem::remove_all(m_root, ec);
        if (ec) throw std::runtime_error{"cannot wipe chunk blob root: " + ec.message()};
    }
    m_used_bytes = ScanBlobs(m_root);
}

ChunkBlobPutStatus ChunkBlobStore::Put(const ChunkId& id, const std::span<const unsigned char> stored_bytes)
{
    if (id == ChunkId{} || stored_bytes.size() < ENCRYPTED_CHUNK_MIN_STORED_BYTES ||
        stored_bytes.size() > ENCRYPTED_CHUNK_MAX_STORED_BYTES || ComputeChunkId(stored_bytes) != id) {
        return ChunkBlobPutStatus::INVALID;
    }
    std::lock_guard lock{m_mutex};
    if (m_memory_only) {
        const auto existing = m_memory_blobs.find(id);
        if (existing != m_memory_blobs.end()) {
            return existing->second == std::vector<unsigned char>{stored_bytes.begin(), stored_bytes.end()} ?
                ChunkBlobPutStatus::ALREADY_STORED : ChunkBlobPutStatus::CONFLICT;
        }
        if (stored_bytes.size() > std::numeric_limits<std::uint64_t>::max() - m_used_bytes) {
            return ChunkBlobPutStatus::STORAGE_ERROR;
        }
        m_memory_blobs.emplace(id, std::vector<unsigned char>{stored_bytes.begin(), stored_bytes.end()});
        m_used_bytes += stored_bytes.size();
        return ChunkBlobPutStatus::STORED;
    }
    try {
        const auto path = BlobPath(m_root, id);
        if (std::filesystem::exists(path)) {
            if (const auto existing = ReadBlob(path, id)) {
                if (!std::equal(existing->begin(), existing->end(), stored_bytes.begin(), stored_bytes.end())) {
                    return ChunkBlobPutStatus::CONFLICT;
                }
                return SyncDirectory(path.parent_path()) ? ChunkBlobPutStatus::ALREADY_STORED :
                    ChunkBlobPutStatus::STORAGE_ERROR;
            }
            // Damaged on disk (bit rot, truncation): the bytes offered hash to
            // the ChunkID, so replace the file with them.
            if (std::filesystem::is_symlink(path) || !std::filesystem::is_regular_file(path)) {
                return ChunkBlobPutStatus::STORAGE_ERROR;
            }
            const auto damaged_size = std::filesystem::file_size(path);
            if (!WriteBlobAtomically(path, id, stored_bytes, /*replace=*/true)) return ChunkBlobPutStatus::STORAGE_ERROR;
            m_used_bytes = m_used_bytes - std::min<std::uint64_t>(m_used_bytes, damaged_size) + stored_bytes.size();
            return ChunkBlobPutStatus::ALREADY_STORED;
        }
        if (stored_bytes.size() > std::numeric_limits<std::uint64_t>::max() - m_used_bytes) {
            return ChunkBlobPutStatus::STORAGE_ERROR;
        }
        if (!WriteBlobAtomically(path, id, stored_bytes)) {
            // A failed directory sync can leave a valid file behind. Count it
            // for physical capacity, but keep reporting uncertain durability.
            m_used_bytes = ScanBlobs(m_root);
            return ChunkBlobPutStatus::STORAGE_ERROR;
        }
        m_used_bytes += stored_bytes.size();
        return ChunkBlobPutStatus::STORED;
    } catch (...) {
        return ChunkBlobPutStatus::STORAGE_ERROR;
    }
}

std::optional<std::vector<unsigned char>> ChunkBlobStore::Get(const ChunkId& id) const
{
    if (id == ChunkId{}) return std::nullopt;
    std::lock_guard lock{m_mutex};
    if (m_memory_only) {
        const auto it = m_memory_blobs.find(id);
        if (it == m_memory_blobs.end() || ComputeChunkId(it->second) != id) return std::nullopt;
        return it->second;
    }
    return ReadBlob(BlobPath(m_root, id), id);
}

std::optional<std::uint64_t> ChunkBlobStore::StoredSize(const ChunkId& id) const
{
    if (id == ChunkId{}) return std::nullopt;
    std::lock_guard lock{m_mutex};
    if (m_memory_only) {
        const auto it = m_memory_blobs.find(id);
        return it == m_memory_blobs.end() ? std::nullopt : std::optional<std::uint64_t>{it->second.size()};
    }
    try {
        const auto path = BlobPath(m_root, id);
        const auto status = std::filesystem::symlink_status(path);
        if (!std::filesystem::is_regular_file(status)) return std::nullopt;
        const auto size = std::filesystem::file_size(path);
        if (size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES) return std::nullopt;
        return size;
    } catch (...) {
        return std::nullopt;
    }
}

bool ChunkBlobStore::Has(const ChunkId& id) const
{
    return StoredSize(id).has_value();
}

bool ChunkBlobStore::Remove(const ChunkId& id)
{
    if (id == ChunkId{}) return false;
    std::lock_guard lock{m_mutex};
    if (m_memory_only) {
        const auto it = m_memory_blobs.find(id);
        if (it == m_memory_blobs.end()) return false;
        m_used_bytes -= it->second.size();
        m_memory_blobs.erase(it);
        return true;
    }
    try {
        const auto path = BlobPath(m_root, id);
        if (!std::filesystem::exists(path) || std::filesystem::is_symlink(path) ||
            !std::filesystem::is_regular_file(path)) return false;
        const auto size = std::filesystem::file_size(path);
        if (size > m_used_bytes || !std::filesystem::remove(path)) return false;
        m_used_bytes -= size;
        return true;
    } catch (...) {
        return false;
    }
}

std::uint64_t ChunkBlobStore::UsedBytes() const
{
    std::lock_guard lock{m_mutex};
    return m_used_bytes;
}

} // namespace cybou
