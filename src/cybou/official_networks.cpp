// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/official_networks.h>
#include <cybou/official_devnet_constants.h>

#include <stdexcept>

namespace cybou {

namespace {

inline constexpr std::array<RendezvousLocator, 1> DEVNET_BOOTSTRAP_LOCATORS{{
    {
        .host = "51.255.46.58",
        .port = 29461,
        .tls_spki_sha256 = {
            0xd8, 0x30, 0x37, 0x41, 0xaa, 0x79, 0xfd, 0x3d,
            0xe0, 0xf1, 0x77, 0xc3, 0x29, 0x11, 0x9d, 0xa7,
            0x8c, 0x6d, 0x41, 0x9c, 0x16, 0x8e, 0x7f, 0xbb,
            0x2e, 0xc1, 0x1d, 0x6a, 0xba, 0x07, 0xfb, 0xdb,
        },
    },
}};

OfficialNetwork VerifyCompiledDevnet()
{
    auto genesis = DeserializeSignedNetworkGenesis(devnet_constants::SIGNED_GENESIS_BYTES);
    if (!genesis) throw std::runtime_error("failed to deserialize compiled DEVNET genesis");
    const auto key = CanonicalSerializeNetworkPublicKey(genesis->network_public_key);
    if (!std::equal(key.begin(), key.end(), devnet_constants::NETWORK_ID_BYTES.begin(),
            devnet_constants::NETWORK_ID_BYTES.end())) {
        throw std::runtime_error("compiled DEVNET genesis is not bound to the compiled Network Public Key");
    }
    auto verified = VerifiedNetworkGenesis::Create(*genesis);
    if (!verified) throw std::runtime_error("compiled DEVNET genesis failed cryptographic verification");
    auto state = DeserializeCybouState(devnet_constants::GENESIS_STATE_BYTES);
    if (!state || ValidateCybouState(*state) != StateValidationError::NONE) {
        throw std::runtime_error("compiled DEVNET genesis state is invalid for state; provision a new NetworkID before DEVNET startup");
    }
    const auto state_hash = CybouStateHash(*state);
    if (!state_hash || *state_hash != verified->GetGenesisStateRoot()) {
        throw std::runtime_error("compiled DEVNET genesis state root mismatch");
    }
    return OfficialNetwork{
        .kind = NetworkKind::DEVNET,
        .name = "DEVNET",
        .genesis = std::move(*verified),
        .genesis_state = std::move(*state),
        .rendezvous_locators = DEVNET_BOOTSTRAP_LOCATORS,
    };
}


} // namespace


const OfficialNetwork& RequireOfficialNetwork(const NetworkKind kind)
{
    switch (kind) {
    case NetworkKind::DEVNET: {
        static const OfficialNetwork s_devnet = VerifyCompiledDevnet();
        return s_devnet;
    }
    case NetworkKind::MAINNET:
        break;
    }
    throw std::runtime_error("MAINNET unavailable: not provisioned (no key, no genesis, no bootstrap)");
}

const OfficialNetwork& RequireOfficialNetwork(const std::string_view name)
{
    if (name == "devnet" || name == "DEVNET") return RequireOfficialNetwork(NetworkKind::DEVNET);
    if (name == "mainnet" || name == "MAINNET") return RequireOfficialNetwork(NetworkKind::MAINNET);
    throw std::runtime_error("unknown network; use --network devnet");
}

} // namespace cybou
