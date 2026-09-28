// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/canonical_cbor.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace cybou {
namespace {

using Bytes = std::vector<std::uint8_t>;

void AppendArgument(Bytes& out, const std::uint8_t major, const std::uint64_t argument)
{
    const auto prefix = static_cast<std::uint8_t>(major << 5);
    if (argument < 24) {
        out.push_back(static_cast<std::uint8_t>(prefix | argument));
    } else if (argument <= 0xff) {
        out.push_back(prefix | 24);
        out.push_back(static_cast<std::uint8_t>(argument));
    } else if (argument <= 0xffff) {
        out.push_back(prefix | 25);
        for (int shift = 8; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(argument >> shift));
    } else if (argument <= 0xffffffffULL) {
        out.push_back(prefix | 26);
        for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(argument >> shift));
    } else {
        out.push_back(prefix | 27);
        for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(argument >> shift));
    }
}

void CheckOutputSize(const std::size_t size)
{
    if (size > cbor_profile::MAX_ENCODED_BYTES) throw std::length_error{"CBOR output exceeds profile limit"};
}

void AppendChecked(Bytes& out, const std::uint8_t byte)
{
    if (out.size() == cbor_profile::MAX_ENCODED_BYTES) throw std::length_error{"CBOR output exceeds profile limit"};
    out.push_back(byte);
}

void AppendChecked(Bytes& out, const Bytes& bytes)
{
    if (bytes.size() > cbor_profile::MAX_ENCODED_BYTES - out.size()) throw std::length_error{"CBOR output exceeds profile limit"};
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void AppendChecked(Bytes& out, const std::string& text)
{
    if (text.size() > cbor_profile::MAX_ENCODED_BYTES - out.size()) throw std::length_error{"CBOR output exceeds profile limit"};
    out.insert(out.end(), text.begin(), text.end());
}

bool IsValidUtf8(const std::string_view text)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = bytes[i++];
        if (lead <= 0x7f) continue;
        std::uint32_t codepoint;
        std::size_t continuation_count;
        if (lead >= 0xc2 && lead <= 0xdf) {
            codepoint = lead & 0x1f;
            continuation_count = 1;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            codepoint = lead & 0x0f;
            continuation_count = 2;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            codepoint = lead & 0x07;
            continuation_count = 3;
        } else {
            return false;
        }
        if (continuation_count > text.size() - i) return false;
        for (std::size_t j = 0; j < continuation_count; ++j) {
            const auto next = bytes[i++];
            if ((next & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if ((continuation_count == 1 && codepoint < 0x80) ||
            (continuation_count == 2 && codepoint < 0x800) ||
            (continuation_count == 3 && codepoint < 0x10000) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff) {
            return false;
        }
    }
    return true;
}

struct Encoder {
    std::size_t item_count{0};

    void AddItem()
    {
        if (++item_count > cbor_profile::MAX_TOTAL_ITEMS) throw std::length_error{"CBOR item count exceeds profile limit"};
    }

    void Encode(const CborValue& item, Bytes& out, const std::size_t depth)
    {
        AddItem();
        if (depth > cbor_profile::MAX_NESTING_DEPTH) throw std::length_error{"CBOR nesting exceeds profile limit"};

        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                AppendChecked(out, 0xf6);
            } else if constexpr (std::is_same_v<T, bool>) {
                AppendChecked(out, value ? 0xf5 : 0xf4);
            } else if constexpr (std::is_same_v<T, std::uint64_t>) {
                AppendArgument(out, 0, value);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                if (value >= 0) throw std::invalid_argument{"CBOR negative integer must be less than zero"};
                AppendArgument(out, 1, static_cast<std::uint64_t>(-(value + 1)));
            } else if constexpr (std::is_same_v<T, Bytes>) {
                if (value.size() > cbor_profile::MAX_STRING_BYTES) throw std::length_error{"CBOR byte string exceeds profile limit"};
                AppendArgument(out, 2, value.size());
                AppendChecked(out, value);
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (value.size() > cbor_profile::MAX_STRING_BYTES) throw std::length_error{"CBOR text exceeds profile limit"};
                if (!IsValidUtf8(value)) throw std::invalid_argument{"CBOR text is not valid UTF-8"};
                AppendArgument(out, 3, value.size());
                AppendChecked(out, value);
            } else if constexpr (std::is_same_v<T, CborValue::Array>) {
                if (value.size() > cbor_profile::MAX_CONTAINER_ITEMS) throw std::length_error{"CBOR array exceeds profile limit"};
                AppendArgument(out, 4, value.size());
                for (const auto& child : value) Encode(child, out, depth + 1);
            } else if constexpr (std::is_same_v<T, CborValue::Map>) {
                if (value.size() > cbor_profile::MAX_CONTAINER_ITEMS) throw std::length_error{"CBOR map exceeds profile limit"};
                struct EncodedPair {
                    Bytes key;
                    const CborValue* value;
                    std::size_t key_item_count;
                };
                std::vector<EncodedPair> entries;
                entries.reserve(value.size());
                for (const auto& [key, mapped] : value) {
                    Encoder key_encoder;
                    Bytes encoded_key;
                    key_encoder.Encode(key, encoded_key, depth + 1);
                    entries.push_back({std::move(encoded_key), &mapped, key_encoder.item_count});
                }
                std::sort(entries.begin(), entries.end(), [](const EncodedPair& a, const EncodedPair& b) {
                    return std::lexicographical_compare(a.key.begin(), a.key.end(), b.key.begin(), b.key.end());
                });
                for (std::size_t i = 1; i < entries.size(); ++i) {
                    if (entries[i - 1].key == entries[i].key) throw std::invalid_argument{"CBOR map has duplicate keys"};
                }
                AppendArgument(out, 5, entries.size());
                for (const auto& entry : entries) {
                    if (entry.key_item_count > cbor_profile::MAX_TOTAL_ITEMS - item_count) throw std::length_error{"CBOR item count exceeds profile limit"};
                    item_count += entry.key_item_count;
                    AppendChecked(out, entry.key);
                    Encode(*entry.value, out, depth + 1);
                }
            }
            CheckOutputSize(out.size());
        }, item.value);
    }
};

