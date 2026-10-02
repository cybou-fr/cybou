// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BOOTSTRAP_NODES_H
#define CYBOU_BOOTSTRAP_NODES_H

#include <cybou/official_networks.h>

#include <array>
#include <cstdint>
#include <string_view>

namespace cybou {

using InitialBootstrapLocator = OfficialBootstrapLocator;

inline constexpr auto& CYBOU_INITIAL_BOOTSTRAP_LOCATORS = OFFICIAL_DEVNET_BOOTSTRAP_LOCATORS;

} // namespace cybou

#endif // CYBOU_BOOTSTRAP_NODES_H
