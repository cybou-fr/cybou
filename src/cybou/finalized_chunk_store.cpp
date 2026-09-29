// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/finalized_chunk_store.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#define NOMINMAX
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
    std::string out;
    out.resize(bytes.size() * 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out[i * 2] = HEX[bytes[i] >> 4];
        out[i * 2 + 1] = HEX[bytes[i] & 0x0f];
    }
    return out;
}

std::string ChunkKey(const std::string& name_space, const ChunkId& id)
{
    return name_space + "/chunk/" + Hex(id);
}

std::filesystem::path BlobPath(const std::filesystem::path& root, const ChunkId& id)
{
    const auto hex = Hex(id);
    return root / hex.substr(0, 2) / hex.substr(2, 2) / hex;
}

std::optional<ChunkId> ParseChunkId(const std::string& hex)
{
    if (hex.size() != 64) return std::nullopt;
    ChunkId id{};
    for (std::size_t i = 0; i < id.size(); ++i) {
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

std::optional<std::vector<unsigned char>> ReadBlob(
    const std::filesystem::path& path, const ChunkId& expected_id, const std::uint64_t expected_size = 0)
{
    try {
        const auto size = std::filesystem::file_size(path);
        if (size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
            (expected_size != 0 && size != expected_size)) return std::nullopt;
        std::ifstream input(path, std::ios::binary);
        if (!input) return std::nullopt;
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input || input.peek() != std::char_traits<char>::eof() || ComputeChunkId(bytes) != expected_id) {
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

bool WriteBlobAtomically(const std::filesystem::path& path, const ChunkId& id,
    const std::span<const unsigned char> bytes)
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
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temp, ec);
        return false;
    }
#else
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    const int directory_fd = ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
    if (directory_fd < 0) return false;
    const bool directory_synced = ::fsync(directory_fd) == 0;
    ::close(directory_fd);
    if (!directory_synced) return false;
#endif
    return true;
}

std::optional<std::uint64_t> ReconcileBlobStore(
    KVStore& db, const std::string& name_space, const std::filesystem::path& root)
{
    try {
        const auto chunk_prefix = name_space + "/chunk/";
        const auto key_size = chunk_prefix.size() + 64;
        std::uint64_t total{0};
        db.ForEachStringPrefixRaw(chunk_prefix, key_size, [&](const std::string& key, const std::string& raw_size) {
            if (key.size() != key_size || raw_size.size() != sizeof(std::uint64_t)) {
                throw std::runtime_error{"corrupt finalized chunk metadata"};
            }
            const auto id = ParseChunkId(key.substr(chunk_prefix.size()));
            if (!id) throw std::runtime_error{"corrupt finalized chunk metadata key"};
            std::uint64_t size{0};
            const auto* value_begin = reinterpret_cast<const std::byte*>(raw_size.data());
            SpanReader value_reader{std::span<const std::byte>{value_begin, raw_size.size()}};
            value_reader >> size;
            if (size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
                size > std::numeric_limits<std::uint64_t>::max() - total) {
                throw std::runtime_error{"invalid finalized chunk size metadata"};
            }
            total += size;
        });

        if (!std::filesystem::exists(root)) return total;
        if (std::filesystem::is_symlink(root)) return std::nullopt;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (entry.is_symlink()) return std::nullopt;
            if (entry.is_directory()) continue;
            if (!entry.is_regular_file()) return std::nullopt;
            const auto filename = entry.path().filename().string();
            if (filename.ends_with(".tmp")) {
                std::error_code ec;
                std::filesystem::remove(entry.path(), ec);
                if (ec) return std::nullopt;
                continue;
            }
            const auto id = ParseChunkId(filename);
            if (!id || BlobPath(root, *id) != entry.path()) return std::nullopt;
            if (!db.Exists(ChunkKey(name_space, *id))) {
                std::error_code ec;
                std::filesystem::remove(entry.path(), ec);
                if (ec) return std::nullopt;
                continue;
            }
            const auto size = entry.file_size();
            std::uint64_t expected_size{0};
            if (!db.Read(ChunkKey(name_space, *id), expected_size) || size != expected_size) return std::nullopt;
            if (size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES) {
                return std::nullopt;
            }
        }
        return total;
    } catch (...) {
        return std::nullopt;
    }
}

std::string PublicationChunkKey(const std::string& name_space, const uint256& publication_id, const ChunkId& id)
{
    return name_space + "/publication-chunk/" + publication_id.GetHex() + "/" + Hex(id);
}

std::vector<unsigned char> EncodeProofMetadata(const ChunkAuthorizationProof& proof)
{
    std::vector<unsigned char> encoded;
    encoded.reserve(8 + proof.siblings.size() * ChunkId{}.size());
    const auto append_u32 = [&encoded](const std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) encoded.push_back(static_cast<unsigned char>(value >> shift));
    };
    append_u32(proof.leaf_index);
    append_u32(static_cast<std::uint32_t>(proof.siblings.size()));
    for (const auto& sibling : proof.siblings) encoded.insert(encoded.end(), sibling.begin(), sibling.end());
    return encoded;
}

} // namespace

