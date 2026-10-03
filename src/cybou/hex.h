// Copyright (c) 2026 CYBOU contributors
#ifndef CYBOU_HEX_H
#define CYBOU_HEX_H
#include <cybou/hash256.h>
#include <span>
namespace cybou {
/** Strict 64-digit forward hex, without prefixes or numeric padding. */
std::optional<Hash256> ParseHash256UserHex(std::string_view input);
std::string HexEncode(std::span<const unsigned char> bytes);
}
#endif
