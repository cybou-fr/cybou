// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_OFFICIAL_NETWORKS_H
#define CYBOU_OFFICIAL_NETWORKS_H

#include <cybou/network_definition.h>
#include <cybou/network_genesis.h>
#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace cybou {

enum class NetworkKind : uint8_t {
    DEVNET = 0,
    MAINNET = 1,
};

/** Known rendezvous peer: an ordinary full node, pinned for transport discovery only. */
struct OfficialBootstrapLocator {
    std::string_view host;
    uint16_t port{0};
    std::array<unsigned char, 32> tls_spki_sha256{};
};

/**
 * One official network, built only from compiled public constants and verified
 * once: Network Public Key (NetworkID) -> signed immutable NetworkGenesis ->
 * initial state root. `network_definition` is the runtime consensus view
 * derived from the verified genesis.
 */
struct OfficialNetwork {
    NetworkKind kind{NetworkKind::DEVNET};
    std::string_view name;
    VerifiedNetworkGenesis genesis;
    CybouState genesis_state;
    CybouNetworkDefinition network_definition;
    std::span<const OfficialBootstrapLocator> bootstrap_locators;
};

/**
 * The only official startup source. "devnet" returns the compiled, verified
 * DEVNET. MAINNET is not provisioned (no key, genesis or bootstrap) and fails
 * closed; any other value, including a file path, is rejected.
 */
const OfficialNetwork& RequireOfficialNetwork(std::string_view name);
const OfficialNetwork& RequireOfficialNetwork(NetworkKind kind);

} // namespace cybou

#endif // CYBOU_OFFICIAL_NETWORKS_H