struct Decoder {
    std::span<const std::uint8_t> input;
    std::size_t offset{0};
    std::size_t item_count{0};

    std::uint8_t ReadByte()
    {
        if (offset == input.size()) throw std::invalid_argument{"truncated CBOR item"};
        return input[offset++];
    }

    std::uint64_t ReadArgument(const std::uint8_t additional)
    {
        if (additional < 24) return additional;
        std::size_t bytes;
        std::uint64_t minimum;
        switch (additional) {
        case 24: bytes = 1; minimum = 24; break;
        case 25: bytes = 2; minimum = 0x100; break;
        case 26: bytes = 4; minimum = 0x10000; break;
        case 27: bytes = 8; minimum = 0x100000000ULL; break;
        default: throw std::invalid_argument{"reserved or indefinite CBOR argument"};
        }
        if (bytes > input.size() - offset) throw std::invalid_argument{"truncated CBOR argument"};
        std::uint64_t result{0};
        for (std::size_t i = 0; i < bytes; ++i) result = (result << 8) | ReadByte();
        if (result < minimum) throw std::invalid_argument{"non-preferred CBOR integer or length"};
        return result;
    }

    std::size_t ReadLength(const std::uint64_t value, const std::size_t maximum)
    {
        if (value > maximum || value > input.size() - offset) throw std::length_error{"CBOR length exceeds available or allowed bytes"};
        return static_cast<std::size_t>(value);
    }

