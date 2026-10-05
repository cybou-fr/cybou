// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
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

/// \brief Верхние границы канонического формата `RootPublication`.
inline constexpr std::size_t ROOT_PUBLICATION_MAX_BYTES{128 * 1024};
inline constexpr std::size_t ROOT_PUBLICATION_MAX_OPERATION_BYTES{144 * 1024};
inline constexpr std::size_t ROOT_PUBLICATION_MAX_CAPSULES{32};
/// \brief Размер поля `wrapped_content_key`.
/// \details ChaCha20-Poly1305 nonce 12 байт + ciphertext `ContentKey` 32 байта + tag 16 байт = 60.
inline constexpr std::size_t ROOT_CAPSULE_WRAPPED_KEY_BYTES{60};
/// \brief Размер ChaCha20-Poly1305 nonce в recipient capsule.
inline constexpr std::size_t ROOT_CAPSULE_NONCE_BYTES{12};

/// \brief Opaque recipient capsule, оборачивающая ContentKey под KEM-ключ получателя.
struct RootRecipientCapsule {
    std::uint16_t kem_profile{IDENTITY_KEM_PROFILE_XWING}; ///< Единственный поддерживаемый KEM profile для capsule.
    std::uint64_t key_epoch{0}; ///< Epoch ключей получателя, для которого капсула создана.
    XWingCiphertext encapsulation{}; ///< X-Wing ciphertext, несущий shared secret для обёртки ContentKey.
    std::array<unsigned char, ROOT_CAPSULE_WRAPPED_KEY_BYTES> wrapped_content_key{}; ///< `nonce || ciphertext || tag`.

    friend bool operator==(const RootRecipientCapsule&, const RootRecipientCapsule&) = default;
};

/// \brief Единственная каноническая операция публикации контента в консенсусе.
struct RootPublication {
    ChunkId root_chunk_id{}; ///< Корневой ChunkId опубликованного зашифрованного дерева.
    ChunkId chunk_authorization_root{}; ///< Merkle root авторизации chunk-ов для finality-first storage admission.
    std::uint32_t chunk_count{0}; ///< Число авторизованных chunk-ов.
    /// \brief Settlement-периоды начальной аренды, оплачиваемой атомарно с публикацией (DEC-279);
    ///        0 — без аренды: remote storage admission начнётся только после StorageLease.
    std::uint32_t lease_periods{0};
    std::vector<RootRecipientCapsule> recipient_capsules; ///< Капсулы для получателей ContentKey.

    friend bool operator==(const RootPublication&, const RootPublication&) = default;
};

/// \brief Identity-authorized RootPublication, пригодная для включения в блок.
struct AuthorizedRootPublication {
    IdentityOperationAuthorization authorization;
    RootPublication publication;

    friend bool operator==(const AuthorizedRootPublication&, const AuthorizedRootPublication&) = default;
};

/// \brief Размер payload RevokePublication: OperationID отзываемой публикации.
inline constexpr std::size_t REVOKE_PUBLICATION_PAYLOAD_SIZE{32};

/// \brief Отзыв собственной финализированной RootPublication её автором.
/// \details Запись удаляется из регистра публикаций, квота освобождается, а Full Node,
///          хранящие её chunk-и, могут немедленно их удалить (DEC-271, DEC-272).
struct RevokePublicationPayload {
    cybou::Hash256 publication_id; ///< OperationID финализированной RootPublication.

    friend bool operator==(const RevokePublicationPayload&, const RevokePublicationPayload&) = default;
};

/// \brief Identity-authorized RevokePublication.
struct AuthorizedRevokePublication {
    IdentityOperationAuthorization authorization;
    RevokePublicationPayload revoke;

    friend bool operator==(const AuthorizedRevokePublication&, const AuthorizedRevokePublication&) = default;
};

/// \brief Сериализует payload RevokePublication; нулевой publication_id недопустим.
std::optional<std::array<unsigned char, REVOKE_PUBLICATION_PAYLOAD_SIZE>> SerializeRevokePublicationPayload(
    const RevokePublicationPayload& revoke);
/// \brief Десериализует payload RevokePublication с точным потреблением.
std::optional<RevokePublicationPayload> DeserializeRevokePublicationPayload(std::span<const unsigned char> bytes);
/// \brief Domain-separated коммитмент payload для Identity authorization.
std::optional<IdentityKeyId> ComputeRevokePublicationPayloadCommitment(const RevokePublicationPayload& revoke);

/// \brief Сериализует RootPublication в текущий канонический бинарный формат.
std::optional<std::vector<unsigned char>> SerializeRootPublication(const RootPublication& publication);
/// \brief Десериализует и валидирует RootPublication.
std::optional<RootPublication> DeserializeRootPublication(std::span<const unsigned char> bytes);
/// \brief Вычисляет протокольную комиссию `RootPublication` из размера операции и числа chunk'ов.
/// \return `std::nullopt` при нулевых/слишком больших значениях или арифметическом переполнении.
std::optional<std::uint64_t> ComputeRootPublicationFee(
    const CybouProtocolParameters& params,
    std::size_t canonical_operation_bytes, std::uint32_t chunk_count);
/// \brief Вычисляет payload commitment RootPublication.
std::optional<IdentityKeyId> ComputeRootPublicationPayloadCommitment(const RootPublication& publication);

/// \brief Создаёт recipient capsule для публикации корневого контента.
/// \return Капсулу только при корректных входных ключах и успешной KEM/AEAD-обёртке; иначе `std::nullopt`.
/// \note Функция не меняет консенсусное состояние; это локальный helper публикации.
std::optional<RootRecipientCapsule> CreateRootRecipientCapsule(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> sender_account_id,
    std::uint64_t sender_nonce,
    std::uint64_t sender_key_epoch,
    const ChunkId& root_chunk_id,
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> recipient_public_key,
    std::uint64_t recipient_key_epoch,
    std::span<const unsigned char, 32> content_key);

/// \brief Открывает recipient capsule локальным seed'ом получателя и извлекает `ContentKey`.
/// \return `ContentKey` только если KEM decapsulation и AEAD-проверка прошли успешно; иначе `std::nullopt`.
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
