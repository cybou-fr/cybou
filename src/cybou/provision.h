// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROVISION_H
#define CYBOU_PROVISION_H

/// \file
/// \brief Одноразовое офлайн provisioning DEVNET и генерация compiled public constants.

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

/// \brief Полный набор результатов DEVNET provisioning: приватный материал, genesis и public constants.
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

/// \brief Генерирует весь DEVNET provisioning полностью в памяти.
std::optional<DevnetProvisionResult> GenerateDevnetProvisioning();

/// \brief Выполняет одноразовое provisioning: private secrets и public constants header.
bool ProvisionDevnet(
    const std::filesystem::path& private_dir,
    const std::filesystem::path& constants_header_path);

} // namespace cybou

#endif // CYBOU_PROVISION_H
