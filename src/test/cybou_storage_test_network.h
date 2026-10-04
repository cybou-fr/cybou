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
 * finality the way full nodes do, by committing the finalizer's blocks, so
 * admission, proof checks and ALREADY_STORED behave as on the network.
 */
class ProviderNetwork final : public cybou::StorageTransport {
public:
    explicit ProviderNetwork(CybouServiceTestFixture& fixture, int count = 3) : m_fixture{fixture}
    {
        for (int i{0}; i < count; ++i) {
            cybou::NodeRuntimeConfig config{
                .network_genesis = fixture.definition,
                .data_dir = fixture.directory / ("provider-" + std::to_string(i)),
                .memory_only = true,
                .wipe_data = true,
                .storage_capacity_bytes = 64ULL << 20, .operation_work_bits = 0
            };
            auto provider = std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
            if (!provider->InitializeGenesis(fixture.genesis)) throw std::runtime_error{"provider genesis failed"};
            const auto id = provider->LocalStorageId();
            if (!id) throw std::runtime_error{"provider has no StorageId"};
            m_runtimes.push_back(std::move(provider));
            m_providers.emplace(cybou::StorageEndpoint{*id, "10.0.0." + std::to_string(i + 1), 7070},
                m_runtimes.back().get());
        }
    }

    /** A second endpoint served by the same provider key (a Sybil-style alias). */
    cybou::StorageEndpoint AddAlias(const cybou::StorageEndpoint& of, std::string address)
    {
        cybou::StorageEndpoint alias{of.storage_id, std::move(address), of.port};
        m_providers.emplace(alias, m_providers.at(of));
        return alias;
    }

    void Sync()
    {
        const auto height = m_fixture.runtime->GetFinalizedHeight().value_or(0);
        for (auto& provider : m_runtimes) {
            if (std::any_of(lagging.begin(), lagging.end(),
                    [&](const auto& e) { return m_providers.at(e) == provider.get(); })) continue;
            for (auto h = provider->GetFinalizedHeight().value_or(0) + 1; h <= height; ++h) {
                const auto block = m_fixture.runtime->GetBlockAtHeight(h);
                if (!block || !provider->CommitBlock(*block)) throw std::runtime_error{"provider sync failed"};
            }
        }
    }

    /** Endpoints stay keyed without payout account; Providers() decorates them with it. */
    static cybou::StorageEndpoint Key(cybou::StorageEndpoint endpoint)
    {
        endpoint.payout_account.reset();
        return endpoint;
    }

    std::vector<cybou::StorageEndpoint> Providers() override
    {
        std::vector<cybou::StorageEndpoint> out;
        for (const auto& [key, _] : m_providers) {
            if (offline.contains(key)) continue;
            auto endpoint = key;
            if (const auto account = payout.find(endpoint.storage_id); account != payout.end()) endpoint.payout_account = account->second;
            out.push_back(endpoint);
        }
        return out;
    }

    std::optional<cybou::ChunkAdmissionResult> Put(const cybou::StorageEndpoint& provider,
        const cybou::Hash256& op_id, const cybou::ChunkId& chunk_id, std::span<const unsigned char> bytes,
        const cybou::ChunkAuthorizationProof& proof) override
    {
        ++puts;
        if (offline.contains(Key(provider))) return std::nullopt;
        return m_providers.at(Key(provider))->PutFinalizedChunk(op_id, chunk_id, bytes, proof);
    }

    std::optional<std::vector<unsigned char>> Get(const cybou::StorageEndpoint& provider,
        const cybou::ChunkId& chunk_id) override
    {
        if (offline.contains(Key(provider))) return std::nullopt;
        auto bytes = m_providers.at(Key(provider))->GetFinalizedChunk(chunk_id);
        if (bytes && corrupt.contains(Key(provider)) && !bytes->empty()) (*bytes)[0] ^= 0x01;
        return bytes;
    }

    std::optional<cybou::ChunkAuthorizationProof> GetProof(const cybou::StorageEndpoint& provider,
        const cybou::Hash256& operation_id, const cybou::ChunkId& chunk_id) override
    {
        if (proof_budget) {
            if (*proof_budget==0) return std::nullopt;
            --*proof_budget;
        }
        if (offline.contains(Key(provider))) return std::nullopt;
        return m_providers.at(Key(provider))->GetFinalizedChunkAuthorizationProof(operation_id, chunk_id);
    }

    std::optional<cybou::StorageAuditAnswer> Audit(const cybou::StorageEndpoint& provider,
        const cybou::StorageAuditChallenge& challenge) override
    {
        ++audits;
        if (offline.contains(Key(provider))) return std::nullopt;
        auto answer = m_providers.at(Key(provider))->AnswerStorageAudit(challenge);
        if (answer.held && corrupt.contains(Key(provider))) answer.response_hash.begin()[0] ^= 0x01;
        return answer;
    }

    bool Holds(const cybou::StorageEndpoint& provider, const cybou::ChunkId& id) const
    {
        return m_providers.at(Key(provider))->HasFinalizedChunk(id);
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
    /** Verified payout account per StorageId, as the runtime transport would report it. */
    std::map<std::array<unsigned char, 32>, std::array<unsigned char, 32>> payout;
    int puts{0};
    int audits{0};
    std::optional<std::size_t> proof_budget;

private:
    CybouServiceTestFixture& m_fixture;
    std::vector<std::unique_ptr<cybou::CybouNodeRuntime>> m_runtimes;
    std::map<cybou::StorageEndpoint, cybou::CybouNodeRuntime*> m_providers;
};

#endif // CYBOU_STORAGE_TEST_NETWORK_H
