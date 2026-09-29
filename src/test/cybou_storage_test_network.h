// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STORAGE_TEST_NETWORK_H
#define CYBOU_STORAGE_TEST_NETWORK_H

#include <cybou/node_runtime.h>
#include <cybou/storage_service.h>
#include <test/cybou_service_test_fixture.h>

#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * Real provider runtimes behind an in-process transport. Providers learn
 * finality the way full nodes do, by committing the authority's blocks, so
 * admission, proof checks and ALREADY_STORED behave as on the network.
 */
class ProviderNetwork final : public cybou::StorageTransport {
public:
    explicit ProviderNetwork(CybouServiceTestFixture& fixture, int count = 3) : m_fixture{fixture}
    {
        for (int i{0}; i < count; ++i) {
            cybou::NodeRuntimeConfig config{
                .network_definition = fixture.definition,
                .data_dir = fixture.directory / ("provider-" + std::to_string(i)),
                .memory_only = true,
                .wipe_data = true,
                .storage_enabled = true,
                .storage_capacity_bytes = 64ULL << 20,
            };
            auto provider = std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
            if (!provider->InitializeGenesis(fixture.genesis)) throw std::runtime_error{"provider genesis failed"};
            m_providers.emplace(cybou::StorageEndpoint{"10.0.0." + std::to_string(i + 1), 7070}, std::move(provider));
        }
    }

    void Sync()
    {
        const auto height = m_fixture.runtime->GetFinalizedHeight().value_or(0);
        for (auto& [endpoint, provider] : m_providers) {
            if (lagging.contains(endpoint)) continue;
            for (auto h = provider->GetFinalizedHeight().value_or(0) + 1; h <= height; ++h) {
                const auto block = m_fixture.runtime->GetBlockAtHeight(h);
                if (!block || !provider->CommitBlock(*block)) throw std::runtime_error{"provider sync failed"};
            }
        }
    }

    std::vector<cybou::StorageEndpoint> Providers() override
    {
        std::vector<cybou::StorageEndpoint> out;
        for (const auto& [endpoint, _] : m_providers) {
            if (!offline.contains(endpoint)) out.push_back(endpoint);
        }
        return out;
    }

    std::optional<cybou::ChunkAdmissionResult> Put(const cybou::StorageEndpoint& provider,
        const uint256& op_id, const cybou::ChunkId& chunk_id, std::span<const unsigned char> bytes,
        const cybou::ChunkAuthorizationProof& proof) override
    {
        ++puts;
        if (offline.contains(provider)) return std::nullopt;
        return m_providers.at(provider)->PutFinalizedChunk(op_id, chunk_id, bytes, proof);
    }

    std::optional<std::vector<unsigned char>> Get(const cybou::StorageEndpoint& provider,
        const cybou::ChunkId& chunk_id) override
    {
        if (offline.contains(provider)) return std::nullopt;
        auto bytes = m_providers.at(provider)->GetFinalizedChunk(chunk_id);
        if (bytes && corrupt.contains(provider) && !bytes->empty()) (*bytes)[0] ^= 0x01;
        return bytes;
    }

    std::optional<cybou::ChunkAuthorizationProof> GetProof(const cybou::StorageEndpoint& provider,
        const uint256& operation_id, const cybou::ChunkId& chunk_id) override
    {
        if (offline.contains(provider)) return std::nullopt;
        return m_providers.at(provider)->GetFinalizedChunkAuthorizationProof(operation_id, chunk_id);
    }

    bool Holds(const cybou::StorageEndpoint& provider, const cybou::ChunkId& id) const
    {
        return m_providers.at(provider)->HasFinalizedChunk(id);
    }

    std::vector<cybou::StorageEndpoint> Endpoints() const
    {
        std::vector<cybou::StorageEndpoint> out;
        for (const auto& [endpoint, _] : m_providers) out.push_back(endpoint);
        return out;
    }

    void SetAllOffline(bool value)
    {
        offline.clear();
        if (value) {
            for (const auto& endpoint : Endpoints()) offline.insert(endpoint);
        }
    }

    std::set<cybou::StorageEndpoint> offline;
    std::set<cybou::StorageEndpoint> corrupt;
    std::set<cybou::StorageEndpoint> lagging;
    int puts{0};

private:
    CybouServiceTestFixture& m_fixture;
    std::map<cybou::StorageEndpoint, std::unique_ptr<cybou::CybouNodeRuntime>> m_providers;
};

#endif // CYBOU_STORAGE_TEST_NETWORK_H
