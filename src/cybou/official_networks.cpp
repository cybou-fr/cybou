// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/official_networks.h>
#include <cybou/official_devnet_constants.h>
#include <cybou/network_genesis.h>
#include <cybou/network_definition.h>

#include <stdexcept>

namespace cybou {

namespace {

VerifiedNetworkBundle InitializeOfficialDevnetBundle()
{
    auto genesis = DeserializeSignedNetworkGenesis(devnet_constants::SIGNED_GENESIS_BYTES);
    if (!genesis) {
        throw std::runtime_error("failed to deserialize compiled DEVNET genesis");
    }
    auto verified = VerifiedNetworkGenesis::Create(*genesis);
    if (!verified) {
        throw std::runtime_error("compiled DEVNET genesis failed cryptographic verification");
    }
    auto state = DeserializeCybouState(devnet_constants::GENESIS_STATE_BYTES);
    if (!state) {
        throw std::runtime_error("failed to deserialize compiled DEVNET genesis state");
    }
    if (ValidateCybouState(*state) != StateValidationError::NONE) {
        throw std::runtime_error("compiled DEVNET genesis state validation failed");
    }
    const auto state_hash = CybouStateHash(*state);
    if (!state_hash || *state_hash != verified->GetGenesisStateRoot()) {
        throw std::runtime_error("compiled DEVNET genesis state root mismatch");
    }
    CybouNetworkDefinition def;
    def.protocol_version = CYBOU_NETWORK_DEFINITION_VERSION;
    def.network_public_key = verified->GetNetworkPublicKey();
    def.genesis_state_root = verified->GetGenesisStateRoot();
    def.poa_finalizer_public_key = verified->GetPoaPublicKey();
    def.genesis_block_id = ComputeGenesisBlockId(def.genesis_state_root, def.poa_finalizer_public_key);
    def.protocol_parameters = verified->GetProtocolParameters();

    const auto digest = verified->GetGenesisDigest();

    return VerifiedNetworkBundle{
        .genesis = std::move(*verified),
        .genesis_state = std::move(*state),
        .network_definition = std::move(def),
        .genesis_digest = digest,
    };
}

} // namespace

const VerifiedNetworkBundle& GetOfficialDevnetBundle()
{
    static const VerifiedNetworkBundle s_bundle = InitializeOfficialDevnetBundle();
    return s_bundle;
}

const VerifiedNetworkBundle& RequireOfficialNetwork(std::string_view name)
{
    if (name == "devnet" || name == "DEVNET") return GetOfficialDevnetBundle();
    if (name == "mainnet" || name == "MAINNET") {
        throw std::runtime_error("MAINNET unavailable: not provisioned (no key, no genesis, no bootstrap)");
    }
    throw std::runtime_error("unknown network; use --network devnet");
}

} // namespace cybou
