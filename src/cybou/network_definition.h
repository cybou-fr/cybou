// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_NETWORK_DEFINITION_H
#define CYBOU_NETWORK_DEFINITION_H

#include <cybou/protocol_params.h>
#include <cybou/signing.h>
#include <cybou/state.h>
#include <uint256.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t CYBOU_NETWORK_DEFINITION_VERSION{2};

/** Immutable consensus identity for one DEV, Beta, or Mainnet network. */
struct CybouNetworkDefinition {
    uint8_t protocol_version{CYBOU_NETWORK_DEFINITION_VERSION};
    uint256 genesis_block_id;
    uint256 genesis_state_root;
    CybouProtocolParameters protocol_parameters;
    uint256 initial_validator_set_commitment;
    std::optional<OperatorAuthorityKeySet> operator_authority;

    friend bool operator==(const CybouNetworkDefinition&, const CybouNetworkDefinition&) = default;
};

enum class NetworkDefinitionError : uint8_t {
    NONE,
    UNSUPPORTED_VERSION,
    NULL_GENESIS_BLOCK_ID,
    NULL_GENESIS_STATE_ROOT,
    NULL_VALIDATOR_SET_COMMITMENT,
    INVALID_ACCOUNT_CREATION_WORK_BITS,
    ZERO_MAX_ACCOUNT_CREATES_PER_BLOCK,
    ZERO_EPOCH_BLOCKS,
    INVALID_NAME_PARAMETERS,
    NULL_OPERATOR_AUTHORITY_KEYSET_ID,
    NULL_OPERATOR_AUTHORITY_KEY,
    INVALID_OPERATOR_AUTHORITY_EPOCH,
};

NetworkDefinitionError ValidateNetworkDefinition(const CybouNetworkDefinition& definition);
std::vector<unsigned char> SerializeNetworkDefinition(const CybouNetworkDefinition& definition);
std::optional<CybouNetworkDefinition> DeserializeNetworkDefinition(std::span<const unsigned char> bytes);
uint256 NetworkId(const CybouNetworkDefinition& definition);

struct CybouNetworkFile {
    CybouNetworkDefinition definition;
    CybouState genesis;
};

std::optional<CybouNetworkFile> LoadCybouNetworkFile(const std::filesystem::path& path);

uint256 ComputeGenesisBlockId(const uint256& state_root, const uint256& validator_set_commitment);

CybouState CreateDevGenesisState(const IdentityHybridPublicKey& validator_public_key);
CybouNetworkDefinition CreateDevNetworkDefinition(const CybouState& genesis);

// Transition aliases
using CybouNetworkDefinitionV1 = CybouNetworkDefinition;

} // namespace cybou

#endif // CYBOU_NETWORK_DEFINITION_H
