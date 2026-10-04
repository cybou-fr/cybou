// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Реализация remote durability и placement exact encrypted chunks.

#include <cybou/storage_service.h>

#include <cybou/node_runtime.h>
#include <cybou/root_publication.h>

#include <openssl/rand.h>

#include <algorithm>
#include <chrono>
#include <array>
#include <limits>
#include <set>

namespace cybou {
namespace {

constexpr std::array<unsigned char, 4> MAGIC{'C', 'Y', 'S', 'P'};

/// Дедупликация идёт по StorageId, потому что один Full Node может отвечать с нескольких endpoint.
bool HasProvider(std::span<const StorageEndpoint> replicas, const StorageEndpoint& provider)
{
    return std::any_of(replicas.begin(), replicas.end(),
        [&](const StorageEndpoint& r) { return SameProvider(r, provider); });
}
/// Верхняя граница числа реплик, сериализуемых на один чанк placement.
constexpr std::size_t MAX_REPLICAS_PER_CHUNK{16};

/// Ключ индекса всех placements, за которыми идёт audit/repair.
constexpr std::string_view PLACEMENT_INDEX_KEY{"storage/placements"};

/// Placement metadata живут в Application DB, а не в consensus state.
std::string PlacementKey(const cybou::Hash256& operation_id)
{
    return "storage/placement/" + operation_id.GetHex();
}

/// Little-endian encoding достаточно для локального state.
void Append16(std::vector<unsigned char>& out, const std::uint16_t value)
{
    out.push_back(static_cast<unsigned char>(value));
    out.push_back(static_cast<unsigned char>(value >> 8));
}

/// Little-endian encoding достаточно для локального state.
void Append32(std::vector<unsigned char>& out, const std::uint32_t value)
{
    for (unsigned i{0}; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

/// Минимальный локальный reader fail-closed для placement metadata.
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

/// Подписанные receipts хранятся отдельно от placement, по одному на (публикация, чанк, provider).
std::string ReceiptKey(const cybou::Hash256& operation_id, const ChunkId& chunk_id,
    const std::array<unsigned char, 32>& storage_id)
{
    return "storage/receipt/" + operation_id.GetHex() + '/' + cybou::Hash256{std::span<const unsigned char, 32>{chunk_id}}.GetHex() + '/' +
        cybou::Hash256{std::span<const unsigned char, 32>{storage_id}}.GetHex();
}

constexpr std::string_view EVIDENCE_INDEX_KEY{"storage/evidence-index"};

std::string EvidenceKey(const std::array<unsigned char, 32>& storage_id)
{
    return "storage/evidence/" + cybou::Hash256{std::span<const unsigned char, 32>{storage_id}}.GetHex();
}

/// Фиксированная локальная запись: десять little-endian 64-битных полей.
constexpr std::size_t EVIDENCE_RECORD_BYTES{10 * 8};

std::vector<unsigned char> EncodeEvidence(const StorageProviderEvidence& e)
{
    std::vector<unsigned char> out;
    out.reserve(EVIDENCE_RECORD_BYTES);
    for (const std::uint64_t value : {e.receipts, e.successes, e.failures, e.full_verifications,
             static_cast<std::uint64_t>(e.last_success_ms), static_cast<std::uint64_t>(e.last_failure_ms),
             static_cast<std::uint64_t>(e.last_full_verification_ms), e.verified_unit_seconds,
             e.shadow_reward.cybou, e.shadow_reward.remainder}) {
        for (unsigned i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    }
    return out;
}

std::optional<StorageProviderEvidence> DecodeEvidence(std::span<const unsigned char> bytes)
{
    if (bytes.size() != EVIDENCE_RECORD_BYTES) return std::nullopt;
    std::array<std::uint64_t, 10> v{};
    for (std::size_t field{0}; field < v.size(); ++field) {
        for (unsigned i{0}; i < 8; ++i) v[field] |= std::uint64_t{bytes[field * 8 + i]} << (8 * i);
    }
    if (v[9] >= STORAGE_RENT_DENOMINATOR) return std::nullopt;
    return StorageProviderEvidence{.receipts = v[0], .successes = v[1], .failures = v[2], .full_verifications = v[3],
        .last_success_ms = static_cast<std::int64_t>(v[4]), .last_failure_ms = static_cast<std::int64_t>(v[5]),
        .last_full_verification_ms = static_cast<std::int64_t>(v[6]), .verified_unit_seconds = v[7],
        .shadow_reward = {.cybou = v[8], .remainder = v[9]}};
}

std::int64_t NowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/// Экономическая идентичность provider'а: payout-аккаунт, а без binding — сам StorageId (DEC-280).
std::array<unsigned char, 32> EconomicIdentity(const StorageEndpoint& provider)
{
    return provider.payout_account.value_or(provider.storage_id);
}

/// Равномерное CSPRNG-перемешивание; false означает отказ системного RNG.
template <typename T>
bool Shuffle(std::vector<T>& items)
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

class ActivePlacementGuard {
public:
    ActivePlacementGuard(std::unique_lock<std::mutex>& lock,
                         std::set<cybou::Hash256>& active,
                         std::condition_variable& cv,
                         const cybou::Hash256& id)
        : m_lock{lock}, m_active{active}, m_cv{cv}, m_id{id}
    {
        m_active.insert(m_id);
    }

    ~ActivePlacementGuard()
    {
        if (!m_lock.owns_lock()) {
            m_lock.lock();
        }
        m_active.erase(m_id);
        m_cv.notify_all();
    }

    ActivePlacementGuard(const ActivePlacementGuard&) = delete;
    ActivePlacementGuard& operator=(const ActivePlacementGuard&) = delete;

private:
    std::unique_lock<std::mutex>& m_lock;
    std::set<cybou::Hash256>& m_active;
    std::condition_variable& m_cv;
    const cybou::Hash256 m_id;
};

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
    for (auto& peer : m_runtime.StorageEndpoints()) {
        StorageEndpoint endpoint{peer.storage_id, peer.address, peer.port};
        if (peer.payout_account) {
            std::array<unsigned char, 32> account{};
            std::copy(peer.payout_account->Value().begin(), peer.payout_account->Value().end(), account.begin());
            endpoint.payout_account = account;
        }
        if (!HasProvider(providers, endpoint)) providers.push_back(std::move(endpoint));
    }
    return providers;
}

std::optional<ChunkAdmissionResult> RuntimeStorageTransport::Put(const StorageEndpoint& provider,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
    const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    return m_runtime.PutChunkToStorageEndpoint(provider.address, provider.port, provider.storage_id, publication_operation_id,
        chunk_id, stored_bytes, proof);
}

std::optional<std::vector<unsigned char>> RuntimeStorageTransport::Get(const StorageEndpoint& provider,
    const ChunkId& chunk_id)
{
    return m_runtime.GetChunkFromStorageEndpoint(provider.address, provider.port, provider.storage_id, chunk_id);
}

std::optional<ChunkAuthorizationProof> RuntimeStorageTransport::GetProof(const StorageEndpoint& provider,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id)
{
    return m_runtime.GetChunkAuthorizationProofFromStorageEndpoint(provider.address, provider.port,
        provider.storage_id, publication_operation_id, chunk_id);
}

std::optional<StorageAuditAnswer> RuntimeStorageTransport::Audit(const StorageEndpoint& provider,
    const StorageAuditChallenge& challenge)
{
    return m_runtime.AuditChunkAtStorageEndpoint(provider.address, provider.port, provider.storage_id, challenge);
}

/* ---- StorageService ---- */

struct StorageService::Placement {
    cybou::Hash256 operation_id;
    /** Точный leaf-order finalized публикации; именно он нужен для Merkle proof каждого чанка. */
    std::vector<ChunkId> leaves;
    /** Удалённые providers, подтвердившие leaf; локальная копия здесь никогда не учитывается. */
    std::vector<std::vector<StorageEndpoint>> replicas;
};

StorageService::StorageService(CybouNodeRuntime& runtime, StorageTransport& transport,
    PrivateApplicationStore& application_db, const std::uint8_t remote_replica_target)
    : m_runtime{runtime}, m_transport{transport}, m_application_db{application_db},
      m_target{std::clamp<std::uint8_t>(remote_replica_target, 1, MAX_REPLICAS_PER_CHUNK)}
{
    LoadEvidence();
}

std::optional<StorageService::Placement> StorageService::Load(const cybou::Hash256& operation_id, const bool rebuilding) const
{
    const auto encoded = m_application_db.Get(rebuilding ? "storage/rebuild/"+operation_id.GetHex() : PlacementKey(operation_id));
    if (!encoded) return std::nullopt;
    Reader in{*encoded};
    std::array<unsigned char, MAGIC.size()> magic{};
    Placement placement;
    if (!in.Take(magic) || magic != MAGIC || !in.Take(std::span{placement.operation_id.begin(), 32}) ||
        placement.operation_id != operation_id) return std::nullopt;
    const auto count = in.U32();
    if (!count || *count == 0 || *count > MAX_PUBLICATION_CHUNKS) return std::nullopt;
    placement.leaves.resize(*count);
    placement.replicas.resize(*count);
    for (std::uint32_t i{0}; i < *count; ++i) {
        if (!in.Take(placement.leaves[i])) return std::nullopt;
        const auto replicas = in.U8();
        if (!replicas || *replicas > MAX_REPLICAS_PER_CHUNK) return std::nullopt;
        placement.replicas[i].reserve(*replicas);
        for (std::uint32_t r{0}; r < *replicas; ++r) {
            StorageEndpoint endpoint;
            if (!in.Take(endpoint.storage_id)) return std::nullopt;
            const auto length = in.U8();
            if (!length || *length == 0) return std::nullopt;
            std::string address(*length, '\0');
            if (!in.Take(std::span{reinterpret_cast<unsigned char*>(address.data()), address.size()})) {
                return std::nullopt;
            }
            const auto port = in.U16();
            if (!port || *port == 0) return std::nullopt;
            endpoint.address = std::move(address);
            endpoint.port = static_cast<std::uint16_t>(*port);
            if (HasProvider(placement.replicas[i], endpoint)) return std::nullopt;
            placement.replicas[i].push_back(std::move(endpoint));
        }
    }
    if (!in.Done()) return std::nullopt;
    return placement;
}

bool StorageService::Save(const Placement& placement, const bool rebuilding)
{
    std::size_t reserve = MAGIC.size() + placement.operation_id.size() + 4;
    for (const auto& replicas : placement.replicas) {
        reserve += 32 + 1;
        for (std::size_t r{0}; r < replicas.size() && r < MAX_REPLICAS_PER_CHUNK; ++r) {
            reserve += 32 + 1 + replicas[r].address.size() + 2;
        }
    }
    std::vector<unsigned char> out;
    out.reserve(reserve);
    out.insert(out.end(), MAGIC.begin(), MAGIC.end());
    out.insert(out.end(), placement.operation_id.begin(), placement.operation_id.end());
    Append32(out, static_cast<std::uint32_t>(placement.leaves.size()));
    for (std::size_t i{0}; i < placement.leaves.size(); ++i) {
        out.insert(out.end(), placement.leaves[i].begin(), placement.leaves[i].end());
        const auto& replicas = placement.replicas[i];
        out.push_back(static_cast<unsigned char>(std::min(replicas.size(), MAX_REPLICAS_PER_CHUNK)));
        for (std::size_t r{0}; r < replicas.size() && r < MAX_REPLICAS_PER_CHUNK; ++r) {
            const auto& address = replicas[r].address;
            if (address.empty() || address.size() > 255) return false;
            out.insert(out.end(), replicas[r].storage_id.begin(), replicas[r].storage_id.end());
            out.push_back(static_cast<unsigned char>(address.size()));
            out.insert(out.end(), address.begin(), address.end());
            Append16(out, replicas[r].port);
        }
    }
    // Placement и его присутствие в индексе должны фиксироваться как одна логическая запись.
    if (rebuilding) return m_application_db.Put("storage/rebuild/"+placement.operation_id.GetHex(), out);
    PrivateApplicationStore::Batch batch{m_application_db};
    if (!m_application_db.Put(PlacementKey(placement.operation_id), out)) return false;
    auto index = PlacementIndex();
    if (std::find(index.begin(), index.end(), placement.operation_id) == index.end()) {
        std::vector<unsigned char> encoded;
        for (const auto& id : index) encoded.insert(encoded.end(), id.begin(), id.end());
        encoded.insert(encoded.end(), placement.operation_id.begin(), placement.operation_id.end());
        if (!m_application_db.Put(PLACEMENT_INDEX_KEY, encoded)) return false;
    }
    return batch.Commit();
}

std::vector<cybou::Hash256> StorageService::PlacementIndex() const
{
    std::vector<cybou::Hash256> ids;
    const auto encoded = m_application_db.Get(PLACEMENT_INDEX_KEY);
    if (!encoded || encoded->size() % 32 != 0) return ids;
    for (std::size_t offset{0}; offset < encoded->size(); offset += 32) {
        cybou::Hash256 id;
        std::copy_n(encoded->begin() + static_cast<std::ptrdiff_t>(offset), 32, id.begin());
        ids.push_back(id);
    }
    return ids;
}

bool StorageService::Track(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    const auto index = PlacementIndex();
    if (std::find(index.begin(), index.end(), operation_id) != index.end()) return true;
    const auto placement = Load(operation_id);
    return placement && Save(*placement);
}

PublicationDurability StorageService::Summarize(const Placement& placement) const
{
    PublicationDurability result;
    result.chunk_count = static_cast<std::uint32_t>(placement.leaves.size());
    result.min_replicas = std::numeric_limits<std::uint32_t>::max();
    for (const auto& replicas : placement.replicas) {
        std::set<std::array<unsigned char, 32>> unique;
        for (const auto& r : replicas) unique.insert(r.storage_id);
        const auto count = static_cast<std::uint32_t>(unique.size());
        result.min_replicas = std::min(result.min_replicas, count);
        if (count >= m_target) ++result.chunks_at_target;
    }
    if (placement.leaves.empty()) result.min_replicas = 0;
    result.state = result.chunk_count > 0 && result.chunks_at_target == result.chunk_count
        ? DurabilityState::PROTECTED : DurabilityState::SECURING;
    return result;
}

PublicationDurability StorageService::Secure(const cybou::Hash256& operation_id, const std::span<const ChunkId> leaves)
{
    std::unique_lock lock{m_mutex};
    if (!m_application_db.IsUnlocked()) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Application DB is locked"};
    }
    // Размещаем только finalized публикации и только с их точным набором leaves.
    // Это соответствует finality-first admission из docs/cybou/STORAGE_ADMISSION.md:
    // Validation сама по себе не даёт права на placement.
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication) return {.state = DurabilityState::SECURING, .error = "Publication is not finalized yet"};
    if (leaves.empty() || leaves.size() > MAX_PUBLICATION_CHUNKS || leaves.size() != publication->chunk_count) {
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
        if (auto log = m_runtime.EventLog()) log->Write(NodeEvent::placement_created,{{"operation_id",operation_id.GetHex()}});
    }
    return Place(lock, *placement);
}

std::optional<ChunkAuthorizationProof> StorageService::GetAuthorizationProof(
    const cybou::Hash256& operation_id, const ChunkId& chunk_id)
{
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication) return std::nullopt;
    for (const auto& provider : m_transport.Providers()) {
        const auto proof = m_transport.GetProof(provider, operation_id, chunk_id);
        // Доверяем не provider-ответу самому по себе, а локальной повторной проверке proof против finalized state.
        if (proof && VerifyChunkAuthorizationProof(*publication, chunk_id, *proof)) return proof;
    }
    return std::nullopt;
}

PublicationDurability StorageService::Rebuild(const cybou::Hash256& operation_id,
    const std::span<const ChunkId> candidate_chunks)
{
    std::unique_lock lock{m_mutex};
    if (!m_application_db.IsUnlocked()) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Application DB is locked"};
    }
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication) return {.state = DurabilityState::SECURING, .error = "Publication is not finalized yet"};
    if (publication->chunk_count == 0 || publication->chunk_count > MAX_PUBLICATION_CHUNKS ||
        candidate_chunks.empty() || candidate_chunks.size() > MAX_PUBLICATION_CHUNKS) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Candidate chunk set is invalid"};
    }

    Placement placement{.operation_id = operation_id,
        .leaves = std::vector<ChunkId>(publication->chunk_count),
        .replicas = std::vector<std::vector<StorageEndpoint>>(publication->chunk_count)};
    if (const auto progress=Load(operation_id, true); progress && progress->leaves.size()==publication->chunk_count)
        placement=*progress;
    std::vector<bool> found(publication->chunk_count, false);
    std::set<ChunkId> verified;
    for (std::size_t i{0}; i<placement.leaves.size(); ++i) {
        found[i]=placement.leaves[i]!=ChunkId{} && !placement.replicas[i].empty();
        if (found[i]) verified.insert(placement.leaves[i]);
    }
    std::set<ChunkId> unique;
    lock.unlock();
    const auto providers = m_transport.Providers();
    lock.lock();
    for (const auto& chunk_id : candidate_chunks) {
        if (chunk_id == ChunkId{} || !unique.insert(chunk_id).second) continue;
        if (verified.contains(chunk_id)) continue;
        for (const auto& provider : providers) {
            lock.unlock();
            const auto proof = m_transport.GetProof(provider, operation_id, chunk_id);
            lock.lock();
            // Rebuild принимает только те кандидаты, которые достижимый provider может доказать против finalized publication.
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
            verified.insert(chunk_id);
            if (!HasProvider(placement.replicas[index], provider)) placement.replicas[index].push_back(provider);
        }
        // Preserve verified progress and yield when providers are unavailable or rate limited.
        if (!verified.contains(chunk_id)) break;
    }
    if (std::find(found.begin(), found.end(), false) != found.end()) {
        if (!Save(placement, true)) return {.state=DurabilityState::NEEDS_ATTENTION, .error="Cannot save rebuild progress"};
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
    m_application_db.Erase("storage/rebuild/"+operation_id.GetHex());
    return Place(lock, placement);
}

