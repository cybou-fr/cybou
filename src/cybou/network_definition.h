// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_NETWORK_DEFINITION_H
#define CYBOU_NETWORK_DEFINITION_H

#include <cybou/protocol_params.h>
#include <cybou/signing.h>
#include <cybou/identity_crypto.h>
#include <cybou/state.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t CYBOU_NETWORK_DEFINITION_VERSION{6};

/**
 * Runtime consensus view of one network, derived from its signed genesis.
 * NetworkID is network_public_key itself; every 32-byte network field is
 * ComputeNetworkBinding(network_public_key).
 */
struct CybouNetworkDefinition {
    uint8_t protocol_version{CYBOU_NETWORK_DEFINITION_VERSION};
    IdentityHybridPublicKey network_public_key{IdentityKeyPurpose::NETWORK_ROOT, {}, {}};
    uint256 genesis_block_id;
    uint256 genesis_state_root;
    IdentityHybridPublicKey poa_finalizer_public_key{IdentityKeyPurpose::POA_FINALIZER, {}, {}};
    CybouProtocolParameters protocol_parameters;

    friend bool operator==(const CybouNetworkDefinition&, const CybouNetworkDefinition&) = default;
};

enum class NetworkDefinitionError : uint8_t {
    NONE,
    UNSUPPORTED_VERSION,
    NULL_GENESIS_BLOCK_ID,
    NULL_GENESIS_STATE_ROOT,
    INVALID_NETWORK_KEY,
    INVALID_POA_FINALIZER_KEY,
    GENESIS_BLOCK_ID_MISMATCH,
    INVALID_ACCOUNT_CREATION_WORK_BITS,
    ZERO_MAX_ACCOUNT_CREATES_PER_BLOCK,
    ZERO_EPOCH_BLOCKS,
    INVALID_ROOT_PUBLICATION_FEES,
    INVALID_NAME_PARAMETERS,
};

bool ValidateProtocolParameters(const CybouProtocolParameters& params);
NetworkDefinitionError ValidateNetworkDefinition(const CybouNetworkDefinition& definition);
/**
 * The 32-byte binding used wherever wire formats, signatures and persistence
 * carry the network: SHA-256("CYBOU/NETWORK-ID/V6" || canonical Network Public Key).
 * It depends only on the key, because exactly one genesis exists per key.
 */
uint256 ComputeNetworkBinding(const IdentityHybridPublicKey& network_public_key);

uint256 ComputeGenesisBlockId(const uint256& state_root, const IdentityHybridPublicKey& poa_finalizer_public_key);

CybouState CreateDevGenesisState();
CybouNetworkDefinition CreateDevNetworkDefinition(
    const CybouState& genesis,
    const IdentityHybridPublicKey& poa_finalizer_public_key,
    const IdentityHybridPublicKey& network_public_key);

} // namespace cybou

#endif // CYBOU_NETWORK_DEFINITION_H
