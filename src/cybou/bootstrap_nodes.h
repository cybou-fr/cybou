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

// No future-service locator is compiled in until its endpoint and SPKI pin
// are approved.
inline constexpr std::array<InitialBootstrapLocator, 0> CYBOU_INITIAL_BOOTSTRAP_LOCATORS{};

} // namespace cybou

#endif // CYBOU_BOOTSTRAP_NODES_H
