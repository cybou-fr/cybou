// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_HEX_H
#define CYBOU_HEX_H

#include <uint256.h>

#include <optional>
#include <string_view>

namespace cybou {

/** Parse a user-entered 256-bit hex value; accepts an optional 0x prefix and short values. */
std::optional<uint256> ParseUint256UserHex(std::string_view input);

} // namespace cybou

#endif // CYBOU_HEX_H