FinalizedChunkStore::FinalizedChunkStore(const std::filesystem::path& path,
    const std::span<const unsigned char, 32> network_id, const std::uint64_t capacity_bytes,
    const bool memory_only, const bool wipe_data)
    : m_namespace{"chunk-store/v4/" + Hex(network_id)},
      m_blob_root{memory_only ? std::filesystem::path{} : path / "chunks"},
      m_memory_only{memory_only}, m_capacity_bytes{capacity_bytes}
{
    if (capacity_bytes == 0 || std::all_of(network_id.begin(), network_id.end(), [](const auto byte) { return byte == 0; }) ||
        (!memory_only && path.empty())) {
        throw std::invalid_argument{"invalid finalized chunk store configuration"};
    }
    const auto metadata_path = memory_only ? std::filesystem::path{} : path / "metadata";
    if (!memory_only && wipe_data) {
        std::error_code ec;
        const auto absolute_base = std::filesystem::absolute(path, ec).lexically_normal();
        if (ec || absolute_base == absolute_base.root_path()) {
            throw std::runtime_error{"refusing to wipe finalized chunk store at an unsafe path"};
        }
        const auto canonical_base = std::filesystem::weakly_canonical(absolute_base, ec);
        if (ec || canonical_base == canonical_base.root_path()) {
            throw std::runtime_error{"refusing to wipe finalized chunk store at an unsafe path"};
        }
        const auto absolute_blob_root = std::filesystem::absolute(m_blob_root, ec).lexically_normal();
        if (ec || absolute_blob_root == absolute_blob_root.root_path()) {
            throw std::runtime_error{"refusing to wipe finalized chunk blobs at an unsafe path"};
        }
        if (std::filesystem::exists(m_blob_root, ec) && !ec && !std::filesystem::is_symlink(m_blob_root, ec)) {
            const auto canonical_blob_root = std::filesystem::weakly_canonical(m_blob_root, ec);
            if (ec || canonical_blob_root == canonical_blob_root.root_path()) {
                throw std::runtime_error{"refusing to wipe finalized chunk blobs at an unsafe path"};
            }
        }
        std::filesystem::remove_all(m_blob_root, ec);
        if (ec) throw std::runtime_error{"cannot wipe finalized chunk blob store: " + ec.message()};
    }
    m_db = std::make_unique<KVStore>(KVStoreOptions{
        .path = metadata_path,
        .cache_bytes = 8 << 20,
        .memory_only = memory_only,
        .wipe_data = wipe_data,
    });

    // The path is operator-configured and can accidentally be reused across networks.
    // Refuse that configuration rather than mixing provider data or accounting.
    const std::string network_key{"chunk-store/v4/network-id"};
    std::vector<unsigned char> saved_network_id;
    if (m_db->Read(network_key, saved_network_id)) {
        if (!std::equal(saved_network_id.begin(), saved_network_id.end(), network_id.begin(), network_id.end())) {
            throw std::invalid_argument{"finalized chunk store network ID mismatch"};
        }
    } else {
        if (m_db->Exists(network_key)) throw std::runtime_error{"corrupt finalized chunk store network ID"};
        m_db->Write(network_key, std::vector<unsigned char>{network_id.begin(), network_id.end()}, true);
    }

    if (!memory_only) {
        const auto used = ReconcileBlobStore(*m_db, m_namespace, m_blob_root);
        if (!used || *used > capacity_bytes) throw std::runtime_error{"invalid finalized chunk blob store usage"};
        m_db->Write(m_namespace + "/provider-bytes", *used, true);
    }
}

FinalizedChunkStore::~FinalizedChunkStore() = default;

std::optional<std::uint64_t> FinalizedChunkStore::ReadCounter(const std::string& key) const
{
    if (!m_db->Exists(key)) return std::uint64_t{0};
    std::uint64_t value{0};
    if (!m_db->Read(key, value)) return std::nullopt;
    return value;
}

