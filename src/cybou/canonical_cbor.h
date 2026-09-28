// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_CANONICAL_CBOR_H
#define CYBOU_CANONICAL_CBOR_H

#include <cstdint>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cybou {

struct CborValue {
    using ByteString = std::vector<std::uint8_t>;
    using Array = std::vector<CborValue>;
    using Map = std::vector<std::pair<CborValue, CborValue>>;
    using Storage = std::variant<std::monostate, bool, std::uint64_t, std::int64_t, ByteString, std::string, Array, Map>;

    Storage value;
    bool operator==(const CborValue& other) const;

    static CborValue Null();
    static CborValue Boolean(bool value);
    static CborValue Unsigned(std::uint64_t value);
    static CborValue Negative(std::int64_t value);
    static CborValue Bytes(ByteString value);
    static CborValue Text(std::string value);
    static CborValue ArrayValue(Array value);
    static CborValue MapValue(Map value);
};

namespace cbor_profile {
inline constexpr std::size_t MAX_ENCODED_BYTES = 256 * 1024;
inline constexpr std::size_t MAX_STRING_BYTES = 256 * 1024;
inline constexpr std::size_t MAX_CONTAINER_ITEMS = 4096;
inline constexpr std::size_t MAX_TOTAL_ITEMS = 16384;
inline constexpr std::size_t MAX_NESTING_DEPTH = 32;
} // namespace cbor_profile

/** RFC 8949 core-deterministic CBOR, restricted to the profile in this header. */
std::vector<std::uint8_t> EncodeCanonicalCbor(const CborValue& value);
CborValue DecodeCanonicalCbor(std::span<const std::uint8_t> encoded);

} // namespace cybou

#endif // CYBOU_CANONICAL_CBOR_H
