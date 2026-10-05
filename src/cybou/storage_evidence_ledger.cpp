// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Bounded off-chain receipts, verification intervals and shadow accounting.

#include <cybou/storage_service_internal.h>
#include <algorithm>
#include <limits>

namespace cybou {

namespace {
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

} // namespace

StorageService::EvidenceLedger::EvidenceLedger(PrivateApplicationStore& db)
    : m_application_db{db}
{
    LoadEvidence();
}

void StorageService::EvidenceLedger::CreditReplica(const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id,
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

void StorageService::EvidenceLedger::ForgetReplica(const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id)
{
    std::lock_guard lock{m_evidence_mutex};
    m_replica_verified_ms.erase(std::pair{chunk_id, storage_id});
}

void StorageService::EvidenceLedger::RecordEvidence(const std::array<unsigned char, 32>& storage_id,
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

void StorageService::EvidenceLedger::SaveEvidenceIndex()
{
    std::vector<unsigned char> index;
    index.reserve(m_evidence.size() * 32);
    for (const auto& [storage_id, _] : m_evidence) index.insert(index.end(), storage_id.begin(), storage_id.end());
    (void)m_application_db.Put(EVIDENCE_INDEX_KEY, index);
}

void StorageService::EvidenceLedger::LoadEvidence()
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

bool StorageService::EvidenceLedger::SaveReceipt(const cybou::Hash256& operation_id, const ChunkId& chunk_id,
    const StorageEndpoint& provider, const std::span<const unsigned char> receipt)
{
    return m_application_db.Put(ReceiptKey(operation_id, chunk_id, provider.storage_id), receipt);
}

void StorageService::EvidenceLedger::EraseReceipt(const cybou::Hash256& operation_id, const ChunkId& chunk_id,
    const StorageEndpoint& provider)
{
    (void)m_application_db.Erase(ReceiptKey(operation_id, chunk_id, provider.storage_id));
}

std::map<std::array<unsigned char, 32>, StorageProviderEvidence> StorageService::EvidenceLedger::ProviderEvidence()
{
    std::lock_guard lock{m_evidence_mutex};
    return m_evidence;
}

std::map<AccountId, std::uint64_t> StorageService::EvidenceLedger::VerifiedChunks(
    const Placement& placement, const std::map<std::array<unsigned char, 32>, AccountId>& payout_by_storage,
    const AccountId& payer, const std::int64_t verified_since_ms)
{
    std::map<AccountId, std::uint64_t> verified_chunks;
    {
        std::lock_guard lock{m_evidence_mutex};
        for (std::size_t leaf = 0; leaf < placement.leaves.size() && leaf < placement.replicas.size(); ++leaf) {
            std::set<AccountId> counted;
            for (const auto& replica : placement.replicas[leaf]) {
                const auto payout = payout_by_storage.find(replica.storage_id);
                if (payout == payout_by_storage.end() || payout->second == payer) continue;
                const auto checked = m_replica_verified_ms.find(std::pair{placement.leaves[leaf], replica.storage_id});
                if (checked == m_replica_verified_ms.end() || checked->second < verified_since_ms) continue;
                if (counted.insert(payout->second).second) ++verified_chunks[payout->second];
            }
        }
    }
    return verified_chunks;
}

} // namespace cybou
