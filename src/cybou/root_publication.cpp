// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Каноническая сериализация и fee-расчёт RootPublication.

#include <cybou/root_publication.h>

#include <cybou/binary_codec.h>
#include <cybou/crypto/sha256.h>
#include <algorithm>
#include <limits>
#include <string_view>

namespace cybou {
namespace {

bool IsZero(const std::span<const unsigned char> bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](const auto byte) { return byte == 0; });
}

bool IsValid(const RootPublication& publication)
{
    // RootPublication — единственная консенсусная операция контентной плоскости,
    // поэтому её payload проверяется особенно жёстко: без нулевых корней, без пустого
    // списка капсул и без альтернативных KEM profile.
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
        BinaryWriter writer{ROOT_PUBLICATION_MAX_BYTES};
        writer.Fixed(publication.root_chunk_id);
        writer.Fixed(publication.chunk_authorization_root);
        writer.U32(publication.chunk_count);
        writer.U32(publication.lease_periods);
        writer.U16(static_cast<std::uint16_t>(publication.recipient_capsules.size()));
        for (const auto& capsule : publication.recipient_capsules) {
            writer.U16(capsule.kem_profile);
            writer.U64(capsule.key_epoch);
            writer.Fixed(capsule.encapsulation);
            writer.Fixed(capsule.wrapped_content_key);
        }
        return writer.Take();
    } catch (...) { return std::nullopt; }
}

std::optional<RootPublication> DeserializeRootPublication(std::span<const unsigned char> bytes)
{
    try {
        BinaryReader reader{bytes, ROOT_PUBLICATION_MAX_BYTES};
        RootPublication publication;
        publication.root_chunk_id = reader.Fixed<ChunkId>();
        publication.chunk_authorization_root = reader.Fixed<ChunkId>();
        publication.chunk_count = reader.U32();
        publication.lease_periods = reader.U32();
        const auto count = reader.U16();
        if (count == 0 || count > ROOT_PUBLICATION_MAX_CAPSULES) return std::nullopt;
        publication.recipient_capsules.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i) {
            RootRecipientCapsule capsule;
            capsule.kem_profile = reader.U16();
            capsule.key_epoch = reader.U64();
            capsule.encapsulation = reader.Fixed<decltype(capsule.encapsulation)>();
            capsule.wrapped_content_key = reader.Fixed<decltype(capsule.wrapped_content_key)>();
            publication.recipient_capsules.push_back(std::move(capsule));
        }
        reader.Finish();
        return IsValid(publication) ? std::optional{std::move(publication)} : std::nullopt;
    } catch (...) { return std::nullopt; }
}

std::optional<std::uint64_t> ComputeRootPublicationFee(
    const CybouProtocolParameters& params,
    const std::size_t canonical_operation_bytes, const std::uint32_t chunk_count)
{
    // Формула соответствует docs/cybou/18_ECONOMICS_FEES.md:
    // fee = per-started-KiB + per-chunk, обе части считаются детерминированно
    // по каноническому размеру операции и заявленному числу chunk-ов.
    if (canonical_operation_bytes == 0 || canonical_operation_bytes > ROOT_PUBLICATION_MAX_OPERATION_BYTES ||
        chunk_count == 0 || chunk_count > MAX_PUBLICATION_CHUNKS) return std::nullopt;
    const auto kib = (canonical_operation_bytes + 1023) / 1024;
    if (params.root_publication_fee_per_started_kib != 0 &&
        kib > std::numeric_limits<std::uint64_t>::max() / params.root_publication_fee_per_started_kib) {
        return std::nullopt;
    }
    if (params.root_publication_fee_per_chunk != 0 &&
        chunk_count > std::numeric_limits<std::uint64_t>::max() / params.root_publication_fee_per_chunk) {
        return std::nullopt;
    }
    const auto byte_fee = static_cast<std::uint64_t>(kib) * params.root_publication_fee_per_started_kib;
    const auto chunk_fee = static_cast<std::uint64_t>(chunk_count) * params.root_publication_fee_per_chunk;
    if (chunk_fee > std::numeric_limits<std::uint64_t>::max() - byte_fee) return std::nullopt;
    return byte_fee + chunk_fee;
}

std::optional<IdentityKeyId> ComputeRootPublicationPayloadCommitment(const RootPublication& publication)
{
    constexpr std::string_view domain{"CYBOU/ROOT-PUBLICATION/P4"};
    const auto encoded = SerializeRootPublication(publication);
    if (!encoded) return std::nullopt;
    IdentityKeyId digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*encoded}}, digest.data())) {
        return std::nullopt;
    }
    return digest;
}

std::optional<std::array<unsigned char, REVOKE_PUBLICATION_PAYLOAD_SIZE>> SerializeRevokePublicationPayload(
    const RevokePublicationPayload& revoke)
{
    if (revoke.publication_id.IsNull()) return std::nullopt;
    std::array<unsigned char, REVOKE_PUBLICATION_PAYLOAD_SIZE> out{};
    std::copy(revoke.publication_id.begin(), revoke.publication_id.end(), out.begin());
    return out;
}

std::optional<RevokePublicationPayload> DeserializeRevokePublicationPayload(std::span<const unsigned char> bytes)
{
    if (bytes.size() != REVOKE_PUBLICATION_PAYLOAD_SIZE) return std::nullopt;
    RevokePublicationPayload revoke;
    std::copy(bytes.begin(), bytes.end(), revoke.publication_id.begin());
    if (revoke.publication_id.IsNull()) return std::nullopt;
    return revoke;
}

std::optional<IdentityKeyId> ComputeRevokePublicationPayloadCommitment(const RevokePublicationPayload& revoke)
{
    constexpr std::string_view domain{"CYBOU/REVOKE-PUBLICATION-PAYLOAD"};
    const auto encoded = SerializeRevokePublicationPayload(revoke);
    if (!encoded) return std::nullopt;
    IdentityKeyId digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*encoded}}, digest.data())) {
        return std::nullopt;
    }
    return digest;
}

} // namespace cybou
