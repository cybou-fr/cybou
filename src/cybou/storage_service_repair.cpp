// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Placement, audit and repair coordination without network I/O under placement lock.

#include <cybou/storage_service_internal.h>
#include <cybou/node_runtime.h>
#include <cybou/storage_io_scheduler.h>
#include <boost/asio/ip/address.hpp>
#include <algorithm>

namespace cybou {

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
        // Distinct StorageIds can share one machine (several nodes behind one address): two
        // replicas on one address, or one on this very machine, would be lost together.
        const bool diverse = m_runtime.RequiresReplicaAddressDiversity();
        std::set<std::string> used_addresses;
        for (const auto& replica : replicas) used_addresses.insert(replica.address);
        const auto same_machine = [](const std::string& address) {
            boost::system::error_code ec;
            const auto ip = boost::asio::ip::make_address(address, ec);
            return !ec && ip.is_loopback();
        };
        std::set<std::array<unsigned char, 32>> attempted;
        while (replicas.size() < m_target) {
            std::vector<StorageEndpoint> plan;
            auto reserved = used_identities;
            auto reserved_addresses = used_addresses;
            for (const auto& provider : providers) {
                if (plan.size() >= std::min<size_t>(4, m_target - replicas.size())) break;
                if (attempted.contains(provider.storage_id) || HasProvider(replicas, provider) ||
                    reserved.contains(ProviderSelector::EconomicIdentity(provider))) continue;
                if (diverse && (same_machine(provider.address) || reserved_addresses.contains(provider.address))) continue;
                plan.push_back(provider);
                attempted.insert(provider.storage_id);
                reserved.insert(ProviderSelector::EconomicIdentity(provider));
                reserved_addresses.insert(provider.address);
            }
            if (plan.empty()) break;
            const auto op_id = placement.operation_id;
            const auto chunk_id = placement.leaves[i];
            std::vector<std::future<std::optional<ChunkAdmissionResult>>> pending;
            pending.reserve(plan.size());
            std::vector<std::optional<ChunkAdmissionResult>> results(plan.size());
            // Copies share one bounded encrypted chunk; never queue a whole publication.
            const auto payload = std::make_shared<const std::vector<unsigned char>>(*bytes);
            lock.unlock();
            try {
                for (const auto& provider : plan) {
                    pending.push_back(m_runtime.StorageIo().Submit(provider.storage_id, StorageIoScheduler::Kind::WRITE,
                        [this, provider, op_id, chunk_id, payload, proof] {
                            return m_transport.Put(provider, op_id, chunk_id, *payload, proof);
                        }));
                }
            } catch (...) {
                for (auto& job : pending) job.wait();
                throw;
            }
            for (size_t j = 0; j < pending.size(); ++j) {
                try { results[j] = pending[j].get(); }
                catch (...) { /* unavailable transport */ }
            }
            lock.lock();
            for (size_t j = 0; j < plan.size(); ++j) {
                const auto& provider = plan[j];
                const auto& admitted = results[j];
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
                    used_addresses.insert(provider.address);
                    changed = true;
                }
            }
            if (changed && !m_placements->Save(placement)) {
                return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save placement state"};
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

std::vector<StorageEndpoint> StorageService::CheckReplicas(const cybou::Hash256& operation_id,
    const ChunkId& chunk_id, const std::vector<StorageEndpoint>& replicas,
    const std::optional<std::vector<unsigned char>>& local, const bool force_full)
{
    const auto bytes = std::make_shared<const std::optional<std::vector<unsigned char>>>(local);
    std::vector<std::future<bool>> pending;
    pending.reserve(replicas.size());
    std::vector<StorageEndpoint> healthy;
    healthy.reserve(replicas.size());
    try {
        for (const auto& provider : replicas) {
            pending.push_back(m_runtime.StorageIo().Submit(provider.storage_id, StorageIoScheduler::Kind::READ,
                [this, provider, chunk_id, bytes, force_full] {
                    return m_verifier->CheckReplica(provider, chunk_id, *bytes, force_full);
                }));
        }
    } catch (...) {
        for (auto& job : pending) job.wait();
        throw;
    }
    for (size_t i = 0; i < pending.size(); ++i) {
        bool ok{false};
        try { ok = pending[i].get(); } catch (...) {
            m_evidence->ForgetReplica(replicas[i].storage_id, chunk_id);
            m_evidence->RecordEvidence(replicas[i].storage_id, [](StorageProviderEvidence& e) { ++e.failures; });
        }
        if (ok) healthy.push_back(replicas[i]);
        else m_evidence->EraseReceipt(operation_id, chunk_id, replicas[i]);
    }
    return healthy;
}

std::optional<std::pair<cybou::Hash256, PublicationDurability>> StorageService::AuditNextPlacement(const std::size_t max_chunks)
{
    std::unique_lock lock{m_mutex};
    const auto index = m_placements->PlacementIndex();
    if (index.empty()) return std::nullopt;
    const auto operation_id = index[m_audit_placement_cursor++ % index.size()];
    if (auto log = m_runtime.EventLog()) log->Write(NodeEvent::storage_audit_started,{{"operation_id",operation_id.GetHex()}});
    m_placement_cv.wait(lock, [&] { return !m_active_placements.contains(operation_id); });
    ActivePlacementGuard guard{lock, m_active_placements, m_placement_cv, operation_id};
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
        healthy = CheckReplicas(operation_id, chunk_id, replicas, local, false);
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
        guard.Finish();
        result = Place(lock, *placement);
    }
    if (degraded) if (auto log = m_runtime.EventLog()) log->Write(result.state == DurabilityState::PROTECTED ? NodeEvent::placement_repaired : NodeEvent::storage_audit_failed,
        {{"operation_id",operation_id.GetHex()},{"replicas",std::uint64_t{result.min_replicas}},{"target",std::uint64_t{m_target}}});
    return std::pair{operation_id, result};
}

PublicationDurability StorageService::Audit(const cybou::Hash256& operation_id)
{
    std::unique_lock lock{m_mutex};
    m_placement_cv.wait(lock, [&] { return !m_active_placements.contains(operation_id); });
    ActivePlacementGuard guard{lock, m_active_placements, m_placement_cv, operation_id};
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
        healthy = CheckReplicas(operation_id, chunk_id, replicas, std::nullopt, true);
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
    guard.Finish();
    return Place(lock, *placement);
}

} // namespace cybou
