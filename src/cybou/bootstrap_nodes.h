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
 * DEV bootstrap authority endpoints.
 *
 * Static seed list for the disposable CYBOU-DEV network only. Beta and
 * Mainnet must derive their bootstrap sets from the operator-approved
 * operator-configured peer discovery flow, not from a compiled-in list (docs 04, 08).
 *
 * CYP2 is the only transport. The list is
 * transport metadata: it is never part of the serialized network definition
 * and carries no trust — the genesis file remains the root of trust for every
 * synced block.
 */
struct BootstrapAuthorityEndpoint {
    std::string_view host;
    uint16_t p2p_port; // CYP2 peer session
};

inline constexpr std::array<BootstrapAuthorityEndpoint, 1> CYBOU_DEV_BOOTSTRAP_AUTHORITIES{{
    {"51.255.46.58", 29461}, // OVH DEV PoA finalizer node (vps-d0669a91)
}};

} // namespace cybou

#endif // CYBOU_BOOTSTRAP_NODES_H
