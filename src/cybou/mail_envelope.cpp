// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/mail_envelope.h>

#include <algorithm>
#include <array>
#include <limits>

namespace cybou {
namespace {

constexpr size_t FIXED_HEADER_BYTES{1 + 2 + 2 + 2 + 32 + 8 + 32 + 12 + 1 + 4};
constexpr size_t CAPSULE_BYTES{32 + 8 + 32 + MAIL_ENVELOPE_ENC_BYTES + MAIL_ENVELOPE_WRAPPED_CEK_BYTES};

bool IsZero(std::span<const unsigned char> bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](unsigned char byte) { return byte == 0; });
}

void AppendU16Be(std::vector<unsigned char>& out, uint16_t value)
{
    out.push_back(static_cast<unsigned char>(value >> 8));
    out.push_back(static_cast<unsigned char>(value));
}

void AppendU32Le(std::vector<unsigned char>& out, uint32_t value)
{
    for (unsigned int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

void AppendU64Le(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint16_t ReadU16Be(std::span<const unsigned char> bytes, size_t offset)
{
    return (uint16_t{bytes[offset]} << 8) | uint16_t{bytes[offset + 1]};
}

uint32_t ReadU32Le(std::span<const unsigned char> bytes, size_t offset)
{
    uint32_t value{0};
    for (unsigned int i = 0; i < 4; ++i) value |= uint32_t{bytes[offset + i]} << (8 * i);
    return value;
}

uint64_t ReadU64Le(std::span<const unsigned char> bytes, size_t offset)
{
    uint64_t value{0};
    for (unsigned int i = 0; i < 8; ++i) value |= uint64_t{bytes[offset + i]} << (8 * i);
    return value;
}

bool Valid(const MailEnvelopeV1& envelope, std::optional<size_t> expected_active_devices)
{
    if (IsZero(envelope.mail_id) || envelope.capsules.empty() ||
        envelope.capsules.size() > MAIL_ENVELOPE_MAX_CAPSULES ||
        (expected_active_devices && envelope.capsules.size() != *expected_active_devices) ||
        envelope.content_ciphertext.size() < MAIL_ENVELOPE_CONTENT_TAG_BYTES ||
        envelope.content_ciphertext.size() > MAIL_ENVELOPE_MAX_BYTES) {
        return false;
    }

    for (size_t i = 0; i < envelope.capsules.size(); ++i) {
        const auto& capsule = envelope.capsules[i];
        if (IsZero(capsule.device_key_id) || IsZero(capsule.package_commitment)) return false;
        if (i != 0 && !(envelope.capsules[i - 1].device_key_id < capsule.device_key_id)) return false;
    }

    if (envelope.capsules.size() > (std::numeric_limits<size_t>::max() - FIXED_HEADER_BYTES) / CAPSULE_BYTES) return false;
    const size_t overhead = FIXED_HEADER_BYTES + envelope.capsules.size() * CAPSULE_BYTES;
    return overhead <= MAIL_ENVELOPE_MAX_BYTES && envelope.content_ciphertext.size() <= MAIL_ENVELOPE_MAX_BYTES - overhead;
}

} // namespace

std::optional<std::vector<unsigned char>> SerializeMailEnvelopeV1(const MailEnvelopeV1& envelope)
{
    if (!Valid(envelope, std::nullopt)) return std::nullopt;

    std::vector<unsigned char> out;
    out.reserve(FIXED_HEADER_BYTES + envelope.capsules.size() * CAPSULE_BYTES + envelope.content_ciphertext.size());
    out.push_back(MAIL_ENVELOPE_VERSION);
    AppendU16Be(out, MAIL_ENVELOPE_KEM_ID);
    AppendU16Be(out, MAIL_ENVELOPE_KDF_ID);
    AppendU16Be(out, MAIL_ENVELOPE_AEAD_ID);
    out.insert(out.end(), envelope.mail_id.begin(), envelope.mail_id.end());
    AppendU64Le(out, envelope.recipient_state_height);
    out.insert(out.end(), envelope.recipient_state_root.begin(), envelope.recipient_state_root.end());
    out.insert(out.end(), envelope.content_nonce.begin(), envelope.content_nonce.end());
    out.push_back(static_cast<unsigned char>(envelope.capsules.size()));

    for (const auto& capsule : envelope.capsules) {
        out.insert(out.end(), capsule.device_key_id.begin(), capsule.device_key_id.end());
        AppendU64Le(out, capsule.activation_nonce);
        out.insert(out.end(), capsule.package_commitment.begin(), capsule.package_commitment.end());
        out.insert(out.end(), capsule.enc.begin(), capsule.enc.end());
        out.insert(out.end(), capsule.wrapped_cek.begin(), capsule.wrapped_cek.end());
    }

    AppendU32Le(out, static_cast<uint32_t>(envelope.content_ciphertext.size()));
    out.insert(out.end(), envelope.content_ciphertext.begin(), envelope.content_ciphertext.end());
    return out;
}

std::optional<MailEnvelopeV1> DeserializeMailEnvelopeV1(
    std::span<const unsigned char> bytes,
    std::optional<size_t> expected_active_devices)
{
    if (bytes.size() < FIXED_HEADER_BYTES + CAPSULE_BYTES + MAIL_ENVELOPE_CONTENT_TAG_BYTES ||
        bytes.size() > MAIL_ENVELOPE_MAX_BYTES || bytes[0] != MAIL_ENVELOPE_VERSION ||
        ReadU16Be(bytes, 1) != MAIL_ENVELOPE_KEM_ID ||
        ReadU16Be(bytes, 3) != MAIL_ENVELOPE_KDF_ID ||
        ReadU16Be(bytes, 5) != MAIL_ENVELOPE_AEAD_ID) {
        return std::nullopt;
    }

    MailEnvelopeV1 envelope;
    size_t offset{7};
    std::copy_n(bytes.begin() + offset, envelope.mail_id.size(), envelope.mail_id.begin());
    offset += envelope.mail_id.size();
    envelope.recipient_state_height = ReadU64Le(bytes, offset);
    offset += 8;
    std::copy_n(bytes.begin() + offset, envelope.recipient_state_root.size(), envelope.recipient_state_root.begin());
    offset += envelope.recipient_state_root.size();
    std::copy_n(bytes.begin() + offset, envelope.content_nonce.size(), envelope.content_nonce.begin());
    offset += envelope.content_nonce.size();

    const size_t capsule_count = bytes[offset++];
    if (capsule_count == 0 || capsule_count > MAIL_ENVELOPE_MAX_CAPSULES ||
        (expected_active_devices && capsule_count != *expected_active_devices) ||
        capsule_count > (bytes.size() - offset) / CAPSULE_BYTES) {
        return std::nullopt;
    }
    envelope.capsules.reserve(capsule_count);
    for (size_t i = 0; i < capsule_count; ++i) {
        MailRecipientCapsuleV1 capsule;
        std::copy_n(bytes.begin() + offset, capsule.device_key_id.size(), capsule.device_key_id.begin());
        offset += capsule.device_key_id.size();
        capsule.activation_nonce = ReadU64Le(bytes, offset);
        offset += 8;
        std::copy_n(bytes.begin() + offset, capsule.package_commitment.size(), capsule.package_commitment.begin());
        offset += capsule.package_commitment.size();
        std::copy_n(bytes.begin() + offset, capsule.enc.size(), capsule.enc.begin());
        offset += capsule.enc.size();
        std::copy_n(bytes.begin() + offset, capsule.wrapped_cek.size(), capsule.wrapped_cek.begin());
        offset += capsule.wrapped_cek.size();
        envelope.capsules.push_back(std::move(capsule));
    }

    if (offset > bytes.size() || bytes.size() - offset < 4) return std::nullopt;
    const uint32_t content_length = ReadU32Le(bytes, offset);
    offset += 4;
    if (content_length != bytes.size() - offset || content_length < MAIL_ENVELOPE_CONTENT_TAG_BYTES) return std::nullopt;
    envelope.content_ciphertext.assign(bytes.begin() + offset, bytes.end());

    if (!Valid(envelope, expected_active_devices)) return std::nullopt;
    return envelope;
}

} // namespace cybou