    CborValue Decode(const std::size_t depth)
    {
        if (++item_count > cbor_profile::MAX_TOTAL_ITEMS) throw std::length_error{"CBOR item count exceeds profile limit"};
        if (depth > cbor_profile::MAX_NESTING_DEPTH) throw std::length_error{"CBOR nesting exceeds profile limit"};
        const auto initial = ReadByte();
        const auto major = static_cast<std::uint8_t>(initial >> 5);
        const auto additional = static_cast<std::uint8_t>(initial & 0x1f);

        if (major == 7) {
            if (additional == 20) return CborValue::Boolean(false);
            if (additional == 21) return CborValue::Boolean(true);
            if (additional == 22) return CborValue::Null();
            throw std::invalid_argument{"unsupported CBOR simple or floating-point value"};
        }

        if (major == 6) throw std::invalid_argument{"CBOR tags are not permitted by this profile"};
        const auto argument = ReadArgument(additional);
        switch (major) {
        case 0:
            return CborValue::Unsigned(argument);
        case 1:
            if (argument > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument{"CBOR negative integer outside supported range"};
            return CborValue::Negative(-1 - static_cast<std::int64_t>(argument));
        case 2: {
            const auto length = ReadLength(argument, cbor_profile::MAX_STRING_BYTES);
            CborValue::ByteString bytes(input.begin() + offset, input.begin() + offset + length);
            offset += length;
            return CborValue::Bytes(std::move(bytes));
        }
        case 3: {
            const auto length = ReadLength(argument, cbor_profile::MAX_STRING_BYTES);
            std::string text(input.begin() + offset, input.begin() + offset + length);
            offset += length;
            if (!IsValidUtf8(text)) throw std::invalid_argument{"CBOR text is not valid UTF-8"};
            return CborValue::Text(std::move(text));
        }
        case 4: {
            if (argument > cbor_profile::MAX_CONTAINER_ITEMS || argument > cbor_profile::MAX_TOTAL_ITEMS - item_count) throw std::length_error{"CBOR array exceeds profile limit"};
            CborValue::Array items;
            items.reserve(static_cast<std::size_t>(argument));
            for (std::uint64_t i = 0; i < argument; ++i) items.push_back(Decode(depth + 1));
            return CborValue::ArrayValue(std::move(items));
        }
        case 5: {
            if (argument > cbor_profile::MAX_CONTAINER_ITEMS || argument > (cbor_profile::MAX_TOTAL_ITEMS - item_count) / 2) throw std::length_error{"CBOR map exceeds profile limit"};
            CborValue::Map entries;
            entries.reserve(static_cast<std::size_t>(argument));
            Bytes previous_key;
            for (std::uint64_t i = 0; i < argument; ++i) {
                const auto key_start = offset;
                auto key = Decode(depth + 1);
                Bytes encoded_key(input.begin() + key_start, input.begin() + offset);
                if (!previous_key.empty() && !std::lexicographical_compare(previous_key.begin(), previous_key.end(), encoded_key.begin(), encoded_key.end())) {
                    throw std::invalid_argument{"CBOR map keys are duplicated or not in deterministic order"};
                }
                previous_key = std::move(encoded_key);
                entries.emplace_back(std::move(key), Decode(depth + 1));
            }
            return CborValue::MapValue(std::move(entries));
        }
        default:
            throw std::invalid_argument{"unsupported CBOR major type"};
        }
    }
};

} // namespace

CborValue CborValue::Null() { return {{}}; }
CborValue CborValue::Boolean(const bool value) { return {{value}}; }
CborValue CborValue::Unsigned(const std::uint64_t value) { return {{value}}; }
CborValue CborValue::Negative(const std::int64_t value) { return {{value}}; }
CborValue CborValue::Bytes(ByteString value) { return {{std::move(value)}}; }
CborValue CborValue::Text(std::string value) { return {{std::move(value)}}; }
CborValue CborValue::ArrayValue(Array value) { return {{std::move(value)}}; }
CborValue CborValue::MapValue(Map value) { return {{std::move(value)}}; }

bool CborValue::operator==(const CborValue& other) const
{
    if (value.index() != other.value.index()) return false;
    const auto* left_map = std::get_if<Map>(&value);
    if (!left_map) return value == other.value;

    const auto& right_map = std::get<Map>(other.value);
    if (left_map->size() != right_map.size()) return false;
    std::vector<bool> matched(right_map.size(), false);
    for (const auto& left_entry : *left_map) {
        bool found = false;
        for (std::size_t i = 0; i < right_map.size(); ++i) {
            if (!matched[i] && left_entry == right_map[i]) {
                matched[i] = true;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

std::vector<std::uint8_t> EncodeCanonicalCbor(const CborValue& value)
{
    Bytes result;
    Encoder{}.Encode(value, result, 0);
    return result;
}

CborValue DecodeCanonicalCbor(const std::span<const std::uint8_t> encoded)
{
    if (encoded.size() > cbor_profile::MAX_ENCODED_BYTES) throw std::length_error{"CBOR input exceeds profile limit"};
    Decoder decoder{encoded};
    auto result = decoder.Decode(0);
    if (decoder.offset != encoded.size()) throw std::invalid_argument{"trailing bytes after CBOR item"};
    return result;
}

} // namespace cybou