PublicationDurability StorageService::Resume(const cybou::Hash256& operation_id)
{
    std::unique_lock lock{m_mutex};
    auto placement = Load(operation_id);
    if (!placement) return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Unknown publication placement"};
    if (!m_runtime.FindFinalizedRootPublication(operation_id)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Publication is not finalized"};
    }
    return Place(lock, *placement);
}

PublicationDurability StorageService::Place(std::unique_lock<std::mutex>& lock, Placement& placement)
{
    while (m_active_placements.contains(placement.operation_id)) {
        m_placement_cv.wait(lock);
    }
    if (const auto latest = Load(placement.operation_id)) {
        placement = *latest;
    }
    auto result = Summarize(placement);
    if (result.state == DurabilityState::PROTECTED) return result;

    ActivePlacementGuard guard{lock, m_active_placements, m_placement_cv, placement.operation_id};

    // Proof'ы пересобираются локально из сохранённого leaf-order; placement не хранит готовые Merkle paths.
    std::vector<AuthorizedChunk> chunks;
    chunks.reserve(placement.leaves.size());
    for (const auto& leaf : placement.leaves) chunks.push_back({leaf});
    const auto commitment = BuildChunkAuthorizationTree(chunks);
    if (!commitment || commitment->chunk_count != placement.leaves.size()) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot rebuild chunk authorization proofs"};
    }

    lock.unlock();
    auto providers = m_transport.Providers();
    // Равный шанс каждой экономической идентичности, а не каждому StorageId: много узлов
    // одного payout-аккаунта не умножают его долю placements (DEC-280).
    bool shuffled = Shuffle(providers);
    std::map<std::array<unsigned char, 32>, std::vector<StorageEndpoint>> groups;
    std::vector<std::array<unsigned char, 32>> group_order;
    for (auto& provider : providers) {
        auto& group = groups[EconomicIdentity(provider)];
        if (group.empty()) group_order.push_back(EconomicIdentity(provider));
        group.push_back(std::move(provider));
    }
    shuffled = shuffled && Shuffle(group_order);
    providers.clear();
    for (const auto& key : group_order) {
        for (auto& provider : groups[key]) providers.push_back(std::move(provider));
    }
    std::map<std::array<unsigned char, 32>, std::array<unsigned char, 32>> identity_of;
    for (const auto& provider : providers) identity_of.emplace(provider.storage_id, EconomicIdentity(provider));
    lock.lock();
    if (!shuffled) return {.state = DurabilityState::SECURING, .error = "System RNG failure"};

    bool changed{false};
    bool content_missing{false};
    std::string admission_error;
    for (std::size_t i{0}; i < placement.leaves.size(); ++i) {
        auto& replicas = placement.replicas[i];
        if (replicas.size() >= m_target) continue;

        // Используем локальную копию или любую здоровую удалённую копию при ремонте после eviction.
        lock.unlock();
        const auto bytes = FetchInternal(placement.leaves[i], replicas);
        lock.lock();
        if (!bytes) {
            content_missing = true;
            continue;
        }

        const auto proof = commitment->Proof(static_cast<std::uint32_t>(i));

        // Реплики одного чанка — у разных экономических идентичностей; неизвестный сейчас
        // provider считается своей собственной идентичностью.
        std::set<std::array<unsigned char, 32>> used_identities;
        for (const auto& replica : replicas) {
            const auto known = identity_of.find(replica.storage_id);
            used_identities.insert(known == identity_of.end() ? replica.storage_id : known->second);
        }
        for (const auto& provider : providers) {
            if (replicas.size() >= m_target) break;
            // Один provider key считается одной репликой независимо от числа endpoint.
            if (HasProvider(replicas, provider) || used_identities.contains(EconomicIdentity(provider))) continue;

            const auto op_id = placement.operation_id;
            const auto chunk_id = placement.leaves[i];

            lock.unlock();
            const auto admitted = m_transport.Put(provider, op_id, chunk_id, *bytes, proof);
            lock.lock();

            // STORED и ALREADY_STORED одинаково означают, что provider удерживает этот чанк.
            if (!admitted || !*admitted) {
                admission_error = "Provider " + provider.address + ':' + std::to_string(provider.port) +
                    (admitted ? " rejected chunk with status " + std::to_string(static_cast<unsigned>(admitted->status))
                              : " did not acknowledge chunk admission");
                continue;
            }
            // Реплика засчитывается только с receipt, подписанным именно этим StorageId (DEC-276).
            const auto signer = VerifyStorageReceipt(admitted->receipt, m_runtime.GetNetworkBinding(), op_id,
                chunk_id, static_cast<std::uint32_t>(bytes->size()));
            if (!signer || *signer != provider.storage_id) {
                admission_error = "Provider " + provider.address + ':' + std::to_string(provider.port) +
                    " returned no valid storage receipt";
                continue;
            }
            RecordEvidence(provider.storage_id, [](StorageProviderEvidence& e) { ++e.receipts; });
            // Receipt открывает интервал хранения; следующая успешная проверка его засчитывает.
            CreditReplica(provider.storage_id, chunk_id, bytes->size(), NowMs());
            if (!HasProvider(replicas, provider)) {
                if (!SaveReceipt(op_id, chunk_id, provider, admitted->receipt)) {
                    admission_error = "Cannot save storage receipt";
                    continue;
                }
                replicas.push_back(provider);
                used_identities.insert(EconomicIdentity(provider));
                changed = true;
                (void)Save(placement);
            }
        }
    }
    if (changed && !Save(placement)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save placement state"};
    }
    result = Summarize(placement);
    if (result.state != DurabilityState::PROTECTED) {
        result.error = content_missing ? "Some encrypted content is temporarily unavailable"
            : providers.size() < m_target ? "Not enough storage providers are reachable"
            : admission_error.empty() ? "Storage providers did not accept every chunk yet" : admission_error;
    }
    if (auto log = m_runtime.EventLog()) log->Write(result.state == DurabilityState::PROTECTED ? NodeEvent::content_protected : NodeEvent::content_securing,
        {{"operation_id",placement.operation_id.GetHex()},{"replicas",std::uint64_t{result.min_replicas}},{"target",std::uint64_t{m_target}}});
    return result;
}

