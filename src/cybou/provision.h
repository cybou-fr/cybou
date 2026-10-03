// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROVISION_H
#define CYBOU_PROVISION_H

#include <cybou/identity_crypto.h>
#include <cybou/identity_kem.h>
#include <cybou/network_genesis.h>
#include <cybou/recovery_phrase.h>
#include <cybou/state.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cybou {

struct DevnetProvisionResult {
    // Network Root (strictly offline)
    RecoveryEntropy network_entropy{};
    RecoveryWords network_words{};
    IdentityHybridPublicKey network_public_key;
    std::vector<unsigned char> network_id_bytes;

    // cybou.cybou Identity
    RecoveryEntropy cybou_entropy{};
    RecoveryWords cybou_words{};
    AccountId cybou_account_id{};
    IdentityHybridPublicKey cybou_recovery_key;
    IdentityKeyId cybou_recovery_key_id{};
    IdentityHybridPublicKey cybou_auth_key;
    XWingPublicKey cybou_kem_pub{};
    IdentityHybridPublicKey cybou_poa_pub;
    IdentityKeyId cybou_poa_key_id{};

    // Consensus Genesis & Initial State
    CybouState genesis_state;
    NetworkGenesis signed_genesis;
    cybou::Hash256 genesis_state_root;
    std::vector<unsigned char> serialized_signed_genesis;
    std::vector<unsigned char> serialized_genesis_state;
};

/** Generates complete DEVNET provisioning material in memory. */
std::optional<DevnetProvisionResult> GenerateDevnetProvisioning();

/**
 * Executes full one-time provisioning:
 * 1. Generates secret keys and genesis material in memory.
 * 2. Writes private secret files to private_dir (e.g. private/devnet).
 * 3. Writes public C++ constants header to constants_header_path.
 */
bool ProvisionDevnet(
    const std::filesystem::path& private_dir,
    const std::filesystem::path& constants_header_path,
    bool overwrite = false);

} // namespace cybou

#endif // CYBOU_PROVISION_H
