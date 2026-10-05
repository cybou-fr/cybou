// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_STORAGE_SERVICE_INTERNAL_H
#define CYBOU_STORAGE_SERVICE_INTERNAL_H

#include <cybou/storage_service.h>
#include <chrono>
#include <algorithm>

namespace cybou {

inline constexpr std::size_t MAX_REPLICAS_PER_CHUNK{16};
/// Дедупликация идёт по StorageId, потому что один Full Node может отвечать с нескольких endpoint.
inline bool HasProvider(std::span<const StorageEndpoint> replicas, const StorageEndpoint& provider)
{
    return std::any_of(replicas.begin(), replicas.end(),
        [&](const StorageEndpoint& r) { return SameProvider(r, provider); });
}

inline std::int64_t StorageEvidenceNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

struct StorageService::Placement {
    cybou::Hash256 operation_id;
    /** Точный leaf-order finalized публикации; именно он нужен для Merkle proof каждого чанка. */
    std::vector<ChunkId> leaves;
    /** Удалённые providers, подтвердившие leaf; локальная копия здесь никогда не учитывается. */
    std::vector<std::vector<StorageEndpoint>> replicas;
};


// Encrypted local placement metadata. Service serializes plans; Save batches record/index.
class StorageService::PlacementRepository {
public:
    explicit PlacementRepository(PrivateApplicationStore& db) : m_application_db{db} {}
    std::optional<Placement> Load(const cybou::Hash256& id, bool rebuilding = false) const;
    bool Save(const Placement& placement, bool rebuilding = false);
    std::vector<cybou::Hash256> PlacementIndex() const;
    bool IsUnlocked() const { return m_application_db.IsUnlocked(); }
    void EraseRebuild(const cybou::Hash256& id) { (void)m_application_db.Erase("storage/rebuild/" + id.GetHex()); }
private:
    PrivateApplicationStore& m_application_db;
};

// Bounded off-chain evidence/receipts. Restart closes credited intervals conservatively.
class StorageService::EvidenceLedger {
public:
    explicit EvidenceLedger(PrivateApplicationStore& db);
    void RecordEvidence(const std::array<unsigned char, 32>& storage_id,
        const std::function<void(StorageProviderEvidence&)>& update);
    void CreditReplica(const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id,
        std::uint64_t stored_bytes, std::int64_t now_ms);
    void ForgetReplica(const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id);
    std::map<std::array<unsigned char, 32>, StorageProviderEvidence> ProviderEvidence();
    bool SaveReceipt(const cybou::Hash256& id, const ChunkId& chunk_id,
        const StorageEndpoint& provider, std::span<const unsigned char> receipt);
    void EraseReceipt(const cybou::Hash256& id, const ChunkId& chunk_id, const StorageEndpoint& provider);
    std::map<AccountId, std::uint64_t> VerifiedChunks(const Placement& placement,
        const std::map<std::array<unsigned char, 32>, AccountId>& payout_by_storage,
        const AccountId& payer, std::int64_t verified_since_ms);
private:
    void LoadEvidence();
    void SaveEvidenceIndex(); // Requires m_evidence_mutex.
    PrivateApplicationStore& m_application_db;
    std::mutex m_evidence_mutex;
    std::map<std::array<unsigned char, 32>, StorageProviderEvidence> m_evidence;
    std::map<std::pair<ChunkId, std::array<unsigned char, 32>>, std::int64_t> m_replica_verified_ms;
};

// Called outside placement mutex. Transport interface and verification rules are unchanged.
class StorageService::ReplicaVerifier {
public:
    ReplicaVerifier(StorageTransport& transport, EvidenceLedger& evidence)
        : m_transport{transport}, m_evidence{evidence} {}
    bool CheckReplica(const StorageEndpoint& provider, const ChunkId& chunk_id,
        const std::optional<std::vector<unsigned char>>& local_bytes, bool force_full);
private:
    StorageTransport& m_transport;
    EvidenceLedger& m_evidence;
};

class StorageService::ProviderSelector {
public:
    static std::array<unsigned char, 32> EconomicIdentity(const StorageEndpoint& provider);
    static bool OrderProviders(std::vector<StorageEndpoint>& providers);
    static bool Shuffle(std::vector<StorageEndpoint>& providers);
};

} // namespace cybou

#endif // CYBOU_STORAGE_SERVICE_INTERNAL_H