std::optional<std::pair<cybou::Hash256, PublicationDurability>> StorageService::AuditNextPlacement(const std::size_t max_chunks)
{
    std::unique_lock lock{m_mutex};
    const auto index = PlacementIndex();
    if (index.empty()) return std::nullopt;
    const auto operation_id = index[m_audit_placement_cursor++ % index.size()];
    if (auto log = m_runtime.EventLog()) log->Write(NodeEvent::storage_audit_started,{{"operation_id",operation_id.GetHex()}});
    auto placement = Load(operation_id);
    if (!placement || placement->leaves.empty()) return std::nullopt;
    const std::size_t count = placement->leaves.size();
    auto& cursor = m_audit_cursor[operation_id];
    bool changed{false};
    for (std::size_t checked{0}; checked < std::min(max_chunks, count); ++checked) {
        const std::size_t i = cursor % count;
        cursor = (cursor + 1) % count;
        auto& replicas = placement->replicas[i];
        const auto chunk_id = placement->leaves[i];
        std::vector<StorageEndpoint> healthy;
        healthy.reserve(replicas.size());
        lock.unlock();
        // Локальная копия позволяет проверить дешёвый random-offset ответ без выгрузки чанка.
        auto local = m_runtime.GetChunkBlobStore().Get(chunk_id);
        if (local && ComputeChunkId(*local) != chunk_id) local.reset();
        for (const auto& provider : replicas) {
            if (CheckReplica(provider, chunk_id, local, false)) healthy.push_back(provider);
            else EraseReceipt(operation_id, chunk_id, provider);
        }
        lock.lock();
        if (healthy.size() != replicas.size()) {
            replicas = std::move(healthy);
            changed = true;
        }
    }
    if (changed && !Save(*placement)) {
        return std::pair{operation_id, PublicationDurability{.state = DurabilityState::NEEDS_ATTENTION,
            .error = "Cannot save placement state"}};
    }
    auto result = Summarize(*placement);
    const bool degraded = result.state != DurabilityState::PROTECTED;
    if (degraded) if (auto log = m_runtime.EventLog()) log->Write(NodeEvent::placement_degraded,
        {{"operation_id",operation_id.GetHex()},{"replicas",std::uint64_t{result.min_replicas}},{"target",std::uint64_t{m_target}}});
    // Если ушли ниже target, сразу ремонтируем из любой валидной копии: локальной или удалённой.
    if (result.state != DurabilityState::PROTECTED && m_runtime.FindFinalizedRootPublication(operation_id)) {
        result = Place(lock, *placement);
    }
    if (degraded) if (auto log = m_runtime.EventLog()) log->Write(result.state == DurabilityState::PROTECTED ? NodeEvent::placement_repaired : NodeEvent::storage_audit_failed,
        {{"operation_id",operation_id.GetHex()},{"replicas",std::uint64_t{result.min_replicas}},{"target",std::uint64_t{m_target}}});
    return std::pair{operation_id, result};
}

