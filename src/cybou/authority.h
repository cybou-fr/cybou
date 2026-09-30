// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_AUTHORITY_H
#define CYBOU_AUTHORITY_H

#include <cybou/account_id.h>
#include <cybou/protocol_operation.h>

#include <cstdint>
#include <map>
#include <filesystem>
#include <array>
#include <mutex>
#include <optional>

namespace cybou {

class CybouNodeRuntime;

/**
 * Identity Authority v1 policy (docs/cybou/57, 67).
 *
 * DERIVED PREVIEW: these values are computed deterministically from finalized
 * history but are not part of consensus and are not enforced. Enforcement
 * requires immutable policy bound to NetworkID and canonical accumulators
 * (or an equivalently consensus-deterministic accounting specification).
 * Persisting this rebuildable preview does not make it enforcement-ready.
 */
struct AuthorityPolicy {
    /** Most Activity credited per Identity per epoch. */
    std::uint64_t activity_cap_per_epoch{16};
    std::uint32_t max_tier{16};

    /** ProtocolBudget: canonical operations per epoch. */
    std::uint64_t protocol_base{64};
    std::uint64_t protocol_per_tier{32};
    std::uint64_t protocol_ceiling{2048};
    /** StorageBudget: active remote replicated bytes. */
    std::uint64_t storage_base{256ULL << 20};
    std::uint64_t storage_per_tier{512ULL << 20};
    std::uint64_t storage_ceiling{64ULL << 30};
    /** BandwidthBudget: PUT/GET bytes per epoch. */
    std::uint64_t bandwidth_base{2ULL << 30};
    std::uint64_t bandwidth_per_tier{2ULL << 30};
    std::uint64_t bandwidth_ceiling{256ULL << 30};
};

struct AuthorityBudgets {
    std::uint64_t protocol_operations_per_epoch{0};
    std::uint64_t storage_bytes{0};
    std::uint64_t bandwidth_bytes_per_epoch{0};
    bool operator==(const AuthorityBudgets&) const = default;
};

/** Earnings and penalties are accumulated separately. */
struct AuthorityRecord {
    AccountId account_id;
    std::uint64_t creation_epoch{0};
    std::uint64_t current_epoch{0};
    std::uint64_t age{0};
    std::uint64_t activity{0};
    std::uint64_t system_contribution{0};
    /** Zero until canonical NodeID binding and uptime evidence exist. */
    std::uint64_t liveness{0};
    /** Zero until canonical verified-storage evidence exists. */
    std::uint64_t storage{0};
    /** Zero until canonical attributable misbehaviour evidence exists. */
    std::uint64_t penalty_debt{0};
    std::uint64_t earned{0};
    std::uint64_t effective{0};
    std::uint32_t tier{0};
    AuthorityBudgets budgets;
    /** Always false for v1: the values are a derived preview. */
    bool enforced{false};
};

/** Saturating integer arithmetic used by every Authority computation. */
constexpr std::uint64_t SaturatingAdd(std::uint64_t a, std::uint64_t b)
{
    return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}

constexpr std::uint64_t SaturatingMul(std::uint64_t a, std::uint64_t b)
{
    return a != 0 && b > UINT64_MAX / a ? UINT64_MAX : a * b;
}

/** tier = min(max_tier, floor(log2(effective + 1))), with integer bit operations. */
std::uint32_t AuthorityTier(std::uint64_t effective, std::uint32_t max_tier);
AuthorityBudgets AuthorityBudgetsForTier(std::uint32_t tier, const AuthorityPolicy& policy);

/** The Identity whose authorization a finalized operation carries, if any. */
std::optional<AccountId> AuthorizingAccount(const ProtocolOperation& operation);

/**
 * Deterministic Authority over canonical finalized history.
 *
 * Scans finalized blocks once, in order, keeping per-Identity activity
 * (capped per epoch) and voluntary System Balance contribution. Age and
 * creation come from canonical account state. Rebuildable at any time.
 */
class AuthorityIndex final {
public:
    explicit AuthorityIndex(CybouNodeRuntime& runtime, AuthorityPolicy policy = {});
    /**
     * With a checkpoint file the scan resumes where it stopped instead of
     * starting at height 1. The file is a derived, rebuildable cache: it is
     * trusted only for the same network and policy and while its block id
     * still matches finalized history; otherwise the index rescans from 0.
     */
    AuthorityIndex(CybouNodeRuntime& runtime, std::filesystem::path checkpoint, AuthorityPolicy policy = {});

    /** Processes up to max_blocks newly finalized blocks; returns the scanned height. */
    std::uint64_t Sync(std::uint64_t max_blocks = 4096);
    /** Authority at the current scanned height; nullopt for unknown Identities. */
    std::optional<AuthorityRecord> Get(const AccountId& account);
    std::uint64_t ScannedHeight() const;
    const AuthorityPolicy& Policy() const { return m_policy; }

private:
    struct Tally {
        std::uint64_t activity{0};
        std::uint64_t epoch{0};
        std::uint64_t epoch_count{0};
        std::uint64_t system_contribution{0};
    };
    bool LoadCheckpoint();
    bool SaveCheckpoint() const;
    std::array<unsigned char, 32> PolicyFingerprint() const;

    CybouNodeRuntime& m_runtime;
    const AuthorityPolicy m_policy;
    const std::filesystem::path m_checkpoint;
    mutable std::mutex m_mutex;
    std::uint64_t m_height{0};
    std::map<AccountId, Tally> m_tallies;
};

} // namespace cybou

#endif // CYBOU_AUTHORITY_H
