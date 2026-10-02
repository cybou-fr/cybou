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
 * Local indexing policy for the informational Authority metric. It has no
 * protocol, resource-allocation, or PoA effect.
 */
struct AuthorityPolicy {
    /** Most Activity credited per Identity per epoch. */
    std::uint64_t activity_cap_per_epoch{16};
};

/** Read-only values derived from finalized history and current finalized state. */
struct AuthorityRecord {
    AccountId account_id;
    std::uint64_t creation_epoch{0};
    std::uint64_t current_epoch{0};
    std::uint64_t age{0};
    std::uint64_t activity{0};
    std::uint64_t system_contribution{0};
    std::uint64_t value{0};
};

/** Saturating integer arithmetic used by every Authority computation. */
constexpr std::uint64_t SaturatingAdd(std::uint64_t a, std::uint64_t b)
{
    return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}

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
