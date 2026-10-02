// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_OFFICIAL_NETWORKS_H
#define CYBOU_OFFICIAL_NETWORKS_H

#include <cybou/identity_crypto.h>
#include <cybou/network_genesis.h>
#include <cybou/official_devnet_constants.h>
#include <uint256.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace cybou {

enum class NetworkKind : uint8_t {
    DEVNET = 0,
    MAINNET = 1,
};

struct OfficialBootstrapLocator {
    std::string_view host;
    uint16_t port{0};
    std::array<unsigned char, 32> tls_spki_sha256{};
};

/**
 * Standard known official network profile: the canonical Network Public Key
 * (NetworkID) and bootstrap rendezvous locators with transport SPKI pins.
 * The compiled signed genesis is verified against that key; there is no
 * second trust anchor.
 */
struct OfficialNetworkProfile {
    NetworkKind kind{NetworkKind::DEVNET};
    std::string_view name;
    std::span<const unsigned char> network_public_key_bytes;
    std::span<const OfficialBootstrapLocator> bootstrap_locators;
};

inline constexpr std::array<OfficialBootstrapLocator, 1> OFFICIAL_DEVNET_BOOTSTRAP_LOCATORS{{
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

struct VerifiedNetworkBundle;

inline constexpr OfficialNetworkProfile OFFICIAL_DEVNET_PROFILE{
    .kind = NetworkKind::DEVNET,
    .name = "DEVNET",
    .network_public_key_bytes = devnet_constants::NETWORK_ID_BYTES,
    .bootstrap_locators = OFFICIAL_DEVNET_BOOTSTRAP_LOCATORS,
};

inline constexpr OfficialNetworkProfile OFFICIAL_MAINNET_PROFILE{
    .kind = NetworkKind::MAINNET,
    .name = "MAINNET",
    .network_public_key_bytes = {},
    .bootstrap_locators = {},
};

/** Returns the immutable compiled DEVNET bundle (verified cryptographically). */
const VerifiedNetworkBundle& GetOfficialDevnetBundle();

/**
 * The only official startup source: "devnet" selects the compiled, verified
 * DEVNET constants. MAINNET is not provisioned and fails closed; any other
 * value, including a file path, is rejected.
 */
const VerifiedNetworkBundle& RequireOfficialNetwork(std::string_view name);

inline const OfficialNetworkProfile* FindOfficialNetworkProfile(std::string_view name)
{
    if (name == OFFICIAL_DEVNET_PROFILE.name) return &OFFICIAL_DEVNET_PROFILE;
    if (name == OFFICIAL_MAINNET_PROFILE.name) return &OFFICIAL_MAINNET_PROFILE;
    return nullptr;
}

inline const OfficialNetworkProfile* FindOfficialNetworkProfile(NetworkKind kind)
{
    switch (kind) {
    case NetworkKind::DEVNET: return &OFFICIAL_DEVNET_PROFILE;
    case NetworkKind::MAINNET: return &OFFICIAL_MAINNET_PROFILE;
    }
    return nullptr;
}

inline bool IsOfficialNetworkConfigured(NetworkKind kind)
{
    const auto* profile = FindOfficialNetworkProfile(kind);
    return profile && !profile->network_public_key_bytes.empty();
}

inline const OfficialNetworkProfile* FindOfficialNetworkProfile(std::span<const unsigned char> network_id)
{
    if (network_id.empty()) return nullptr;
    for (const auto* profile : {&OFFICIAL_DEVNET_PROFILE, &OFFICIAL_MAINNET_PROFILE}) {
        if (!profile->network_public_key_bytes.empty() &&
            profile->network_public_key_bytes.size() == network_id.size() &&
            std::equal(network_id.begin(), network_id.end(), profile->network_public_key_bytes.begin())) {
            return profile;
        }
    }
    return nullptr;
}

inline bool IsOfficialNetwork(std::span<const unsigned char> network_id)
{
    return FindOfficialNetworkProfile(network_id) != nullptr;
}

} // namespace cybou

#endif // CYBOU_OFFICIAL_NETWORKS_H
