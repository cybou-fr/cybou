// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_PARAMS_H
#define CYBOU_PROTOCOL_PARAMS_H

#include <cstdint>

namespace cybou {

inline constexpr uint64_t DEV_ONBOARDING_BONUS{6000};
inline constexpr uint32_t DEFAULT_MAX_ACCOUNT_CREATES_PER_BLOCK{100};
inline constexpr unsigned int DEFAULT_ACCOUNT_CREATION_WORK_BITS{16};
inline constexpr uint64_t DEFAULT_ACCOUNT_CREATION_EPOCH_LAG{1};

/**
 * Deterministic protocol parameters governing consensus validation and state transitions.
 * Eliminates caller-supplied parameter divergence across validator nodes.
 */
struct CybouProtocolParameters {
    unsigned int account_creation_work_bits{DEFAULT_ACCOUNT_CREATION_WORK_BITS};
    uint64_t account_creation_epoch_lag{DEFAULT_ACCOUNT_CREATION_EPOCH_LAG};
    uint32_t max_account_creates_per_block{DEFAULT_MAX_ACCOUNT_CREATES_PER_BLOCK};
    uint64_t onboarding_bonus{DEV_ONBOARDING_BONUS};

    friend bool operator==(const CybouProtocolParameters&, const CybouProtocolParameters&) = default;
};

constexpr CybouProtocolParameters DevProtocolParameters()
{
    return CybouProtocolParameters{};
}

} // namespace cybou

#endif // CYBOU_PROTOCOL_PARAMS_H
