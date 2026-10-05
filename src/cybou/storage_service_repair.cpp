// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// \brief Placement, audit and repair coordination without network I/O under placement lock.

#include <cybou/storage_service_internal.h>
#include <cybou/node_runtime.h>
#include <algorithm>

namespace cybou {

namespace {
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

PublicationDurability StorageService::Place(std::unique_lock<std::mutex>& lock, Placement& placement)
{
    while (m_active_placements.contains(placement.operation_id)) {
        m_placement_cv.wait(lock);
    }
    if (const auto latest = m_placements->Load(placement.operation_id)) {
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
    const bool shuffled = ProviderSelector::OrderProviders(providers);
    std::map<std::array<unsigned char, 32>, std::array<unsigned char, 32>> identity_of;
    for (const auto& provider : providers) identity_of.emplace(provider.storage_id, ProviderSelector::EconomicIdentity(provider));
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
            if (HasProvider(replicas, provider) || used_identities.contains(ProviderSelector::EconomicIdentity(provider))) continue;

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
            m_evidence->RecordEvidence(provider.storage_id, [](StorageProviderEvidence& e) { ++e.receipts; });
            // Receipt открывает интервал хранения; следующая успешная проверка его засчитывает.
            m_evidence->CreditReplica(provider.storage_id, chunk_id, bytes->size(), StorageEvidenceNowMs());
            if (!HasProvider(replicas, provider)) {
                if (!m_evidence->SaveReceipt(op_id, chunk_id, provider, admitted->receipt)) {
                    admission_error = "Cannot save storage receipt";
                    continue;
                }
                replicas.push_back(provider);
                used_identities.insert(ProviderSelector::EconomicIdentity(provider));
                changed = true;
                (void)m_placements->Save(placement);
            }
        }
    }
    if (changed && !m_placements->Save(placement)) {
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
    const auto index = m_placements->PlacementIndex();
    if (index.empty()) return std::nullopt;
    const auto operation_id = index[m_audit_placement_cursor++ % index.size()];
    if (auto log = m_runtime.EventLog()) log->Write(NodeEvent::storage_audit_started,{{"operation_id",operation_id.GetHex()}});
    auto placement = m_placements->Load(operation_id);
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
            if (m_verifier->CheckReplica(provider, chunk_id, local, false)) healthy.push_back(provider);
            else m_evidence->EraseReceipt(operation_id, chunk_id, provider);
        }
        lock.lock();
        if (healthy.size() != replicas.size()) {
            replicas = std::move(healthy);
            changed = true;
        }
    }
    if (changed && !m_placements->Save(*placement)) {
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
    auto placement = m_placements->Load(operation_id);
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
            if (m_verifier->CheckReplica(provider, chunk_id, std::nullopt, true)) healthy.push_back(provider);
            else m_evidence->EraseReceipt(operation_id, chunk_id, provider);
        }
        lock.lock();
        if (healthy.size() != replicas.size()) {
            replicas = std::move(healthy);
            changed = true;
        }
    }
    if (changed && !m_placements->Save(*placement)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save placement state"};
    }
    if (!m_runtime.FindFinalizedRootPublication(operation_id)) return Summarize(*placement);
    return Place(lock, *placement);
}

} // namespace cybou
