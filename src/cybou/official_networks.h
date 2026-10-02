// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_OFFICIAL_NETWORKS_H
#define CYBOU_OFFICIAL_NETWORKS_H

#include <cybou/identity_crypto.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace cybou {

enum class NetworkKind : uint8_t {
    DEVNET = 0,
    TESTNET = 1,
    MAINNET = 2,
};

struct OfficialBootstrapLocator {
    std::string_view host;
    uint16_t port{0};
    std::array<unsigned char, 32> tls_spki_sha256{};
};

/**
 * Standard known official network profile.
 * Contains bootstrap rendezvous locators with transport SPKI pins
 * and the initial official Authority / PoA key K0.
 */
struct OfficialNetworkProfile {
    NetworkKind kind{NetworkKind::DEVNET};
    std::string_view name;
    std::span<const OfficialBootstrapLocator> bootstrap_locators;
    std::optional<IdentityHybridPublicKey> initial_authority_key;
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

inline constexpr OfficialNetworkProfile OFFICIAL_DEVNET_PROFILE{
    .kind = NetworkKind::DEVNET,
    .name = "DEVNET",
    .bootstrap_locators = OFFICIAL_DEVNET_BOOTSTRAP_LOCATORS,
    .initial_authority_key = std::nullopt,
};

inline constexpr OfficialNetworkProfile OFFICIAL_TESTNET_PROFILE{
    .kind = NetworkKind::TESTNET,
    .name = "TESTNET",
    .bootstrap_locators = {},
    .initial_authority_key = std::nullopt,
};

inline constexpr OfficialNetworkProfile OFFICIAL_MAINNET_PROFILE{
    .kind = NetworkKind::MAINNET,
    .name = "MAINNET",
    .bootstrap_locators = {},
    .initial_authority_key = std::nullopt,
};

} // namespace cybou

#endif // CYBOU_OFFICIAL_NETWORKS_H
