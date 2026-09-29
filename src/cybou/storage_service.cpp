// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/storage_service.h>

#include <cybou/node_runtime.h>
#include <cybou/root_publication.h>

#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <limits>
#include <set>

namespace cybou {
namespace {

constexpr std::array<unsigned char, 5> MAGIC{'C', 'Y', 'S', 'P', 1};
/** Bound on a single placement record: the largest publication chunk count. */
constexpr std::uint32_t MAX_PLACEMENT_LEAVES{1U << 20};
constexpr std::size_t MAX_REPLICAS_PER_CHUNK{16};

std::string PlacementKey(const uint256& operation_id)
{
    return "storage/placement/" + operation_id.GetHex();
}

void Append16(std::vector<unsigned char>& out, const std::uint16_t value)
{
    out.push_back(static_cast<unsigned char>(value));
    out.push_back(static_cast<unsigned char>(value >> 8));
}

void Append32(std::vector<unsigned char>& out, const std::uint32_t value)
{
    for (unsigned i{0}; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

class Reader {
public:
    explicit Reader(std::span<const unsigned char> bytes) : m_bytes{bytes} {}
    bool Take(std::span<unsigned char> out)
    {
        if (m_bytes.size() - m_offset < out.size()) return false;
        std::copy_n(m_bytes.begin() + m_offset, out.size(), out.begin());
        m_offset += out.size();
        return true;
    }
    std::optional<std::uint32_t> U8()
    {
        std::array<unsigned char, 1> b{};
        return Take(b) ? std::optional<std::uint32_t>{b[0]} : std::nullopt;
    }
    std::optional<std::uint32_t> U16()
    {
        std::array<unsigned char, 2> b{};
        if (!Take(b)) return std::nullopt;
        return std::uint32_t{b[0]} | (std::uint32_t{b[1]} << 8);
    }
    std::optional<std::uint32_t> U32()
    {
        std::array<unsigned char, 4> b{};
        if (!Take(b)) return std::nullopt;
        std::uint32_t value{0};
        for (unsigned i{0}; i < 4; ++i) value |= std::uint32_t{b[i]} << (8 * i);
        return value;
    }
    bool Done() const { return m_offset == m_bytes.size(); }

private:
    std::span<const unsigned char> m_bytes;
    std::size_t m_offset{0};
};

/** Uniform CSPRNG shuffle; false when the system RNG fails. */
bool Shuffle(std::vector<StorageEndpoint>& items)
{
    for (std::size_t i = items.size(); i > 1; --i) {
        const std::uint64_t bound = i;
        const std::uint64_t limit = std::numeric_limits<std::uint64_t>::max() -
            std::numeric_limits<std::uint64_t>::max() % bound;
        std::uint64_t draw{0};
        do {
            if (RAND_bytes(reinterpret_cast<unsigned char*>(&draw), sizeof(draw)) != 1) return false;
        } while (draw >= limit);
        std::swap(items[i - 1], items[draw % bound]);
    }
    return true;
}

} // namespace

int PublicationDurability::ProgressPercent(const std::uint8_t target) const
{
    if (state == DurabilityState::PROTECTED) return 100;
    if (chunk_count == 0 || target == 0) return 0;
    return static_cast<int>(std::uint64_t{chunks_at_target} * 100 / chunk_count);
}

/* ---- RuntimeStorageTransport ---- */

std::vector<StorageEndpoint> RuntimeStorageTransport::Providers()
{
    std::vector<StorageEndpoint> providers;
    for (auto& [address, port] : m_runtime.StoragePeerEndpoints()) providers.push_back({address, port});
    return providers;
}

std::optional<ChunkAdmissionResult> RuntimeStorageTransport::Put(const StorageEndpoint& provider,
    const uint256& publication_operation_id, const ChunkId& chunk_id,
    const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    return m_runtime.PutChunkToStoragePeer(provider.address, provider.port, publication_operation_id,
        chunk_id, stored_bytes, proof);
}

std::optional<std::vector<unsigned char>> RuntimeStorageTransport::Get(const StorageEndpoint& provider,
    const ChunkId& chunk_id)
{
    return m_runtime.GetChunkFromStoragePeer(provider.address, provider.port, chunk_id);
}

std::optional<ChunkAuthorizationProof> RuntimeStorageTransport::GetProof(const StorageEndpoint& provider,
    const uint256& publication_operation_id, const ChunkId& chunk_id)
{
    return m_runtime.GetChunkAuthorizationProofFromStoragePeer(provider.address, provider.port,
        publication_operation_id, chunk_id);
}

/* ---- StorageService ---- */

struct StorageService::Placement {
    uint256 operation_id;
    std::vector<ChunkId> leaves;
    /** Remote providers that acknowledged each leaf; the local copy never appears here. */
    std::vector<std::vector<StorageEndpoint>> replicas;
};

StorageService::StorageService(CybouNodeRuntime& runtime, StorageTransport& transport,
    PrivateApplicationStore& application_db, const std::uint8_t remote_replica_target)
    : m_runtime{runtime}, m_transport{transport}, m_application_db{application_db},
      m_target{std::clamp<std::uint8_t>(remote_replica_target, 1, MAX_REPLICAS_PER_CHUNK)}
{
}

std::optional<StorageService::Placement> StorageService::Load(const uint256& operation_id) const
{
    const auto encoded = m_application_db.Get(PlacementKey(operation_id));
    if (!encoded) return std::nullopt;
    Reader in{*encoded};
    std::array<unsigned char, MAGIC.size()> magic{};
    Placement placement;
    if (!in.Take(magic) || magic != MAGIC || !in.Take(std::span{placement.operation_id.begin(), 32}) ||
        placement.operation_id != operation_id) return std::nullopt;
    const auto count = in.U32();
    if (!count || *count == 0 || *count > MAX_PLACEMENT_LEAVES) return std::nullopt;
    placement.leaves.resize(*count);
    placement.replicas.resize(*count);
    for (std::uint32_t i{0}; i < *count; ++i) {
        if (!in.Take(placement.leaves[i])) return std::nullopt;
        const auto replicas = in.U8();
        if (!replicas || *replicas > MAX_REPLICAS_PER_CHUNK) return std::nullopt;
        for (std::uint32_t r{0}; r < *replicas; ++r) {
            const auto length = in.U8();
            if (!length || *length == 0) return std::nullopt;
            std::string address(*length, '\0');
            if (!in.Take(std::span{reinterpret_cast<unsigned char*>(address.data()), address.size()})) {
                return std::nullopt;
            }
            const auto port = in.U16();
            if (!port || *port == 0) return std::nullopt;
            placement.replicas[i].push_back({std::move(address), static_cast<std::uint16_t>(*port)});
        }
    }
    if (!in.Done()) return std::nullopt;
    return placement;
}

bool StorageService::Save(const Placement& placement)
{
    std::vector<unsigned char> out(MAGIC.begin(), MAGIC.end());
    out.insert(out.end(), placement.operation_id.begin(), placement.operation_id.end());
    Append32(out, static_cast<std::uint32_t>(placement.leaves.size()));
    for (std::size_t i{0}; i < placement.leaves.size(); ++i) {
        out.insert(out.end(), placement.leaves[i].begin(), placement.leaves[i].end());
        const auto& replicas = placement.replicas[i];
        out.push_back(static_cast<unsigned char>(std::min(replicas.size(), MAX_REPLICAS_PER_CHUNK)));
        for (std::size_t r{0}; r < replicas.size() && r < MAX_REPLICAS_PER_CHUNK; ++r) {
            const auto& address = replicas[r].address;
            if (address.empty() || address.size() > 255) return false;
            out.push_back(static_cast<unsigned char>(address.size()));
            out.insert(out.end(), address.begin(), address.end());
            Append16(out, replicas[r].port);
        }
    }
    return m_application_db.Put(PlacementKey(placement.operation_id), out);
}

PublicationDurability StorageService::Summarize(const Placement& placement) const
{
    PublicationDurability result;
    result.chunk_count = static_cast<std::uint32_t>(placement.leaves.size());
    result.min_replicas = std::numeric_limits<std::uint32_t>::max();
    for (const auto& replicas : placement.replicas) {
        const auto count = static_cast<std::uint32_t>(replicas.size());
        result.min_replicas = std::min(result.min_replicas, count);
        if (count >= m_target) ++result.chunks_at_target;
    }
    if (placement.leaves.empty()) result.min_replicas = 0;
    result.state = result.chunk_count > 0 && result.chunks_at_target == result.chunk_count
        ? DurabilityState::PROTECTED : DurabilityState::SECURING;
    return result;
}

PublicationDurability StorageService::Secure(const uint256& operation_id, const std::span<const ChunkId> leaves)
{
    std::lock_guard lock{m_mutex};
    if (!m_application_db.IsUnlocked()) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Application DB is locked"};
    }
    // Only finalized publications may be placed, and only with their exact chunk set.
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication) return {.state = DurabilityState::SECURING, .error = "Publication is not finalized yet"};
    if (leaves.empty() || leaves.size() > MAX_PLACEMENT_LEAVES || leaves.size() != publication->chunk_count) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Chunk list does not match the publication"};
    }
    ChunkAuthorizationAccumulator accumulator;
    for (const auto& leaf : leaves) {
        if (!accumulator.Add({leaf})) {
            return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Chunk list is invalid"};
        }
    }
    const auto summary = accumulator.Finish();
    if (!summary || summary->root != publication->chunk_authorization_root) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Chunk list does not match the publication"};
    }
    auto placement = Load(operation_id);
    if (!placement || placement->leaves.size() != leaves.size() ||
        !std::equal(leaves.begin(), leaves.end(), placement->leaves.begin())) {
        placement = Placement{.operation_id = operation_id,
            .leaves = {leaves.begin(), leaves.end()},
            .replicas = std::vector<std::vector<StorageEndpoint>>(leaves.size())};
        if (!Save(*placement)) {
            return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save placement state"};
        }
    }
    return Place(*placement);
}