PublicationDurability StorageService::Audit(const cybou::Hash256& operation_id)
{
    std::unique_lock lock{m_mutex};
    auto placement = Load(operation_id);
    if (!placement) return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Unknown publication placement"};
    bool changed{false};
    for (std::size_t i{0}; i < placement->leaves.size(); ++i) {
        auto& replicas = placement->replicas[i];
        const auto chunk_id = placement->leaves[i];
        std::vector<StorageEndpoint> healthy;
        healthy.reserve(replicas.size());
        lock.unlock();
        // Полный audit публикации всегда проверяет exact bytes полным GET.
        for (const auto& provider : replicas) {
            if (CheckReplica(provider, chunk_id, std::nullopt, true)) healthy.push_back(provider);
            else EraseReceipt(operation_id, chunk_id, provider);
        }
        lock.lock();
        if (healthy.size() != replicas.size()) {
            replicas = std::move(healthy);
            changed = true;
        }
    }
    if (changed && !Save(*placement)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save placement state"};
    }
    if (!m_runtime.FindFinalizedRootPublication(operation_id)) return Summarize(*placement);
    return Place(lock, *placement);
}

std::optional<PublicationDurability> StorageService::GetDurability(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    const auto placement = Load(operation_id);
    if (!placement) return std::nullopt;
    return Summarize(*placement);
}

