// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BOOTSTRAP_NODES_H
#define CYBOU_BOOTSTRAP_NODES_H

#include <array>
#include <cstdint>
#include <string_view>

namespace cybou {

/** Optional DEV CYP2 peers. The VPS bootstrap uses a separate protocol and
 * must never be treated as a CYP2 peer or PoA finalizer. */
struct BootstrapEndpoint {
    std::string_view host;
    uint16_t p2p_port; // CYP2 peer session
};

inline constexpr std::array<BootstrapEndpoint, 0> CYBOU_DEV_BOOTSTRAP_NODES{};

/** Pre-genesis discovery hint. SPKI pin authenticates first contact only. */
struct InitialBootstrapLocator {
    std::string_view host;
    uint16_t port;
    std::array<unsigned char, 32> tls_spki_sha256;
};

/** Official DEV pre-genesis locator, pinned to the SPKI served by
 * cybou-bootstrap.service on the DEV VPS. This does not identify a consensus
 * role or make the endpoint a CYP2 full-node session. */
inline constexpr std::array<InitialBootstrapLocator, 1> CYBOU_INITIAL_BOOTSTRAP_LOCATORS{{
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

} // namespace cybou

#endif // CYBOU_BOOTSTRAP_NODES_H
