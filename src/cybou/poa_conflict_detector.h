// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_POA_CONFLICT_DETECTOR_H
#define CYBOU_POA_CONFLICT_DETECTOR_H

#include <cybou/kv_store.h>
#include <cybou/poa_finality.h>

#include <mutex>
#include <string>

namespace cybou {

enum class PoaConflictStatus : uint8_t {
    OBSERVED,
    ALREADY_OBSERVED,
    SAFETY_CONFLICT,
    INVALID_CERTIFICATE,
    ALREADY_HALTED,
    CORRUPT_STORAGE,
    STORAGE_ERROR,
};

/** Persists finality observations and halts on valid same-parent equivocation. */
class PoaConflictDetector final {
public:
    PoaConflictDetector(KVStore& db, const uint256& network_id,
        const IdentityHybridPublicKey& genesis_finalizer_key);

    PoaConflictStatus Observe(const PoaFinalityCertificate& certificate, const CybouBlock& block);
    bool SafetyHalted() const;

private:
    bool PersistHalt(const std::vector<unsigned char>& record) noexcept;

    KVStore& m_db;
    const uint256 m_network_id;
    const IdentityHybridPublicKey m_genesis_finalizer_key;
    const std::string m_prefix;
    mutable std::mutex m_mutex;
    bool m_halted{false};
};

} // namespace cybou

#endif // CYBOU_POA_CONFLICT_DETECTOR_H