std::optional<StorageService::PlacementView> StorageService::DescribePlacement(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    auto placement = Load(operation_id);
    if (!placement) return std::nullopt;
    return PlacementView{std::move(placement->leaves), std::move(placement->replicas)};
}

bool StorageService::CheckReplica(const StorageEndpoint& provider, const ChunkId& chunk_id,
    const std::optional<std::vector<unsigned char>>& local_bytes, const bool force_full)
{
    // Отказ RNG не наказывает provider: проверка просто становится полным GET.
    std::uint32_t draw{0};
    std::uint64_t offset_draw{0};
    StorageAuditChallenge challenge{.chunk_id = chunk_id};
    const bool full = force_full || !local_bytes || local_bytes->empty() ||
        RAND_bytes(reinterpret_cast<unsigned char*>(&draw), sizeof(draw)) != 1 ||
        RAND_bytes(reinterpret_cast<unsigned char*>(&offset_draw), sizeof(offset_draw)) != 1 ||
        RAND_bytes(challenge.nonce.data(), challenge.nonce.size()) != 1 ||
        draw % STORAGE_FULL_GET_ONE_IN == 0;
    if (!full) {
        challenge.byte_offset = offset_draw % local_bytes->size();
        const auto expected = ComputeStorageAuditResponse(*local_bytes, challenge.byte_offset, challenge.nonce);
        if (const auto answer = m_transport.Audit(provider, challenge)) {
            const bool ok = expected && answer->held && answer->response_hash == *expected;
            const auto now = NowMs();
            RecordEvidence(provider.storage_id, [&](StorageProviderEvidence& e) {
                if (ok) { ++e.successes; e.last_success_ms = now; }
                else { ++e.failures; e.last_failure_ms = now; }
            });
            if (ok) CreditReplica(provider.storage_id, chunk_id, local_bytes->size(), now);
            else ForgetReplica(provider.storage_id, chunk_id);
            return ok;
        }
        // Transport без audit или без ответа: проверяем exact bytes полным GET.
    }
    const auto bytes = m_transport.Get(provider, chunk_id);
    const bool ok = bytes && ComputeChunkId(*bytes) == chunk_id;
    const auto now = NowMs();
    RecordEvidence(provider.storage_id, [&](StorageProviderEvidence& e) {
        if (ok) { ++e.successes; ++e.full_verifications; e.last_success_ms = now; e.last_full_verification_ms = now; }
        else { ++e.failures; e.last_failure_ms = now; }
    });
    if (ok) CreditReplica(provider.storage_id, chunk_id, bytes->size(), now);
    else ForgetReplica(provider.storage_id, chunk_id);
    return ok;
}

