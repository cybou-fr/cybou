// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STORAGE_SERVICE_H
#define CYBOU_STORAGE_SERVICE_H

#include <cybou/chunk_authorization.h>
#include <cybou/chunk_id.h>
#include <cybou/finalized_chunk_store.h>
#include <cybou/private_application_store.h>
#include <uint256.h>

#include <compare>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cybou {

class CybouNodeRuntime;

/** Remote full replicas required for PROTECTED. The local copy never counts
 * (it is normally one more physical copy). DEV may run more providers than
 * the target for failover, repair and soak. */
inline constexpr std::uint8_t DEVELOPMENT_REMOTE_REPLICA_TARGET{1};
inline constexpr std::uint8_t BETA_REMOTE_REPLICA_TARGET{2};

struct StorageEndpoint {
    std::string address;
    std::uint16_t port{0};
    auto operator<=>(const StorageEndpoint&) const = default;
};

/** Remote encrypted-chunk transport. Implementations never interpret content. */
class StorageTransport {
public:
    virtual ~StorageTransport() = default;
    /** Currently reachable remote storage providers, never this node itself. */
    virtual std::vector<StorageEndpoint> Providers() = 0;
    virtual std::optional<ChunkAdmissionResult> Put(const StorageEndpoint& provider,
        const uint256& publication_operation_id, const ChunkId& chunk_id,
        std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof) = 0;
    virtual std::optional<std::vector<unsigned char>> Get(const StorageEndpoint& provider,
        const ChunkId& chunk_id) = 0;
    virtual std::optional<ChunkAuthorizationProof> GetProof(const StorageEndpoint& provider,
        const uint256& publication_operation_id, const ChunkId& chunk_id) = 0;
};

/** CYP2 PUT_AUTHORIZED_CHUNK / GET_CHUNK_BY_ID over the runtime's connected peers. */
class RuntimeStorageTransport final : public StorageTransport {
public:
    explicit RuntimeStorageTransport(CybouNodeRuntime& runtime) : m_runtime{runtime} {}
    std::vector<StorageEndpoint> Providers() override;
    std::optional<ChunkAdmissionResult> Put(const StorageEndpoint& provider,
        const uint256& publication_operation_id, const ChunkId& chunk_id,
        std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof) override;
    std::optional<std::vector<unsigned char>> Get(const StorageEndpoint& provider,
        const ChunkId& chunk_id) override;
    std::optional<ChunkAuthorizationProof> GetProof(const StorageEndpoint& provider,
        const uint256& publication_operation_id, const ChunkId& chunk_id) override;

private:
    CybouNodeRuntime& m_runtime;
};

enum class DurabilityState : std::uint8_t {
    /** Finalized; fewer than the target healthy remote replicas for some chunk. */
    SECURING,
    /** Every chunk has at least the target healthy remote replicas. */
    PROTECTED,
    /** Placement cannot proceed: the publication or its chunk list is invalid. */
    NEEDS_ATTENTION,
};

struct PublicationDurability {
    DurabilityState state{DurabilityState::SECURING};
    std::uint32_t chunk_count{0};
    std::uint32_t chunks_at_target{0};
    /** Lowest healthy remote replica count over all chunks. */
    std::uint32_t min_replicas{0};
    std::string error;
    /** 0..100 over chunk placements toward the target. */
    int ProgressPercent(std::uint8_t target) const;
};

/**
 * Content transport and remote durability for one unlocked Identity.
 *
 * Only finalized RootPublications are placed: the ordered chunk list must
 * reproduce the publication's chunk-authorization root. Placement records are
 * an operational cache inside the Identity's encrypted Application DB; losing
 * them loses no content, since retrieval queries providers by ChunkID.
 * Provider choice is uniform CSPRNG selection among eligible distinct peers.
 */
class StorageService final {
public:
    StorageService(CybouNodeRuntime& runtime, StorageTransport& transport,
        PrivateApplicationStore& application_db,
        std::uint8_t remote_replica_target = DEVELOPMENT_REMOTE_REPLICA_TARGET);

    /** Places every chunk of a finalized publication toward the remote target.
     * Idempotent: call again to retry. leaves are the ChunkIDs in authorization order. */
    PublicationDurability Secure(const uint256& publication_operation_id, std::span<const ChunkId> leaves);
    /** Rebuilds the exact leaf order from provider-held proofs for candidate chunks. */
    PublicationDurability Rebuild(const uint256& publication_operation_id,
        std::span<const ChunkId> candidate_chunks);
    /** Checks whether a candidate chunk is a leaf of this finalized publication. */
    std::optional<ChunkAuthorizationProof> GetAuthorizationProof(
        const uint256& publication_operation_id, const ChunkId& chunk_id);
    /** Resumes placement for a publication already known to this service. */
    PublicationDurability Resume(const uint256& publication_operation_id);
    /** Re-reads every recorded replica, drops missing/corrupt ones and repairs to target. */
    PublicationDurability Audit(const uint256& publication_operation_id);
    /**
     * Bounded periodic health check: re-reads the replicas of at most
     * max_chunks chunks (continuing where the previous call stopped), drops
     * missing or BLAKE3-mismatching ones and reports the resulting state. It
     * does not repair; a SECURING result is repaired by Secure/Resume.
     */
    PublicationDurability AuditSome(const uint256& publication_operation_id, std::size_t max_chunks);
    std::optional<PublicationDurability> GetDurability(const uint256& publication_operation_id);

    /** Local encrypted blob, else the first BLAKE3-valid provider copy (cached locally). */
    std::optional<std::vector<unsigned char>> Fetch(const ChunkId& chunk_id);

    std::uint8_t RemoteReplicaTarget() const { return m_target; }

private:
    struct Placement;
    std::optional<Placement> Load(const uint256& operation_id) const;
    bool Save(const Placement& placement);
    PublicationDurability Place(Placement& placement);
    PublicationDurability Summarize(const Placement& placement) const;
    std::optional<std::vector<unsigned char>> FetchLocked(const ChunkId& chunk_id,
        std::span<const StorageEndpoint> preferred);

    CybouNodeRuntime& m_runtime;
    StorageTransport& m_transport;
    PrivateApplicationStore& m_application_db;
    const std::uint8_t m_target;
    std::mutex m_mutex;
    /** Next chunk to audit per publication; restarting from 0 is harmless. */
    std::map<uint256, std::size_t> m_audit_cursor;
};

} // namespace cybou

#endif // CYBOU_STORAGE_SERVICE_H