std::optional<ChunkAuthorizationProof> StorageService::GetAuthorizationProof(
    const uint256& operation_id, const ChunkId& chunk_id)
{
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication) return std::nullopt;
    for (const auto& provider : m_transport.Providers()) {
        const auto proof = m_transport.GetProof(provider, operation_id, chunk_id);
        if (proof && VerifyChunkAuthorizationProof(*publication, chunk_id, *proof)) return proof;
    }
    return std::nullopt;
}

PublicationDurability StorageService::Rebuild(const uint256& operation_id,
    const std::span<const ChunkId> candidate_chunks)
{
    std::lock_guard lock{m_mutex};
    if (!m_application_db.IsUnlocked()) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Application DB is locked"};
    }
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication) return {.state = DurabilityState::SECURING, .error = "Publication is not finalized yet"};
    if (publication->chunk_count == 0 || publication->chunk_count > MAX_PLACEMENT_LEAVES ||
        candidate_chunks.empty() || candidate_chunks.size() > MAX_PLACEMENT_LEAVES) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Candidate chunk set is invalid"};
    }

    Placement placement{.operation_id = operation_id,
        .leaves = std::vector<ChunkId>(publication->chunk_count),
        .replicas = std::vector<std::vector<StorageEndpoint>>(publication->chunk_count)};
    std::vector<bool> found(publication->chunk_count, false);
    std::set<ChunkId> unique;
    const auto providers = m_transport.Providers();
    for (const auto& chunk_id : candidate_chunks) {
        if (chunk_id == ChunkId{} || !unique.insert(chunk_id).second) continue;
        for (const auto& provider : providers) {
            const auto proof = m_transport.GetProof(provider, operation_id, chunk_id);
            if (!proof || !VerifyChunkAuthorizationProof(*publication, chunk_id, *proof)) continue;
            if (proof->leaf_index >= placement.leaves.size()) {
                return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Provider returned an invalid leaf index"};
            }
            const auto index = proof->leaf_index;
            if (found[index] && placement.leaves[index] != chunk_id) {
                return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Providers disagree on publication leaves"};
            }
            placement.leaves[index] = chunk_id;
            found[index] = true;
            if (std::find(placement.replicas[index].begin(), placement.replicas[index].end(), provider) ==
                placement.replicas[index].end()) {
                placement.replicas[index].push_back(provider);
            }
        }
    }
    if (std::find(found.begin(), found.end(), false) != found.end()) {
        return {.state = DurabilityState::SECURING, .chunk_count = publication->chunk_count,
            .error = "Some finalized chunk proofs are not available from reachable providers"};
    }
    ChunkAuthorizationAccumulator accumulator;
    for (const auto& leaf : placement.leaves) {
        if (!accumulator.Add({leaf})) {
            return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Rebuilt chunk order is invalid"};
        }
    }
    const auto commitment = accumulator.Finish();
    if (!commitment || commitment->root != publication->chunk_authorization_root) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Rebuilt leaves do not match finalized authorization"};
    }
    if (!Save(placement)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save rebuilt placement state"};
    }
    return Place(placement);
}