void StorageService::CreditReplica(const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id,
    const std::uint64_t stored_bytes, const std::int64_t now_ms)
{
    // Засчитываем только интервал между двумя успешными проверками и не длиннее суток:
    // долгий пропуск не доказывает непрерывное хранение.
    std::int64_t previous{0};
    {
        std::lock_guard lock{m_evidence_mutex};
        const auto key = std::pair{chunk_id, storage_id};
        if (const auto it = m_replica_verified_ms.find(key); it != m_replica_verified_ms.end()) previous = it->second;
        else if (m_replica_verified_ms.size() >= MAX_TRACKED_REPLICA_CHECKS) m_replica_verified_ms.clear();
        m_replica_verified_ms[key] = now_ms;
    }
    if (previous == 0 || now_ms <= previous) return;
    const auto seconds = static_cast<std::uint64_t>(std::min(now_ms - previous, STORAGE_MAX_CREDITED_GAP_MS) / 1000);
    const auto units = StorageBillingUnits(stored_bytes);
    if (seconds == 0) return;
    RecordEvidence(storage_id, [&](StorageProviderEvidence& e) {
        auto reward = e.shadow_reward;
        if (units > std::numeric_limits<std::uint64_t>::max() / seconds ||
            e.verified_unit_seconds > std::numeric_limits<std::uint64_t>::max() - units * seconds ||
            !AccrueStorageRent(reward, units, seconds, 1)) return;
        e.verified_unit_seconds += units * seconds;
        e.shadow_reward = reward;
    });
}

