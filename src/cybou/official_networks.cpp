// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/official_networks.h>
#include <cybou/official_devnet_constants.h>
#if defined(CYBOU_ENABLE_LAB_NETWORK)
#include <cybou/crypto/sha256.h>
#endif

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
        throw std::runtime_error("compiled DEVNET genesis state is invalid for state v12; provision a new NetworkID before DEVNET startup");
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

#if defined(CYBOU_ENABLE_LAB_NETWORK)
std::array<unsigned char, 32> LabSeed(std::string_view domain)
{
    std::array<unsigned char, 32> seed{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain)}, seed.data())) {
        throw std::runtime_error("cannot derive LAB seed");
    }
    return seed;
}

OfficialNetwork BuildLabNetwork()
{
    const auto network_seed = LabSeed("CYBOU/LAB/NETWORK-ROOT/V1");
    const auto network_key = DeriveIdentityPublicKey(network_seed, IdentityKeyPurpose::NETWORK_ROOT);
    const auto poa_key = DeriveIdentityPublicKey(LabPoaFinalizerSeed(), IdentityKeyPurpose::POA_FINALIZER);
    auto state = CreateDevGenesisState();
    const auto recovery_key = DeriveIdentityPublicKey(LabSeed("CYBOU/LAB/CENTRAL-AUTHORITY/V1"), IdentityKeyPurpose::RECOVERY_ROOT);
    const auto recovery_id = recovery_key ? ComputeRecoveryKeyId(*recovery_key) : std::nullopt;
    if (!recovery_id) throw std::runtime_error("cannot derive LAB Central Authority");
    state.genesis_allocations.emplace(*recovery_id, GenesisAllocation{
        .balance = 100'000'000, .authority = 1'000'001, .label = std::string{CENTRAL_AUTHORITY_NAME}});
    const auto state_root = CybouStateHash(state);
    if (!network_key || !poa_key || !state_root) throw std::runtime_error("cannot build LAB network");
    NetworkGenesis spec;
    spec.network_public_key = *network_key;
    spec.genesis_state_root = *state_root;
    spec.poa_finalizer_public_key = *poa_key;
    spec.protocol_parameters = DevProtocolParameters();
    const auto digest = ComputeNetworkGenesisDigest(spec);
    const auto signature = SignIdentityMessage(network_seed, IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{digest.begin(), digest.size()});
    if (!signature) throw std::runtime_error("cannot sign LAB genesis");
    spec.signature = *signature;
    auto verified = VerifiedNetworkGenesis::Create(spec);
    if (!verified) throw std::runtime_error("LAB genesis failed verification");
    return OfficialNetwork{
        .kind = NetworkKind::LAB,
        .name = "LAB",
        .genesis = std::move(*verified),
        .genesis_state = std::move(state),
        .rendezvous_locators = {},
    };
}
#endif

} // namespace

#if defined(CYBOU_ENABLE_LAB_NETWORK)
std::array<unsigned char, 32> LabPoaFinalizerSeed()
{
    return LabSeed("CYBOU/LAB/POA-FINALIZER/V1");
}
#endif

const OfficialNetwork& RequireOfficialNetwork(const NetworkKind kind)
{
    switch (kind) {
    case NetworkKind::DEVNET: {
        static const OfficialNetwork s_devnet = VerifyCompiledDevnet();
        return s_devnet;
    }
    case NetworkKind::MAINNET:
        break;
#if defined(CYBOU_ENABLE_LAB_NETWORK)
    case NetworkKind::LAB: {
        static const OfficialNetwork s_lab = BuildLabNetwork();
        return s_lab;
    }
#endif
    }
    throw std::runtime_error("MAINNET unavailable: not provisioned (no key, no genesis, no bootstrap)");
}

const OfficialNetwork& RequireOfficialNetwork(const std::string_view name)
{
    if (name == "devnet" || name == "DEVNET") return RequireOfficialNetwork(NetworkKind::DEVNET);
    if (name == "mainnet" || name == "MAINNET") return RequireOfficialNetwork(NetworkKind::MAINNET);
#if defined(CYBOU_ENABLE_LAB_NETWORK)
    if (name == "lab" || name == "LAB") return RequireOfficialNetwork(NetworkKind::LAB);
#endif
    throw std::runtime_error("unknown network; use --network devnet");
}

} // namespace cybou
