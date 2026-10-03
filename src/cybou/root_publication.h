// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Канонический wire-формат RootPublication и capsule helpers.

#ifndef CYBOU_ROOT_PUBLICATION_H
#define CYBOU_ROOT_PUBLICATION_H

#include <cybou/protocol_limits.h>
#include <cybou/chunk_id.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/identity_kem.h>
#include <cybou/identity_registry.h>
#include <cybou/protocol_params.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief Верхние границы канонического формата RootPublication.
inline constexpr std::size_t ROOT_PUBLICATION_MAX_BYTES{128 * 1024};
inline constexpr std::size_t ROOT_PUBLICATION_MAX_OPERATION_BYTES{144 * 1024};
inline constexpr std::size_t ROOT_PUBLICATION_MAX_CAPSULES{32};
inline constexpr std::size_t ROOT_CAPSULE_WRAPPED_KEY_BYTES{60};
inline constexpr std::size_t ROOT_CAPSULE_NONCE_BYTES{12};

/// \brief Opaque recipient capsule, оборачивающая ContentKey под KEM-ключ получателя.
struct RootRecipientCapsule {
    std::uint16_t kem_profile{IDENTITY_KEM_PROFILE_XWING};
    std::uint64_t key_epoch{0};
    XWingCiphertext encapsulation{};
    std::array<unsigned char, ROOT_CAPSULE_WRAPPED_KEY_BYTES> wrapped_content_key{};

    friend bool operator==(const RootRecipientCapsule&, const RootRecipientCapsule&) = default;
};

/// \brief Единственная каноническая операция публикации контента в консенсусе.
struct RootPublication {
    ChunkId root_chunk_id{};
    ChunkId chunk_authorization_root{};
    std::uint32_t chunk_count{0};
    std::vector<RootRecipientCapsule> recipient_capsules;

    friend bool operator==(const RootPublication&, const RootPublication&) = default;
};

/// \brief Identity-authorized RootPublication, пригодная для включения в блок.
struct AuthorizedRootPublication {
    IdentityOperationAuthorization authorization;
    RootPublication publication;

    friend bool operator==(const AuthorizedRootPublication&, const AuthorizedRootPublication&) = default;
};

/// \brief Сериализует RootPublication в текущий канонический бинарный формат.
std::optional<std::vector<unsigned char>> SerializeRootPublication(const RootPublication& publication);
/// \brief Десериализует и валидирует RootPublication.
std::optional<RootPublication> DeserializeRootPublication(std::span<const unsigned char> bytes);
/// \brief Вычисляет протокольную комиссию RootPublication из размера операции и числа chunk'ов.
std::optional<std::uint64_t> ComputeRootPublicationFee(
    const CybouProtocolParameters& params,
    std::size_t canonical_operation_bytes, std::uint32_t chunk_count);
/// \brief Вычисляет payload commitment RootPublication.
std::optional<IdentityKeyId> ComputeRootPublicationPayloadCommitment(const RootPublication& publication);

/// \brief Создаёт recipient capsule для публикации корневого контента.
std::optional<RootRecipientCapsule> CreateRootRecipientCapsule(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> sender_account_id,
    std::uint64_t sender_nonce,
    std::uint64_t sender_key_epoch,
    const ChunkId& root_chunk_id,
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> recipient_public_key,
    std::uint64_t recipient_key_epoch,
    std::span<const unsigned char, 32> content_key);

/// \brief Открывает recipient capsule локальным seed'ом получателя и извлекает ContentKey.
std::optional<ContentKey> OpenRootRecipientCapsule(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> sender_account_id,
    std::uint64_t sender_nonce,
    std::uint64_t sender_key_epoch,
    const ChunkId& root_chunk_id,
    const RootRecipientCapsule& capsule,
    std::span<const unsigned char, XWING_SEED_SIZE> recipient_seed);

} // namespace cybou

#endif // CYBOU_ROOT_PUBLICATION_H
