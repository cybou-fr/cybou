// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Реализация admission-store для finalized provider-чанков.

#include <cybou/finalized_chunk_store.h>
#include <cybou/encrypted_chunk.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace cybou {
namespace {

constexpr char HEX[] = "0123456789abcdef";

/// Канонический lower-case hex для ключей metadata store.
std::string Hex(const std::span<const unsigned char> bytes)
{
    std::string result(bytes.size() * 2, '\0');
    for (std::size_t i{0}; i < bytes.size(); ++i) {
        result[2 * i] = HEX[bytes[i] >> 4];
        result[2 * i + 1] = HEX[bytes[i] & 0x0f];
    }
    return result;
}

/// Строгий разбор ChunkId из metadata key.
std::optional<ChunkId> ParseChunkId(const std::string_view hex)
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

/// Ключ размера admitted blob.
std::string ChunkKey(const std::string& name_space, const ChunkId& id)
{
    return name_space + "/chunk/" + Hex(id);
}

/// Ключ привязки публикации к конкретному chunk proof.
std::string PublicationChunkKey(const std::string& name_space, const cybou::Hash256& publication_id, const ChunkId& id)
{
    return name_space + "/publication-chunk/" + publication_id.GetHex() + "/" + Hex(id);
}

/// Proof кодируется отдельно от blob: один physical ciphertext может быть авторизован несколькими публикациями.
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

/// Любая неоднозначность кодирования proof считается corruption.
std::optional<ChunkAuthorizationProof> DecodeProofMetadata(const std::span<const unsigned char> encoded)
{
    if (encoded.size() < 8 || encoded.size() > 8 + 32 * 32) return std::nullopt;
    const std::uint32_t sibling_count = (std::uint32_t{encoded[4]} << 24) |
        (std::uint32_t{encoded[5]} << 16) | (std::uint32_t{encoded[6]} << 8) | encoded[7];
    if (sibling_count > 32 || encoded.size() != 8 + sibling_count * 32) return std::nullopt;
    ChunkAuthorizationProof proof;
    proof.leaf_index = (std::uint32_t{encoded[0]} << 24) |
        (std::uint32_t{encoded[1]} << 16) | (std::uint32_t{encoded[2]} << 8) | encoded[3];
    proof.siblings.resize(sibling_count);
    for (std::uint32_t i{0}; i < sibling_count; ++i) {
        std::copy_n(encoded.begin() + 8 + i * 32, 32, proof.siblings[i].begin());
    }
    return proof;
}

} // namespace

