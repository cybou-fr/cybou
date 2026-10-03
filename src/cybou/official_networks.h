// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_OFFICIAL_NETWORKS_H
#define CYBOU_OFFICIAL_NETWORKS_H

/// \file
/// \brief Verified official networks, жёстко собранные из public constants.

#include <cybou/network_genesis.h>
#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace cybou {

/// \brief Идентификатор официальной сети, поддерживаемой текущей сборкой.
enum class NetworkKind : uint8_t {
    DEVNET = 0,
    MAINNET = 1,
};

/// \brief Известный rendezvous peer: обычный Full Node с transport-only TLS SPKI pin.
struct RendezvousLocator {
    std::string_view host;
    uint16_t port{0};
    std::array<unsigned char, 32> tls_spki_sha256{};
};

/// \brief Verified official network: compiled NetworkID, signed immutable genesis и initial state.
struct OfficialNetwork {
    NetworkKind kind{NetworkKind::DEVNET};
    std::string_view name;
    VerifiedNetworkGenesis genesis;
    CybouState genesis_state;
    std::span<const RendezvousLocator> rendezvous_locators;
};

/// \brief Возвращает verified official network по имени профиля; любые внешние файлы отвергаются.
const OfficialNetwork& RequireOfficialNetwork(std::string_view name);
/// \brief Возвращает verified official network по enumerator-сети.
const OfficialNetwork& RequireOfficialNetwork(NetworkKind kind);


} // namespace cybou

#endif // CYBOU_OFFICIAL_NETWORKS_H
