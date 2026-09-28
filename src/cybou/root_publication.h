// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_ROOT_PUBLICATION_H
#define CYBOU_ROOT_PUBLICATION_H

#include <cybou/chunk_id.h>
#include <cybou/identity_kem.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr std::size_t ROOT_PUBLICATION_MAX_BYTES{128 * 1024};
inline constexpr std::size_t ROOT_PUBLICATION_MAX_OPERATION_BYTES{144 * 1024};
inline constexpr std::size_t ROOT_PUBLICATION_MAX_CHUNKS{2048};
inline constexpr std::uint64_t ROOT_PUBLICATION_MAX_STORED_BYTES{512ULL * 1024 * 1024};
inline constexpr std::size_t ROOT_PUBLICATION_MAX_CAPSULES{64};
inline constexpr std::size_t ROOT_CAPSULE_WRAPPED_KEY_BYTES{60};
inline constexpr std::size_t ROOT_CAPSULE_NONCE_BYTES{12};
inline constexpr std::size_t ROOT_PUBLICATION_MIN_CHUNK_STORED_BYTES{1089};
inline constexpr std::uint64_t ROOT_PUBLICATION_FEE_PER_STARTED_KIB{4};

struct RootRecipientCapsule {
    std::uint16_t kem_profile{IDENTITY_KEM_PROFILE_XWING};
    std::uint64_t key_epoch{0};
    std::array<unsigned char, 32> package_commitment{};
    XWingCiphertext encapsulation{};
    std::array<unsigned char, ROOT_CAPSULE_WRAPPED_KEY_BYTES> wrapped_content_key{};

    friend bool operator==(const RootRecipientCapsule&, const RootRecipientCapsule&) = default;
};

struct RootPublication {
    ChunkId root_chunk_id{};
    ChunkId chunk_authorization_root{};
    std::uint32_t chunk_count{0};
    std::uint64_t authorized_stored_bytes{0};
    std::vector<RootRecipientCapsule> recipient_capsules;

    friend bool operator==(const RootPublication&, const RootPublication&) = default;
};

/** Canonical CBOR body; Identity authorization is carried by the outer operation. */
std::optional<std::vector<unsigned char>> SerializeRootPublication(const RootPublication& publication);
std::optional<RootPublication> DeserializeRootPublication(std::span<const unsigned char> bytes);
std::optional<std::uint64_t> ComputeRootPublicationFee(std::size_t canonical_operation_bytes);

} // namespace cybou

#endif // CYBOU_ROOT_PUBLICATION_H
