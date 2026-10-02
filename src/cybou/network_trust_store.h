// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_NETWORK_TRUST_STORE_H
#define CYBOU_NETWORK_TRUST_STORE_H

#include <cybou/network_genesis.h>
#include <uint256.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cybou {

enum class NetworkTrustDecision : uint8_t {
    ACCEPT_NEW,           // generation > highest_generation: valid upgrade, requires clean re-genesis
    ACCEPT_CURRENT,       // generation == highest_generation && digest == accepted_digest: current accepted genesis
    REJECT_ROLLBACK,      // generation < highest_generation: rejected anti-rollback violation
    REJECT_CONFLICT,      // generation == highest_generation && digest != accepted_digest: equivocation conflict
    REJECT_INVALID_KEY,   // genesis network_public_key does not match expected NetworkID
    REJECT_INVALID_SIG,   // genesis signature verification failed
};

/**
 * Record persisted in the isolated global network-trust directory.
 * Stores the monotonic anti-rollback state for one official network.
 */
struct NetworkTrustRecord {
    std::vector<unsigned char> network_id;
    uint64_t highest_generation{0};
    uint256 accepted_genesis_digest;

    friend bool operator==(const NetworkTrustRecord&, const NetworkTrustRecord&) = default;
};

/**
 * Persistent trust store outside network-bound data directories.
 * Guarantees monotonic genesis_generation progression and equivocation detection.
 */
class NetworkTrustStore {
public:
    explicit NetworkTrustStore(std::filesystem::path trust_dir);

    /** Load current trust record for a given canonical NetworkID. */
    std::optional<NetworkTrustRecord> LoadRecord(std::span<const unsigned char> network_id) const;

    /**
     * Evaluates a candidate SignedGenesis against the stored anti-rollback record.
     * Checks signature, network key match, generation monotonicity, and digest identity.
     */
    NetworkTrustDecision Evaluate(
        std::span<const unsigned char> expected_network_id,
        const NetworkGenesis& candidate) const;

    /**
     * Records and persists an accepted SignedGenesis generation and digest.
     * Fails closed if the transition violates anti-rollback or conflict rules.
     */
    bool RecordAccepted(
        std::span<const unsigned char> expected_network_id,
        const VerifiedNetworkGenesis& verified_genesis);

private:
    std::filesystem::path GetRecordPath(std::span<const unsigned char> network_id) const;

    std::filesystem::path m_trust_dir;
};

} // namespace cybou

#endif // CYBOU_NETWORK_TRUST_STORE_H
