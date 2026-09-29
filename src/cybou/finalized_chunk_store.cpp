// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/finalized_chunk_store.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

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
    : m_namespace{"chunk-store/v3/" + Hex(network_id)}, m_capacity_bytes{capacity_bytes}
{
    if (capacity_bytes == 0 || std::all_of(network_id.begin(), network_id.end(), [](const auto byte) { return byte == 0; }) ||
        (!memory_only && path.empty())) {
        throw std::invalid_argument{"invalid finalized chunk store configuration"};
    }
    m_db = std::make_unique<KVStore>(KVStoreOptions{
        .path = path,
        .cache_bytes = 8 << 20,
        .memory_only = memory_only,
        .wipe_data = wipe_data,
    });

    // The path is operator-configured and can accidentally be reused across networks.
    // Refuse that configuration rather than mixing provider data or accounting.
    const std::string network_key{"chunk-store/v3/network-id"};
    std::vector<unsigned char> saved_network_id;
    if (m_db->Read(network_key, saved_network_id)) {
        if (!std::equal(saved_network_id.begin(), saved_network_id.end(), network_id.begin(), network_id.end())) {
            throw std::invalid_argument{"finalized chunk store network ID mismatch"};
        }
    } else {
        if (m_db->Exists(network_key)) throw std::runtime_error{"corrupt finalized chunk store network ID"};
        m_db->Write(network_key, std::vector<unsigned char>{network_id.begin(), network_id.end()}, true);
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
    if (publication_operation_id.IsNull() || chunk_id == ChunkId{} || stored_bytes.empty() || !lookup ||
        ComputeChunkId(stored_bytes) != chunk_id) return {ChunkAdmissionStatus::INVALID};

    std::optional<RootPublication> publication;
    try { publication = lookup(publication_operation_id); }
    catch (...) { return {ChunkAdmissionStatus::NOT_FINALIZED}; }
    if (!publication) return {ChunkAdmissionStatus::NOT_FINALIZED};
    if (!VerifyChunkAuthorizationProof(*publication, chunk_id, proof)) return {ChunkAdmissionStatus::NOT_AUTHORIZED};

    try {
        const auto stored_bytes_vector = std::vector<unsigned char>{stored_bytes.begin(), stored_bytes.end()};
        const auto proof_metadata = EncodeProofMetadata(proof);
        const auto chunk_key = ChunkKey(m_namespace, chunk_id);
        const auto publication_chunk_key = PublicationChunkKey(m_namespace, publication_operation_id, chunk_id);
        const auto provider_bytes_key = m_namespace + "/provider-bytes";

        std::lock_guard lock{m_mutex};
        std::vector<unsigned char> existing_chunk;
        const bool chunk_exists = m_db->Read(chunk_key, existing_chunk);
        if (!chunk_exists && m_db->Exists(chunk_key)) return {ChunkAdmissionStatus::STORAGE_ERROR};
        if (chunk_exists && existing_chunk != stored_bytes_vector) return {ChunkAdmissionStatus::CONFLICT};

        std::vector<unsigned char> existing_association;
        if (m_db->Read(publication_chunk_key, existing_association)) {
            if (!chunk_exists || existing_association != proof_metadata) return {ChunkAdmissionStatus::CONFLICT};
            return {ChunkAdmissionStatus::ALREADY_STORED};
        }
        if (m_db->Exists(publication_chunk_key)) return {ChunkAdmissionStatus::STORAGE_ERROR};

        const auto provider_bytes = ReadCounter(provider_bytes_key);
        if (!provider_bytes) return {ChunkAdmissionStatus::STORAGE_ERROR};
        auto new_provider_bytes = *provider_bytes;
        if (!chunk_exists) {
            if (*provider_bytes > m_capacity_bytes || stored_bytes.size() > m_capacity_bytes - *provider_bytes) {
                return {ChunkAdmissionStatus::CAPACITY_EXCEEDED};
            }
            new_provider_bytes += stored_bytes.size();
        }

        KVStore::Batch batch;
        if (!chunk_exists) batch.Write(chunk_key, stored_bytes_vector);
        batch.Write(publication_chunk_key, proof_metadata);
        if (!chunk_exists) batch.Write(provider_bytes_key, new_provider_bytes);
        m_db->WriteBatch(batch, true);
        return {ChunkAdmissionStatus::STORED};
    } catch (...) {
        return {ChunkAdmissionStatus::STORAGE_ERROR};
    }
}

std::optional<std::vector<unsigned char>> FinalizedChunkStore::GetChunk(const ChunkId& chunk_id) const
{
    if (chunk_id == ChunkId{}) return std::nullopt;
    std::vector<unsigned char> stored_bytes;
    if (!m_db->Read(ChunkKey(m_namespace, chunk_id), stored_bytes) || ComputeChunkId(stored_bytes) != chunk_id) {
        return std::nullopt;
    }
    return stored_bytes;
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
