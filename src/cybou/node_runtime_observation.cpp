// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/node_runtime.h>
#include <cybou/network_observation.h>
#include <cybou/observation_cache.h>
#include <cybou/process_memory.h>
#include <cybou/p2p/observation_exchange.h>
#include <cmath>
#include <limits>
namespace cybou {
std::array<unsigned char, 191> CybouNodeRuntime::ReadObservationReport(const std::array<unsigned char, 32>& challenge) const
{
    return m_observation_collector->Read(challenge);
}
std::shared_ptr<const NetworkObservationSnapshot> CybouNodeRuntime::GetNetworkObservation() const
{
    auto snapshot = std::make_shared<NetworkObservationSnapshot>();
    std::copy(m_network_binding.begin(), m_network_binding.end(), snapshot->network_binding.begin());
    snapshot->observed_unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto local = DecodeObservationReport(ReadObservationReport({}));
    snapshot->local = {true, 0, local.cache_age_ms, local.cursor, local.storage, local.traffic, local.cpu, local.memory};
    snapshot->remote = m_observation_groups->Snapshot();
    snapshot->remote_history = m_network_observation_history->Snapshot();
    return snapshot;
}
ObservationReport CybouNodeRuntime::CollectObservationReport() const
{
    m_observation_exchange->Expire();
    m_network_observation_history->Observe(m_observation_groups->Snapshot());
    ObservationReport r;
    {
        // Contended chain work reduces coverage instead of delaying the sampler.
        std::unique_lock lock{m_chain.mutex, std::try_to_lock};
        if (lock.owns_lock()) {
            // Initialization atomically writes the network binding and head.
            // Read only these fixed metadata keys, never load/deserialize state.
            if (m_chain.store.GetStoredNetworkBinding() == m_network_binding) {
                if (const auto head = m_chain.store.GetFinalizedHead()) {
                    r.cursor.known = true; r.cursor.height = head->height;
                    std::copy(head->block_id.begin(), head->block_id.end(), r.cursor.tip.begin());
                }
            }
        }
    }
    const auto capacity = m_config.storage_capacity_bytes.value_or(0);
    if (capacity >= (uint64_t{15} << 30) && m_provider.chunk_blob_store && m_provider.finalized_chunk_store) {
        const auto provider_used = m_provider.finalized_chunk_store->UsedBytes();
        // UsedBytes uses UINT64_MAX as the unreadable-counter sentinel.
        if (provider_used != std::numeric_limits<uint64_t>::max()) r.storage = {true, capacity,
            m_provider.chunk_blob_store->UsedBytes() >> 20, m_provider.finalized_chunk_store->CapacityBytes(), provider_used >> 20};
    }
    const auto traffic = m_traffic->WindowSnapshot();
    if (traffic.window_ms == 60000) r.traffic = {true, 60000, traffic.window_received_bytes >> 10, traffic.window_sent_bytes >> 10};
    const auto cpu = m_cpu_observations.Sample();
    if (cpu.mean_percent && std::isfinite(*cpu.mean_percent) && *cpu.mean_percent >= 0 && *cpu.mean_percent <= 100 &&
        cpu.processors >= 1 && cpu.processors <= 65536 && cpu.mean_window_ms >= 60000 && cpu.mean_window_ms <= 120000 &&
        cpu.mean_intervals >= 1 && cpu.mean_intervals <= 120000 && cpu.mean_age_ms <= 60000) {
        r.cpu = {true, cpu.processors, static_cast<uint32_t>(cpu.mean_window_ms), static_cast<uint32_t>(cpu.mean_intervals),
            static_cast<uint32_t>(cpu.mean_age_ms), static_cast<uint16_t>(std::lround(*cpu.mean_percent * 100))};
    }
    if (const auto memory = ReadProcessResidentBytes()) r.memory = {true, *memory >> 20};
    return r;
}
}
