// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_AUTHORITY_POLICY_H
#define CYBOU_AUTHORITY_POLICY_H
#include <cstdint>
namespace cybou {
struct AuthorityPolicy {
    uint64_t activity_cap_per_epoch{16};
    uint32_t max_tier{16};
    uint64_t protocol_base{2048}, protocol_per_tier{128}, protocol_ceiling{65536};
    uint64_t storage_base{256ULL << 20}, storage_per_tier{512ULL << 20}, storage_ceiling{64ULL << 30};
    uint64_t bandwidth_base{2ULL << 30}, bandwidth_per_tier{2ULL << 30}, bandwidth_ceiling{256ULL << 30};
    friend bool operator==(const AuthorityPolicy&, const AuthorityPolicy&) = default;
};
constexpr bool ValidAuthorityPolicy(const AuthorityPolicy& p) {
    return p.max_tier <= 63 && p.activity_cap_per_epoch <= 2048 &&
        p.protocol_base > 0 && p.protocol_base <= p.protocol_ceiling && p.protocol_ceiling <= 65536 &&
        p.storage_base > 0 && p.storage_base <= p.storage_ceiling && p.storage_ceiling <= (64ULL << 30) &&
        p.bandwidth_base > 0 && p.bandwidth_base <= p.bandwidth_ceiling && p.bandwidth_ceiling <= (256ULL << 30);
}
struct AuthorityAccumulator {
    uint64_t activity{0}, system_contribution{0}, penalty_debt{0};
    uint64_t epoch{0}, activity_this_epoch{0}, protocol_used{0};
    uint64_t liveness{0}, storage{0}, storage_remainder{0};
    uint64_t liveness_epoch{0}, liveness_observations{0}, last_liveness_height{0};
    uint64_t validation{0}, validation_this_epoch{0};
    friend bool operator==(const AuthorityAccumulator&, const AuthorityAccumulator&) = default;
};
} // namespace cybou
#endif
