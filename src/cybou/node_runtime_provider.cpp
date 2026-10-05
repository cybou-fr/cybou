// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Intrinsic encrypted storage, provider identity and admission.

#include <cybou/node_runtime.h>
#include <cybou/secret_file.h>
#include <cybou/identity_crypto.h>
#include <cybou/crypto/cleanse.h>
#include <openssl/rand.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace cybou {

namespace {
/// \brief Форматирует ChunkId в lower-case hex для diagnostics/event log.
std::string ChunkIdHex(const ChunkId& id)
{
    static constexpr char digits[]="0123456789abcdef";
    std::string out;
    out.reserve(id.size()*2);
    for (auto byte : id) { out+=digits[byte>>4]; out+=digits[byte&15]; }
    return out;
}

/// \brief Загружает или создаёт стабильный storage secret узла.
/// \details 32 random bytes kept beside provider data (0600); in memory for memory-only runtimes.
std::optional<std::array<unsigned char, 32>> LoadOrCreateProviderSecret(const std::filesystem::path& path)
{
    std::array<unsigned char, 32> secret{};
    if (!path.empty() && std::filesystem::exists(path)) {
        auto bytes = ReadSecretFile(path, 32);
        if (!bytes || bytes->size() != secret.size()) return std::nullopt;
        std::copy(bytes->begin(), bytes->end(), secret.begin());
        crypto::CleanseMemory(bytes->data(), bytes->size());
        return secret;
    }
    if (RAND_bytes(secret.data(), static_cast<int>(secret.size())) != 1) return std::nullopt;
    if (path.empty()) return secret;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    if (!CreateSecretFile(path, secret)) {
        crypto::CleanseMemory(secret.data(), secret.size());
        return std::nullopt;
    }
    return secret;
}
} // namespace

CybouNodeRuntime::ProviderCore::~ProviderCore()
{
    if (storage_secret) crypto::CleanseMemory(storage_secret->data(), storage_secret->size());
}

void CybouNodeRuntime::ProviderCore::Initialize(const NodeRuntimeConfig& config,
    const cybou::Hash256& network_binding)
{
    std::filesystem::path storage_path;
    if (!config.memory_only) {
        storage_path = std::filesystem::path{config.data_dir.string() + ".chunks"};
    }
    const uint64_t capacity = *config.storage_capacity_bytes;
    // Zero существует только для memory-only tests «без provider»: собственный staging не ограничен.
    chunk_blob_store = std::make_unique<ChunkBlobStore>(
        config.memory_only ? std::filesystem::path{} : storage_path / "chunks",
        config.memory_only, config.wipe_data,
        capacity == 0 ? std::numeric_limits<uint64_t>::max() : capacity);
    chunk_retention = std::make_unique<ChunkRetentionRegistry>(
        config.memory_only ? std::filesystem::path{} : storage_path / "retention",
        config.memory_only, config.wipe_data);
    finalized_chunk_store = std::make_unique<FinalizedChunkStore>(*chunk_blob_store, storage_path,
        std::span<const unsigned char, 32>{network_binding.begin(), 32},
        ProviderBudgetBytes(capacity), config.wipe_data);
    // The provider key is this node's stable storage identity across restarts.
    storage_secret = LoadOrCreateProviderSecret(config.memory_only ? std::filesystem::path{} :
        storage_path / "storage.key");
    if (!storage_secret) throw std::runtime_error("cannot load or create the storage provider key");
    const auto storage_key = DeriveIdentityPublicKey(*storage_secret, IdentityKeyPurpose::STORAGE);
    if (storage_key) {
        constexpr std::string_view storage_id_domain{"CYBOU/STORAGE-ID"};
        std::vector<unsigned char> storage_id_input(storage_id_domain.begin(), storage_id_domain.end());
        storage_id_input.insert(storage_id_input.end(), storage_key->ed25519.begin(), storage_key->ed25519.end());
        storage_id_input.insert(storage_id_input.end(), storage_key->ml_dsa.begin(), storage_key->ml_dsa.end());
        storage_id = ComputeBlake3Digest(storage_id_input);
    }
    if (!storage_id) throw std::runtime_error("storage provider key is invalid");
}

std::optional<std::array<unsigned char, 32>> CybouNodeRuntime::LocalStorageId() const { return m_provider.storage_id; }

ChunkRetentionRegistry::CollectResult CybouNodeRuntime::CollectChunkGarbage(const std::uint64_t cache_budget_bytes,
    const std::uint64_t now_ms, const std::size_t max_removals)
{
    // A freshly cached or released blob may be in active use by a download.
    constexpr std::uint64_t GRACE_MS{10 * 60 * 1000};
    (void)m_provider.finalized_chunk_store->RetryPendingPurges(max_removals);
    return m_provider.chunk_retention->Collect(*m_provider.chunk_blob_store, cache_budget_bytes, now_ms, GRACE_MS, max_removals,
        [&](const ChunkId& id) {
            return m_provider.finalized_chunk_store->RemoveUnlessAdmitted(id);
        });
}