PublicationDurability StorageService::Resume(const uint256& operation_id)
{
    std::lock_guard lock{m_mutex};
    auto placement = Load(operation_id);
    if (!placement) return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Unknown publication placement"};
    if (!m_runtime.FindFinalizedRootPublication(operation_id)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Publication is not finalized"};
    }
    return Place(*placement);
}

PublicationDurability StorageService::Place(Placement& placement)
{
    auto result = Summarize(placement);
    if (result.state == DurabilityState::PROTECTED) return result;

    std::vector<AuthorizedChunk> chunks;
    chunks.reserve(placement.leaves.size());
    for (const auto& leaf : placement.leaves) chunks.push_back({leaf});
    const auto commitment = BuildChunkAuthorizationCommitment(chunks);
    if (!commitment || commitment->proofs.size() != placement.leaves.size()) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot rebuild chunk authorization proofs"};
    }
    auto providers = m_transport.Providers();
    if (!Shuffle(providers)) return {.state = DurabilityState::SECURING, .error = "System RNG failure"};

    bool changed{false};
    bool content_missing{false};
    for (std::size_t i{0}; i < placement.leaves.size(); ++i) {
        auto& replicas = placement.replicas[i];
        if (replicas.size() >= m_target) continue;
        // Local copy, or any healthy remote copy when repairing after eviction.
        const auto bytes = FetchLocked(placement.leaves[i], replicas);
        if (!bytes) {
            content_missing = true;
            continue;
        }
        for (const auto& provider : providers) {
            if (replicas.size() >= m_target) break;
            if (std::find(replicas.begin(), replicas.end(), provider) != replicas.end()) continue;
            const auto admitted = m_transport.Put(provider, placement.operation_id, placement.leaves[i],
                *bytes, commitment->proofs[i]);
            // STORED and ALREADY_STORED both mean the provider now retains the chunk.
            if (!admitted || !*admitted) continue;
            replicas.push_back(provider);
            changed = true;
        }
    }
    if (changed && !Save(placement)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save placement state"};
    }
    result = Summarize(placement);
    if (result.state != DurabilityState::PROTECTED) {
        result.error = content_missing ? "Some encrypted content is temporarily unavailable"
            : providers.size() < m_target ? "Not enough storage providers are reachable"
            : "Storage providers did not accept every chunk yet";
    }
    return result;
}

