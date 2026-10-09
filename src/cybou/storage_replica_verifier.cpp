// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Random-offset audits and full GET verification over the existing transport.

#include <cybou/storage_service_internal.h>
#include <openssl/rand.h>

namespace cybou {

bool StorageService::ReplicaVerifier::CheckReplica(const StorageEndpoint& provider, const ChunkId& chunk_id,
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
            const auto now = StorageEvidenceNowMs();
            const bool saved = m_evidence.RecordEvidence(provider.storage_id, [&](StorageProviderEvidence& e) {
                if (ok) { ++e.successes; e.last_success_ms = now; }
                else { ++e.failures; e.last_failure_ms = now; }
            });
            if (ok && saved && m_evidence.CreditReplica(provider.storage_id, chunk_id, local_bytes->size(), now)) return true;
            m_evidence.ForgetReplica(provider.storage_id, chunk_id);
            return false;
        }
        // Transport без audit или без ответа: проверяем exact bytes полным GET.
    }
    const auto bytes = m_transport.Get(provider, chunk_id);
    const bool ok = bytes && ComputeChunkId(*bytes) == chunk_id;
    const auto now = StorageEvidenceNowMs();
    const bool saved = m_evidence.RecordEvidence(provider.storage_id, [&](StorageProviderEvidence& e) {
        if (ok) { ++e.successes; ++e.full_verifications; e.last_success_ms = now; e.last_full_verification_ms = now; }
        else { ++e.failures; e.last_failure_ms = now; }
    });
    if (ok && saved && m_evidence.CreditReplica(provider.storage_id, chunk_id, bytes->size(), now)) return true;
    m_evidence.ForgetReplica(provider.storage_id, chunk_id);
    return false;
}

} // namespace cybou
