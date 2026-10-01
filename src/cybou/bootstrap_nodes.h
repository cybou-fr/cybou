// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BOOTSTRAP_NODES_H
#define CYBOU_BOOTSTRAP_NODES_H

#include <array>
#include <cstdint>
#include <string_view>

namespace cybou {

/**
 * Legacy DEV P2P seed endpoints.
 *
 * These endpoints feed the existing DEV peer-sync path, which currently
 * connects to the legacy VPS finalizer. They are not initial locators for the
 * future bootstrap service and grant no bootstrap role.
 *
 * CYP2 is the only transport. The list is
 * transport metadata only. It is never part of consensus state.
 */
struct BootstrapEndpoint {
    std::string_view host;
    uint16_t p2p_port; // CYP2 peer session
};

inline constexpr std::array<BootstrapEndpoint, 1> CYBOU_DEV_BOOTSTRAP_NODES{{
    {"51.255.46.58", 29461}, // OVH DEV PoA finalizer node (vps-d0669a91)
}};

/** Pre-genesis discovery hint. SPKI pin authenticates first contact only. */
struct InitialBootstrapLocator {
    std::string_view host;
    uint16_t port;
    std::array<unsigned char, 32> tls_spki_sha256;
};

// No future-service locator is compiled in until its endpoint and SPKI pin
// are approved. Never reuse the DEV finalizer above as that locator.
inline constexpr std::array<InitialBootstrapLocator, 0> CYBOU_INITIAL_BOOTSTRAP_LOCATORS{};

} // namespace cybou

#endif // CYBOU_BOOTSTRAP_NODES_H