FinalizedChunkStore::FinalizedChunkStore(ChunkBlobStore& blobs, const std::filesystem::path& path,
    const std::span<const unsigned char, 32> network_binding, const std::uint64_t capacity_bytes,
    const bool wipe_data)
    : m_blobs{blobs}, m_namespace{"chunk-store/" + Hex(network_binding)}, m_capacity_bytes{capacity_bytes}, m_path{path}
{
    if (std::all_of(network_binding.begin(), network_binding.end(), [](const auto byte) { return byte == 0; }) ||
        (!m_blobs.MemoryOnly() && path.empty())) {
        throw std::invalid_argument{"invalid finalized chunk store configuration"};
    }
    m_db = std::make_unique<KVStore>(KVStoreOptions{
        .path = m_blobs.MemoryOnly() ? std::filesystem::path{} : path / "metadata",
        .cache_bytes = 8 << 20,
        .memory_only = m_blobs.MemoryOnly(),
        .wipe_data = wipe_data,
    });

    // Provider-метаданные жёстко привязаны к сети, хотя общий blob-store индексируется только exact-byte hash.
    const std::string network_key{"chunk-store/network-id"};
    std::vector<unsigned char> saved_network_binding;
    if (m_db->Read(network_key, saved_network_binding)) {
        if (!std::equal(saved_network_binding.begin(), saved_network_binding.end(), network_binding.begin(), network_binding.end())) {
            throw std::invalid_argument{"finalized chunk store network ID mismatch"};
        }
    } else {
        if (m_db->Exists(network_key)) throw std::runtime_error{"corrupt finalized chunk store network ID"};
        m_db->Write(network_key, std::vector<unsigned char>{network_binding.begin(), network_binding.end()}, true);
    }

    if (!m_blobs.MemoryOnly()) {
        const auto prefix = m_namespace + "/chunk/";
        const auto key_size = prefix.size() + 64;
        std::uint64_t total{0};
        m_db->ForEachStringPrefixRaw(prefix, key_size, [&](const std::string& key, const std::string& raw_size) {
            if (key.size() != key_size || raw_size.size() != sizeof(std::uint64_t)) {
                throw std::runtime_error{"corrupt finalized chunk metadata"};
            }
            const auto id = ParseChunkId(key.substr(prefix.size()));
            std::uint64_t size{0};
            const auto encoded = std::span{reinterpret_cast<const unsigned char*>(raw_size.data()), raw_size.size()};
            if (!id || !detail::DeserializeLocalRecord(encoded, size) ||
                size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
                size > std::numeric_limits<std::uint64_t>::max() - total) {
                throw std::runtime_error{"invalid finalized chunk size metadata"};
            }
            // На старте проверяем только наличие и размер; полная BLAKE3-проверка остаётся на GET и аудитах, чтобы reopen был дешёвым.
            if (m_blobs.StoredSize(*id) != std::optional<std::uint64_t>{size}) {
                throw std::runtime_error{"provider chunk blob is missing or has the wrong size"};
            }
            total += size;
        });
        // Сниженная квота сохраняет уже принятые реплики, но блокирует новый admission.
        m_db->Write(m_namespace + "/storage-bytes", total, true);
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

ChunkAdmissionResult FinalizedChunkStore::PutChunk(const cybou::Hash256& publication_operation_id,
    const ChunkId& chunk_id, const std::span<const unsigned char> stored_bytes,
    const ChunkAuthorizationProof& proof, const FinalizedPublicationLookup& lookup)
{
    if (publication_operation_id.IsNull() || chunk_id == ChunkId{} ||
        stored_bytes.size() < ENCRYPTED_CHUNK_MIN_STORED_BYTES ||
        stored_bytes.size() > ENCRYPTED_CHUNK_MAX_STORED_BYTES || !lookup ||
        ComputeChunkId(stored_bytes) != chunk_id) return {ChunkAdmissionStatus::INVALID};

    std::optional<RootPublication> publication;
    try { publication = lookup(publication_operation_id); }
    catch (...) { return {ChunkAdmissionStatus::NOT_FINALIZED}; }
    // Finality-first admission: provider не принимает chunk, пока RootPublication не виден в finalized state.
    if (!publication) return {ChunkAdmissionStatus::NOT_FINALIZED};
    if (!VerifyChunkAuthorizationProof(*publication, chunk_id, proof)) return {ChunkAdmissionStatus::NOT_AUTHORIZED};

    try {
        const auto proof_metadata = EncodeProofMetadata(proof);
        const auto chunk_key = ChunkKey(m_namespace, chunk_id);
        const auto publication_chunk_key = PublicationChunkKey(m_namespace, publication_operation_id, chunk_id);
        const auto storage_bytes_key = m_namespace + "/storage-bytes";

        std::lock_guard lock{m_mutex};
        std::uint64_t existing_size{0};
        const bool chunk_exists = m_db->Read(chunk_key, existing_size);
        if (!chunk_exists && m_db->Exists(chunk_key)) return {ChunkAdmissionStatus::STORAGE_ERROR};
        if (chunk_exists && existing_size != stored_bytes.size()) return {ChunkAdmissionStatus::CONFLICT};

        std::vector<unsigned char> existing_association;
        const bool association_exists = m_db->Read(publication_chunk_key, existing_association);
        if (association_exists && (!chunk_exists || existing_association != proof_metadata)) {
            return {ChunkAdmissionStatus::CONFLICT};
        }
        if (!association_exists && m_db->Exists(publication_chunk_key)) {
            return {ChunkAdmissionStatus::STORAGE_ERROR};
        }

        const auto storage_bytes = ReadCounter(storage_bytes_key);
        if (!storage_bytes) return {ChunkAdmissionStatus::STORAGE_ERROR};
        if (!chunk_exists && (*storage_bytes > m_capacity_bytes ||
            stored_bytes.size() > m_capacity_bytes - *storage_bytes)) {
            return {ChunkAdmissionStatus::CAPACITY_EXCEEDED};
        }

        if (!m_blobs.MemoryOnly() && !m_blobs.StoredSize(chunk_id)) {
            std::error_code ec;
            const auto space = std::filesystem::space(m_path, ec);
            const uint64_t reserve = std::max<uint64_t>(1ULL << 30, space.capacity / 20);
            // Fail-closed reserve совпадает с архитектурой provider admission: нехватка или неизвестность места блокирует новый admission.
            if (ec || space.available <= reserve || stored_bytes.size() > space.available - reserve)
                return {ChunkAdmissionStatus::CAPACITY_EXCEEDED};
        }
        const auto blob_status = m_blobs.Put(chunk_id, stored_bytes);
        if (blob_status == ChunkBlobPutStatus::INVALID) return {ChunkAdmissionStatus::INVALID};
        if (blob_status == ChunkBlobPutStatus::CONFLICT) return {ChunkAdmissionStatus::CONFLICT};
        if (blob_status == ChunkBlobPutStatus::STORAGE_ERROR) return {ChunkAdmissionStatus::STORAGE_ERROR};
        if (association_exists) {
            return {blob_status == ChunkBlobPutStatus::STORED ? ChunkAdmissionStatus::STORED :
                ChunkAdmissionStatus::ALREADY_STORED};
        }

        KVStore::Batch batch;
        if (!chunk_exists) {
            batch.Write(chunk_key, static_cast<std::uint64_t>(stored_bytes.size()));
            batch.Write(storage_bytes_key, *storage_bytes + stored_bytes.size());
        }
        batch.Write(publication_chunk_key, proof_metadata);
        m_db->WriteBatch(batch, true);
        return {ChunkAdmissionStatus::STORED};
    } catch (...) {
        // Если metadata write не завершился, provisional-admission не существует: authoritative proof так и не появляется.
        return {ChunkAdmissionStatus::STORAGE_ERROR};
    }
}

std::optional<ChunkAuthorizationProof> FinalizedChunkStore::GetChunkAuthorizationProof(
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
    const FinalizedPublicationLookup& lookup) const
{
    if (publication_operation_id.IsNull() || chunk_id == ChunkId{} || !lookup) return std::nullopt;
    const auto publication = lookup(publication_operation_id);
    if (!publication) return std::nullopt;
    const auto bytes = GetChunk(chunk_id);
    if (!bytes || ComputeChunkId(*bytes) != chunk_id) return std::nullopt;
    std::vector<unsigned char> encoded;
    if (!m_db->Read(PublicationChunkKey(m_namespace, publication_operation_id, chunk_id), encoded)) {
        return std::nullopt;
    }
    const auto proof = DecodeProofMetadata(encoded);
    if (!proof || !VerifyChunkAuthorizationProof(*publication, chunk_id, *proof)) return std::nullopt;
    return proof;
}

std::optional<std::vector<unsigned char>> FinalizedChunkStore::GetChunk(const ChunkId& chunk_id) const
{
    if (chunk_id == ChunkId{}) return std::nullopt;
    std::uint64_t expected_size{0};
    if (!m_db->Read(ChunkKey(m_namespace, chunk_id), expected_size)) return std::nullopt;
    const auto bytes = m_blobs.Get(chunk_id);
    if (!bytes || bytes->size() != expected_size) return std::nullopt;
    return bytes;
}

std::optional<uint64_t> FinalizedChunkStore::StoredSize(const ChunkId& chunk_id) const
{
    uint64_t size{0};
    if (chunk_id == ChunkId{} || !m_db->Read(ChunkKey(m_namespace, chunk_id), size) ||
        size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES)
        return std::nullopt;
    return size;
}

bool FinalizedChunkStore::HasChunk(const ChunkId& chunk_id) const
{
    if (chunk_id == ChunkId{}) return false;
    std::uint64_t expected_size{0};
    if (!m_db->Read(ChunkKey(m_namespace, chunk_id), expected_size)) return false;
    return m_blobs.StoredSize(chunk_id) == std::optional<std::uint64_t>{expected_size};
}

bool FinalizedChunkStore::RemoveUnlessAdmitted(const ChunkId& chunk_id)
{
    std::lock_guard lock{m_mutex};
    std::uint64_t admitted_size{0};
    if (m_db->Read(ChunkKey(m_namespace, chunk_id), admitted_size)) return false;
    return m_blobs.Remove(chunk_id);
}

std::uint64_t FinalizedChunkStore::UsedBytes() const
{
    const auto bytes = ReadCounter(m_namespace + "/storage-bytes");
    return bytes.value_or(std::numeric_limits<std::uint64_t>::max());
}

} // namespace cybou
