// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_MAIL_ENVELOPE_H
#define CYBOU_MAIL_ENVELOPE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t MAIL_ENVELOPE_VERSION{1};
inline constexpr uint16_t MAIL_ENVELOPE_KEM_ID{0x647a};
inline constexpr uint16_t MAIL_ENVELOPE_KDF_ID{0x0001};
inline constexpr uint16_t MAIL_ENVELOPE_AEAD_ID{0x0003};
inline constexpr size_t MAIL_ENVELOPE_MAX_BYTES{64 * 1024};
inline constexpr size_t MAIL_ENVELOPE_MAX_CAPSULES{8};
inline constexpr size_t MAIL_ENVELOPE_ENC_BYTES{1120};
inline constexpr size_t MAIL_ENVELOPE_WRAPPED_CEK_BYTES{48};
inline constexpr size_t MAIL_ENVELOPE_CONTENT_TAG_BYTES{16};

struct MailRecipientCapsuleV1 {
    std::array<unsigned char, 32> device_key_id{};
    uint64_t activation_nonce{0};
    std::array<unsigned char, 32> package_commitment{};
    std::array<unsigned char, MAIL_ENVELOPE_ENC_BYTES> enc{};
    std::array<unsigned char, MAIL_ENVELOPE_WRAPPED_CEK_BYTES> wrapped_cek{};

    friend bool operator==(const MailRecipientCapsuleV1&, const MailRecipientCapsuleV1&) = default;
};

struct MailEnvelopeV1 {
    std::array<unsigned char, 32> mail_id{};
    uint64_t recipient_state_height{0};
    std::array<unsigned char, 32> recipient_state_root{};
    std::array<unsigned char, 12> content_nonce{};
    std::vector<MailRecipientCapsuleV1> capsules;
    std::vector<unsigned char> content_ciphertext;

    friend bool operator==(const MailEnvelopeV1&, const MailEnvelopeV1&) = default;
};

/** Canonically serialize a structurally valid DEV MailEnvelopeV1. */
std::optional<std::vector<unsigned char>> SerializeMailEnvelopeV1(const MailEnvelopeV1& envelope);

/**
 * Parse a canonical DEV MailEnvelopeV1. If expected_active_devices is given,
 * the capsule count must match the caller's verified finalized recipient snapshot.
 */
std::optional<MailEnvelopeV1> DeserializeMailEnvelopeV1(
    std::span<const unsigned char> bytes,
    std::optional<size_t> expected_active_devices = std::nullopt);

} // namespace cybou

#endif // CYBOU_MAIL_ENVELOPE_H