void StorageService::ForgetReplica(const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id)
{
    std::lock_guard lock{m_evidence_mutex};
    m_replica_verified_ms.erase(std::pair{chunk_id, storage_id});
}

std::optional<std::uint64_t> StorageService::EstimatedDailyRent()
{
    std::uint64_t units{0};
    {
        std::lock_guard lock{m_mutex};
        for (const auto& operation_id : PlacementIndex()) {
            const auto placement = Load(operation_id);
            if (!placement) continue;
            // Один billing unit на authorized chunk (DEC-279).
            if (units > std::numeric_limits<std::uint64_t>::max() - placement->leaves.size()) return std::nullopt;
            units += placement->leaves.size();
        }
    }
    return StorageRentPerDay(units, m_target);
}

void StorageService::RecordEvidence(const std::array<unsigned char, 32>& storage_id,
    const std::function<void(StorageProviderEvidence&)>& update)
{
    std::lock_guard lock{m_evidence_mutex};
    auto it = m_evidence.find(storage_id);
    if (it == m_evidence.end()) {
        if (m_evidence.size() >= MAX_TRACKED_STORAGE_PROVIDERS) {
            // Вытесняем provider с самой старой активностью: evidence ограничена и не является state.
            const auto oldest = std::min_element(m_evidence.begin(), m_evidence.end(), [](const auto& a, const auto& b) {
                return std::max(a.second.last_success_ms, a.second.last_failure_ms) <
                    std::max(b.second.last_success_ms, b.second.last_failure_ms);
            });
            (void)m_application_db.Erase(EvidenceKey(oldest->first));
            m_evidence.erase(oldest);
        }
        it = m_evidence.emplace(storage_id, StorageProviderEvidence{}).first;
        SaveEvidenceIndex();
    }
    update(it->second);
    // Evidence переживает рестарт, чтобы shadow accounting копил реальные интервалы (M4).
    (void)m_application_db.Put(EvidenceKey(storage_id), EncodeEvidence(it->second));
}