PublicationDurability StorageService::AuditSome(const uint256& operation_id, const std::size_t max_chunks)
{
    std::lock_guard lock{m_mutex};
    auto placement = Load(operation_id);
    if (!placement) return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Unknown publication placement"};
    const std::size_t count = placement->leaves.size();
    auto& cursor = m_audit_cursor[operation_id];
    bool changed{false};
    for (std::size_t checked{0}; checked < std::min(max_chunks, count); ++checked) {
        const std::size_t i = cursor % count;
        cursor = (cursor + 1) % count;
        auto& replicas = placement->replicas[i];
        const auto before = replicas.size();
        std::erase_if(replicas, [&](const StorageEndpoint& provider) {
            const auto bytes = m_transport.Get(provider, placement->leaves[i]);
            return !bytes || ComputeChunkId(*bytes) != placement->leaves[i];
        });
        changed = changed || replicas.size() != before;
    }
    if (changed && !Save(*placement)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save placement state"};
    }
    auto result = Summarize(*placement);
    if (result.state != DurabilityState::PROTECTED) result.error = "A remote copy is missing or damaged";
    return result;
}

PublicationDurability StorageService::Audit(const uint256& operation_id)
{
    std::lock_guard lock{m_mutex};
    auto placement = Load(operation_id);
    if (!placement) return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Unknown publication placement"};
    bool changed{false};
    for (std::size_t i{0}; i < placement->leaves.size(); ++i) {
        auto& replicas = placement->replicas[i];
        const auto before = replicas.size();
        std::erase_if(replicas, [&](const StorageEndpoint& provider) {
            const auto bytes = m_transport.Get(provider, placement->leaves[i]);
            return !bytes || ComputeChunkId(*bytes) != placement->leaves[i];
        });
        changed = changed || replicas.size() != before;
    }
    if (changed && !Save(*placement)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save placement state"};
    }
    if (!m_runtime.FindFinalizedRootPublication(operation_id)) return Summarize(*placement);
    return Place(*placement);
}

std::optional<PublicationDurability> StorageService::GetDurability(const uint256& operation_id)
{
    std::lock_guard lock{m_mutex};
    const auto placement = Load(operation_id);
    if (!placement) return std::nullopt;
    return Summarize(*placement);
}

std::optional<std::vector<unsigned char>> StorageService::Fetch(const ChunkId& chunk_id)
{
    std::lock_guard lock{m_mutex};
    return FetchLocked(chunk_id, {});
}

std::optional<std::vector<unsigned char>> StorageService::FetchLocked(const ChunkId& chunk_id,
    const std::span<const StorageEndpoint> preferred)
{
    if (auto local = m_runtime.GetChunkBlobStore().Get(chunk_id); local && ComputeChunkId(*local) == chunk_id) {
        return local;
    }
    std::vector<StorageEndpoint> candidates{preferred.begin(), preferred.end()};
    auto others = m_transport.Providers();
    if (!Shuffle(others)) return std::nullopt;
    for (auto& provider : others) {
        if (std::find(candidates.begin(), candidates.end(), provider) == candidates.end()) {
            candidates.push_back(std::move(provider));
        }
    }
    for (const auto& provider : candidates) {
        auto bytes = m_transport.Get(provider, chunk_id);
        if (!bytes || ComputeChunkId(*bytes) != chunk_id) continue;
        // Cache the verified ciphertext; a failed cache write does not fail retrieval.
        (void)m_runtime.GetChunkBlobStore().Put(chunk_id, *bytes);
        return bytes;
    }
    return std::nullopt;
}

} // namespace cybou