std::optional<std::vector<unsigned char>> CybouNodeRuntime::SignStorageProof(
    const std::span<const unsigned char> message) const
{
    if (!m_provider.storage_secret) return std::nullopt;
    const auto key = DeriveIdentityPublicKey(*m_provider.storage_secret, IdentityKeyPurpose::STORAGE);
    const auto signature = SignIdentityMessage(*m_provider.storage_secret, IdentityKeyPurpose::STORAGE, message);
    if (!key || !signature) return std::nullopt;
    std::vector<unsigned char> proof;
    proof.reserve(key->ed25519.size() + key->ml_dsa.size() + signature->ed25519.size() + signature->ml_dsa.size());
    proof.insert(proof.end(), key->ed25519.begin(), key->ed25519.end());
    proof.insert(proof.end(), key->ml_dsa.begin(), key->ml_dsa.end());
    proof.insert(proof.end(), signature->ed25519.begin(), signature->ed25519.end());
    proof.insert(proof.end(), signature->ml_dsa.begin(), signature->ml_dsa.end());
    return proof;
}

ChunkAdmissionResult CybouNodeRuntime::PutFinalizedChunk(
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
    const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    // Provider принимает только оплаченное хранение: финализированная публикация с активной арендой (DEC-279).
    auto result = m_provider.finalized_chunk_store->PutChunk(publication_operation_id, chunk_id, stored_bytes, proof,
        [this](const cybou::Hash256& operation_id) -> std::optional<RootPublication> {
            if (!IsStorageLeaseActive(operation_id)) return std::nullopt;
            return FindFinalizedRootPublication(operation_id);
        });
    // Receipt подписывается только после durable admission; без подписи admission не подтверждается.
    if (result) {
        auto receipt = SignStorageProof(StorageReceiptMessage(m_network_binding, publication_operation_id, chunk_id,
            static_cast<std::uint32_t>(stored_bytes.size())));
        if (receipt) result.receipt = std::move(*receipt);
        else result.status = ChunkAdmissionStatus::STORAGE_ERROR;
    }
    if (m_config.event_writer) m_config.event_writer->Write(result ? NodeEvent::chunk_put : NodeEvent::chunk_verify_failed,
        {{"operation_id",publication_operation_id.GetHex()},{"chunk_id",ChunkIdHex(chunk_id)},{"bytes",std::uint64_t{stored_bytes.size()}},
         {"error_code",std::uint64_t{static_cast<unsigned>(result.status)}}});
    return result;
}

std::optional<std::vector<unsigned char>> CybouNodeRuntime::GetFinalizedChunk(const ChunkId& chunk_id) const
{
    auto bytes = m_provider.finalized_chunk_store->GetChunk(chunk_id);
    if (m_config.event_writer) m_config.event_writer->Write(bytes ? NodeEvent::chunk_get : NodeEvent::chunk_verify_failed,
        {{"chunk_id",ChunkIdHex(chunk_id)},{"bytes",std::uint64_t{bytes ? bytes->size() : 0}}});
    return bytes;
}

StorageAuditAnswer CybouNodeRuntime::AnswerStorageAudit(const StorageAuditChallenge& challenge) const
{
    // Отвечаем только по admitted provider-копии: локальный cache не является обязательством.
    const auto bytes = m_provider.finalized_chunk_store->GetChunk(challenge.chunk_id);
    const auto proof = bytes ? CreateStorageAuditProof(challenge, *bytes) : std::nullopt;
    if (!proof) return {};
    return {.held = true, .response_hash = proof->response_hash};
}

std::optional<ChunkAuthorizationProof> CybouNodeRuntime::GetFinalizedChunkAuthorizationProof(
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id) const
{
    return m_provider.finalized_chunk_store->GetChunkAuthorizationProof(publication_operation_id, chunk_id,
        [this](const cybou::Hash256& operation_id) { return FindFinalizedRootPublication(operation_id); });
}

bool CybouNodeRuntime::HasFinalizedChunk(const ChunkId& chunk_id) const
{
    return m_provider.finalized_chunk_store->HasChunk(chunk_id);
}

std::vector<CybouNodeRuntime::StorageHolding> CybouNodeRuntime::StorageHoldings() const
{
    std::vector<StorageHolding> out;
    if (!m_provider.finalized_chunk_store) return out;
    const auto holdings = m_provider.finalized_chunk_store->Holdings();
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    for (const auto& holding : holdings) {
        StorageHolding item{.publication_id = holding.publication_id, .chunks = holding.chunks, .bytes = holding.bytes};
        if (loaded && loaded.state) {
            if (const auto record = loaded.state->publications.find(holding.publication_id);
                record != loaded.state->publications.end()) {
                item.owner = record->second.owner;
                if (const auto* name = loaded.state->names.PrimaryName(record->second.owner)) item.owner_name = *name;
            }
        }
        out.push_back(std::move(item));
    }
    return out;
}

} // namespace cybou