void StorageService::SaveEvidenceIndex()
{
    std::vector<unsigned char> index;
    index.reserve(m_evidence.size() * 32);
    for (const auto& [storage_id, _] : m_evidence) index.insert(index.end(), storage_id.begin(), storage_id.end());
    (void)m_application_db.Put(EVIDENCE_INDEX_KEY, index);
}

void StorageService::LoadEvidence()
{
    std::lock_guard lock{m_evidence_mutex};
    const auto index = m_application_db.Get(EVIDENCE_INDEX_KEY);
    if (!index || index->size() % 32 != 0) return;
    for (std::size_t offset{0}; offset < index->size() && m_evidence.size() < MAX_TRACKED_STORAGE_PROVIDERS;
         offset += 32) {
        std::array<unsigned char, 32> storage_id{};
        std::copy_n(index->begin() + offset, 32, storage_id.begin());
        const auto bytes = m_application_db.Get(EvidenceKey(storage_id));
        if (const auto evidence = bytes ? DecodeEvidence(*bytes) : std::nullopt) m_evidence.emplace(storage_id, *evidence);
    }
}

std::map<std::array<unsigned char, 32>, StorageProviderEvidence> StorageService::ProviderEvidence()
{
    std::lock_guard lock{m_evidence_mutex};
    return m_evidence;
}

bool StorageService::SaveReceipt(const cybou::Hash256& operation_id, const ChunkId& chunk_id,
    const StorageEndpoint& provider, const std::span<const unsigned char> receipt)
{
    return m_application_db.Put(ReceiptKey(operation_id, chunk_id, provider.storage_id), receipt);
}

void StorageService::EraseReceipt(const cybou::Hash256& operation_id, const ChunkId& chunk_id,
    const StorageEndpoint& provider)
{
    (void)m_application_db.Erase(ReceiptKey(operation_id, chunk_id, provider.storage_id));
}

std::optional<std::vector<unsigned char>> StorageService::Fetch(const ChunkId& chunk_id)
{
    return FetchInternal(chunk_id, {});
}

std::optional<std::vector<unsigned char>> StorageService::FetchInternal(const ChunkId& chunk_id,
    const std::span<const StorageEndpoint> preferred)
{
    const auto now_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    if (auto local = m_runtime.GetChunkBlobStore().Get(chunk_id); local && ComputeChunkId(*local) == chunk_id) {
        (void)m_runtime.GetChunkRetention().NoteCacheUse(chunk_id, now_ms);
        return local;
    }
    std::vector<StorageEndpoint> candidates{preferred.begin(), preferred.end()};
    auto others = m_transport.Providers();
    if (!Shuffle(others)) return std::nullopt;
    for (auto& provider : others) {
        if (!HasProvider(candidates, provider)) {
            candidates.push_back(std::move(provider));
        }
    }
    for (const auto& provider : candidates) {
        auto bytes = m_transport.Get(provider, chunk_id);
        if (!bytes || ComputeChunkId(*bytes) != chunk_id) continue;
        // Кешируем уже проверенный ciphertext; fetch не создаёт новых provider-обязательств и не меняет финализацию.
        (void)m_runtime.GetChunkBlobStore().Put(chunk_id, *bytes);
        (void)m_runtime.GetChunkRetention().NoteCacheUse(chunk_id, now_ms);
        return bytes;
    }
    return std::nullopt;
}

} // namespace cybou
