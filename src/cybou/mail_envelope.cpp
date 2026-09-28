// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/mail_envelope.h>

#include <algorithm>
#include <limits>

namespace cybou {
namespace {

constexpr size_t CAPSULE_BYTES{8 + 32 + MAIL_ENVELOPE_ENC_BYTES + MAIL_ENVELOPE_WRAPPED_CEK_BYTES};
constexpr size_t FIXED_BYTES{1 + 2 + 2 + 2 + 32 + 8 + 32 + 12 + CAPSULE_BYTES + 4};

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

bool Valid(const MailEnvelopeV1& envelope, std::optional<uint64_t> expected_key_epoch)
{
    const auto& capsule = envelope.recipient_capsule;
    return !IsZero(envelope.mail_id) && !IsZero(capsule.package_commitment) &&
        (!expected_key_epoch || capsule.key_epoch == *expected_key_epoch) &&
        envelope.content_ciphertext.size() >= MAIL_ENVELOPE_CONTENT_TAG_BYTES &&
        envelope.content_ciphertext.size() <= MAIL_ENVELOPE_MAX_BYTES &&
        FIXED_BYTES <= MAIL_ENVELOPE_MAX_BYTES &&
        envelope.content_ciphertext.size() <= MAIL_ENVELOPE_MAX_BYTES - FIXED_BYTES;
}
} // namespace

std::optional<std::vector<unsigned char>> SerializeMailEnvelopeV1(const MailEnvelopeV1& envelope)
{
    if (!Valid(envelope, std::nullopt) || envelope.content_ciphertext.size() > std::numeric_limits<uint32_t>::max())
        return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(FIXED_BYTES + envelope.content_ciphertext.size());
    out.push_back(MAIL_ENVELOPE_VERSION);
    AppendU16Be(out, MAIL_ENVELOPE_KEM_ID);
    AppendU16Be(out, MAIL_ENVELOPE_KDF_ID);
    AppendU16Be(out, MAIL_ENVELOPE_AEAD_ID);
    out.insert(out.end(), envelope.mail_id.begin(), envelope.mail_id.end());
    AppendU64Le(out, envelope.recipient_state_height);
    out.insert(out.end(), envelope.recipient_state_root.begin(), envelope.recipient_state_root.end());
    out.insert(out.end(), envelope.content_nonce.begin(), envelope.content_nonce.end());
    const auto& capsule = envelope.recipient_capsule;
    AppendU64Le(out, capsule.key_epoch);
    out.insert(out.end(), capsule.package_commitment.begin(), capsule.package_commitment.end());
    out.insert(out.end(), capsule.enc.begin(), capsule.enc.end());
    out.insert(out.end(), capsule.wrapped_cek.begin(), capsule.wrapped_cek.end());
    AppendU32Le(out, static_cast<uint32_t>(envelope.content_ciphertext.size()));
    out.insert(out.end(), envelope.content_ciphertext.begin(), envelope.content_ciphertext.end());
    return out;
}

std::optional<MailEnvelopeV1> DeserializeMailEnvelopeV1(
    std::span<const unsigned char> bytes, std::optional<uint64_t> expected_key_epoch)
{
    if (bytes.size() < FIXED_BYTES + MAIL_ENVELOPE_CONTENT_TAG_BYTES || bytes.size() > MAIL_ENVELOPE_MAX_BYTES ||
        bytes[0] != MAIL_ENVELOPE_VERSION || ReadU16Be(bytes, 1) != MAIL_ENVELOPE_KEM_ID ||
        ReadU16Be(bytes, 3) != MAIL_ENVELOPE_KDF_ID || ReadU16Be(bytes, 5) != MAIL_ENVELOPE_AEAD_ID)
        return std::nullopt;
    MailEnvelopeV1 envelope;
    size_t offset{7};
    std::copy_n(bytes.begin() + offset, envelope.mail_id.size(), envelope.mail_id.begin()); offset += envelope.mail_id.size();
    envelope.recipient_state_height = ReadU64Le(bytes, offset); offset += 8;
    std::copy_n(bytes.begin() + offset, envelope.recipient_state_root.size(), envelope.recipient_state_root.begin()); offset += envelope.recipient_state_root.size();
    std::copy_n(bytes.begin() + offset, envelope.content_nonce.size(), envelope.content_nonce.begin()); offset += envelope.content_nonce.size();
    auto& capsule = envelope.recipient_capsule;
    capsule.key_epoch = ReadU64Le(bytes, offset); offset += 8;
    std::copy_n(bytes.begin() + offset, capsule.package_commitment.size(), capsule.package_commitment.begin()); offset += capsule.package_commitment.size();
    std::copy_n(bytes.begin() + offset, capsule.enc.size(), capsule.enc.begin()); offset += capsule.enc.size();
    std::copy_n(bytes.begin() + offset, capsule.wrapped_cek.size(), capsule.wrapped_cek.begin()); offset += capsule.wrapped_cek.size();
    const uint32_t content_length = ReadU32Le(bytes, offset); offset += 4;
    if (content_length != bytes.size() - offset || content_length < MAIL_ENVELOPE_CONTENT_TAG_BYTES) return std::nullopt;
    envelope.content_ciphertext.assign(bytes.begin() + offset, bytes.end());
    if (!Valid(envelope, expected_key_epoch)) return std::nullopt;
    return envelope;
}

} // namespace cybou
