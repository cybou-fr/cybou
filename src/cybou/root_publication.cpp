// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/root_publication.h>

#include <cybou/canonical_cbor.h>
#include <cybou/crypto/sha256.h>
#include <algorithm>
#include <limits>
#include <string_view>

namespace cybou {
namespace {

constexpr std::uint64_t ROOT_PUBLICATION_WIRE_VERSION{3};

bool IsZero(const std::span<const unsigned char> bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](const auto byte) { return byte == 0; });
}

CborValue ByteString(const std::span<const unsigned char> bytes)
{
    return CborValue::Bytes(CborValue::ByteString{bytes.begin(), bytes.end()});
}

CborValue ToCbor(const RootRecipientCapsule& capsule)
{
    return CborValue::ArrayValue({
        CborValue::Unsigned(capsule.kem_profile),
        CborValue::Unsigned(capsule.key_epoch),
        ByteString(capsule.encapsulation),
        ByteString(capsule.wrapped_content_key),
    });
}

CborValue ToCbor(const RootPublication& publication)
{
    CborValue::Array capsules;
    capsules.reserve(publication.recipient_capsules.size());
    for (const auto& capsule : publication.recipient_capsules) capsules.push_back(ToCbor(capsule));
    return CborValue::MapValue({
        {CborValue::Unsigned(0), CborValue::Unsigned(ROOT_PUBLICATION_WIRE_VERSION)},
        {CborValue::Unsigned(1), ByteString(publication.root_chunk_id)},
        {CborValue::Unsigned(2), ByteString(publication.chunk_authorization_root)},
        {CborValue::Unsigned(3), CborValue::Unsigned(publication.chunk_count)},
        {CborValue::Unsigned(4), CborValue::ArrayValue(std::move(capsules))},
    });
}

std::optional<std::uint64_t> GetUnsigned(const CborValue& value)
{
    if (const auto* number = std::get_if<std::uint64_t>(&value.value)) return *number;
    return std::nullopt;
}

bool ReadFixedBytes(const CborValue& value, std::span<unsigned char> destination)
{
    const auto* bytes = std::get_if<CborValue::ByteString>(&value.value);
    if (bytes == nullptr || bytes->size() != destination.size()) return false;
    std::copy(bytes->begin(), bytes->end(), destination.begin());
    return true;
}

bool IsValid(const RootPublication& publication)
{
    if (IsZero(publication.root_chunk_id) || IsZero(publication.chunk_authorization_root) ||
        publication.chunk_count == 0 ||
        publication.recipient_capsules.empty() ||
        publication.recipient_capsules.size() > ROOT_PUBLICATION_MAX_CAPSULES) return false;

    return std::all_of(publication.recipient_capsules.begin(), publication.recipient_capsules.end(),
        [](const auto& capsule) {
            return capsule.kem_profile == IDENTITY_KEM_PROFILE_XWING;
        });
}

} // namespace

std::optional<std::vector<unsigned char>> SerializeRootPublication(const RootPublication& publication)
{
    if (!IsValid(publication)) return std::nullopt;
    try {
        const auto encoded = EncodeCanonicalCbor(ToCbor(publication));
        if (encoded.empty() || encoded.size() > ROOT_PUBLICATION_MAX_BYTES) return std::nullopt;
        return std::vector<unsigned char>{encoded.begin(), encoded.end()};
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<RootPublication> DeserializeRootPublication(const std::span<const unsigned char> bytes)
{
    if (bytes.empty() || bytes.size() > ROOT_PUBLICATION_MAX_BYTES) return std::nullopt;
    try {
        const auto decoded = DecodeCanonicalCbor(bytes);
        const auto* fields = std::get_if<CborValue::Map>(&decoded.value);
        if (fields == nullptr || fields->size() != 5) return std::nullopt;
        for (std::size_t index = 0; index < fields->size(); ++index) {
            const auto key = GetUnsigned((*fields)[index].first);
            if (!key || *key != index) return std::nullopt;
        }
        const auto version = GetUnsigned((*fields)[0].second);
        const auto chunk_count = GetUnsigned((*fields)[3].second);
        const auto* capsules = std::get_if<CborValue::Array>(&(*fields)[4].second.value);
        if (!version || *version != ROOT_PUBLICATION_WIRE_VERSION || !chunk_count ||
            *chunk_count > std::numeric_limits<std::uint32_t>::max() ||
            capsules == nullptr || capsules->size() > ROOT_PUBLICATION_MAX_CAPSULES) return std::nullopt;

        RootPublication publication;
        if (!ReadFixedBytes((*fields)[1].second, publication.root_chunk_id) ||
            !ReadFixedBytes((*fields)[2].second, publication.chunk_authorization_root)) return std::nullopt;
        publication.chunk_count = static_cast<std::uint32_t>(*chunk_count);
        publication.recipient_capsules.reserve(capsules->size());
        for (const auto& capsule_value : *capsules) {
            const auto* capsule_fields = std::get_if<CborValue::Array>(&capsule_value.value);
            if (capsule_fields == nullptr || capsule_fields->size() != 4) return std::nullopt;
            const auto profile = GetUnsigned((*capsule_fields)[0]);
            const auto key_epoch = GetUnsigned((*capsule_fields)[1]);
            if (!profile || *profile > std::numeric_limits<std::uint16_t>::max() || !key_epoch) return std::nullopt;
            RootRecipientCapsule capsule;
            capsule.kem_profile = static_cast<std::uint16_t>(*profile);
            capsule.key_epoch = *key_epoch;
            if (!ReadFixedBytes((*capsule_fields)[2], capsule.encapsulation) ||
                !ReadFixedBytes((*capsule_fields)[3], capsule.wrapped_content_key)) return std::nullopt;
            publication.recipient_capsules.push_back(std::move(capsule));
        }
        if (!IsValid(publication)) return std::nullopt;
        const auto canonical = SerializeRootPublication(publication);
        if (!canonical || !std::equal(canonical->begin(), canonical->end(), bytes.begin(), bytes.end())) return std::nullopt;
        return publication;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::uint64_t> ComputeRootPublicationFee(
    const std::size_t canonical_operation_bytes, const std::uint32_t chunk_count)
{
    if (canonical_operation_bytes == 0 || canonical_operation_bytes > ROOT_PUBLICATION_MAX_OPERATION_BYTES ||
        chunk_count == 0 || chunk_count > ROOT_PUBLICATION_MAX_CHUNKS) return std::nullopt;
    const auto kib = (canonical_operation_bytes + 1023) / 1024;
    const auto byte_fee = static_cast<std::uint64_t>(kib) * ROOT_PUBLICATION_FEE_PER_STARTED_KIB;
    const auto chunk_fee = static_cast<std::uint64_t>(chunk_count) * ROOT_PUBLICATION_FEE_PER_CHUNK;
    if (chunk_fee > std::numeric_limits<std::uint64_t>::max() - byte_fee) return std::nullopt;
    return byte_fee + chunk_fee;
}

std::optional<IdentityKeyId> ComputeRootPublicationPayloadCommitment(const RootPublication& publication)
{
    constexpr std::string_view domain{"CYBOU/ROOT-PUBLICATION/P3"};
    const auto encoded = SerializeRootPublication(publication);
    if (!encoded) return std::nullopt;
    IdentityKeyId digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*encoded}}, digest.data())) {
        return std::nullopt;
    }
    return digest;
}

} // namespace cybou