ChunkAdmissionResult FinalizedChunkStore::PutChunk(
    const uint256& publication_operation_id,
    const ChunkId& chunk_id,
    const std::span<const unsigned char> stored_bytes,
    const ChunkAuthorizationProof& proof,
    const FinalizedPublicationLookup& lookup)
{
    if (publication_operation_id.IsNull() || chunk_id == ChunkId{} ||
        stored_bytes.size() < ENCRYPTED_CHUNK_MIN_STORED_BYTES ||
        stored_bytes.size() > ENCRYPTED_CHUNK_MAX_STORED_BYTES || !lookup ||
        ComputeChunkId(stored_bytes) != chunk_id) return {ChunkAdmissionStatus::INVALID};

    std::optional<RootPublication> publication;
    try { publication = lookup(publication_operation_id); }
    catch (...) { return {ChunkAdmissionStatus::NOT_FINALIZED}; }
    if (!publication) return {ChunkAdmissionStatus::NOT_FINALIZED};
    if (!VerifyChunkAuthorizationProof(*publication, chunk_id, proof)) return {ChunkAdmissionStatus::NOT_AUTHORIZED};

    try {
        const auto proof_metadata = EncodeProofMetadata(proof);
        const auto chunk_key = ChunkKey(m_namespace, chunk_id);
        const auto publication_chunk_key = PublicationChunkKey(m_namespace, publication_operation_id, chunk_id);
        const auto provider_bytes_key = m_namespace + "/provider-bytes";

        std::lock_guard lock{m_mutex};
        std::uint64_t existing_size{0};
        std::vector<unsigned char> existing_chunk;
        bool chunk_exists{false};
        if (m_memory_only) {
            chunk_exists = m_db->Read(chunk_key, existing_chunk);
            if (chunk_exists) existing_size = existing_chunk.size();
        } else {
            chunk_exists = m_db->Read(chunk_key, existing_size);
        }
        if (!chunk_exists && m_db->Exists(chunk_key)) return {ChunkAdmissionStatus::STORAGE_ERROR};
        const auto blob_path = m_memory_only ? std::filesystem::path{} : BlobPath(m_blob_root, chunk_id);
        const bool blob_exists = m_memory_only ? chunk_exists : std::filesystem::exists(blob_path);
        if (!m_memory_only && blob_exists) {
            const auto existing = ReadBlob(blob_path, chunk_id);
            if (!existing) return {ChunkAdmissionStatus::STORAGE_ERROR};
            existing_chunk = *existing;
        }
        if (chunk_exists && existing_size != stored_bytes.size()) {
            return {ChunkAdmissionStatus::CONFLICT};
        }
        if (blob_exists && !std::equal(existing_chunk.begin(), existing_chunk.end(), stored_bytes.begin(), stored_bytes.end())) {
            return {ChunkAdmissionStatus::CONFLICT};
        }

        std::vector<unsigned char> existing_association;
        const bool association_exists = m_db->Read(publication_chunk_key, existing_association);
        if (association_exists) {
            if (!chunk_exists || existing_association != proof_metadata) return {ChunkAdmissionStatus::CONFLICT};
            if (blob_exists) return {ChunkAdmissionStatus::ALREADY_STORED};
        } else if (m_db->Exists(publication_chunk_key)) return {ChunkAdmissionStatus::STORAGE_ERROR};

        const auto provider_bytes = ReadCounter(provider_bytes_key);
        if (!provider_bytes) return {ChunkAdmissionStatus::STORAGE_ERROR};
        auto new_provider_bytes = *provider_bytes;
        if (!chunk_exists) {
            if (*provider_bytes > m_capacity_bytes || stored_bytes.size() > m_capacity_bytes - *provider_bytes) {
                return {ChunkAdmissionStatus::CAPACITY_EXCEEDED};
            }
            new_provider_bytes += stored_bytes.size();
        }

        if (!blob_exists && !m_memory_only) {
            if (!WriteBlobAtomically(blob_path, chunk_id, stored_bytes)) {
                std::error_code ec;
                std::filesystem::remove(blob_path, ec);
                return {ChunkAdmissionStatus::STORAGE_ERROR};
            }
        }

        KVStore::Batch batch;
        if (!chunk_exists) {
            if (m_memory_only) batch.Write(chunk_key, std::vector<unsigned char>{stored_bytes.begin(), stored_bytes.end()});
            else batch.Write(chunk_key, static_cast<std::uint64_t>(stored_bytes.size()));
        }
        batch.Write(publication_chunk_key, proof_metadata);
        if (!chunk_exists) batch.Write(provider_bytes_key, new_provider_bytes);
        try {
            m_db->WriteBatch(batch, true);
        } catch (...) {
            if (!m_memory_only && !blob_exists) {
                std::error_code ec;
                std::filesystem::remove(blob_path, ec);
            }
            throw;
        }
        return {ChunkAdmissionStatus::STORED};
    } catch (...) {
        return {ChunkAdmissionStatus::STORAGE_ERROR};
    }
}

std::optional<std::vector<unsigned char>> FinalizedChunkStore::GetChunk(const ChunkId& chunk_id) const
{
    if (chunk_id == ChunkId{}) return std::nullopt;
    std::vector<unsigned char> stored_bytes;
    const auto key = ChunkKey(m_namespace, chunk_id);
    if (m_memory_only) {
        if (!m_db->Read(key, stored_bytes) || ComputeChunkId(stored_bytes) != chunk_id) return std::nullopt;
        return stored_bytes;
    }
    std::uint64_t expected_size{0};
    if (!m_db->Read(key, expected_size)) return std::nullopt;
    const auto bytes = ReadBlob(BlobPath(m_blob_root, chunk_id), chunk_id, expected_size);
    if (!bytes) {
        return std::nullopt;
    }
    return bytes;
}

bool FinalizedChunkStore::HasChunk(const ChunkId& chunk_id) const
{
    return chunk_id != ChunkId{} && m_db->Exists(ChunkKey(m_namespace, chunk_id));
}

std::uint64_t FinalizedChunkStore::UsedBytes() const
{
    const auto bytes = ReadCounter(m_namespace + "/provider-bytes");
    return bytes.value_or(std::numeric_limits<std::uint64_t>::max());
}

} // namespace cybou
