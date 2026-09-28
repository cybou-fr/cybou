// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/root_publication.h>

#include <cybou/crypto/chacha20_poly1305.h>
#include <cybou/crypto/hkdf_sha256.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace cybou {
namespace {

constexpr std::string_view CAPSULE_KEY_DOMAIN{"CYBOU/ROOT-CAPSULE-KEY/v1"};
constexpr std::string_view CAPSULE_AAD_DOMAIN{"CYBOU/ROOT-CAPSULE-AAD/v1"};

class CleanseOnExit final
{
public:
    explicit CleanseOnExit(std::span<unsigned char> bytes) : m_bytes{bytes} {}
    ~CleanseOnExit() { if (!m_bytes.empty()) OPENSSL_cleanse(m_bytes.data(), m_bytes.size()); }
    CleanseOnExit(const CleanseOnExit&) = delete;
    CleanseOnExit& operator=(const CleanseOnExit&) = delete;

private:
    std::span<unsigned char> m_bytes;
};

bool IsZero(const std::span<const unsigned char> bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](const auto byte) { return byte == 0; });
}

void AppendU64Be(std::vector<unsigned char>& bytes, const std::uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8) bytes.push_back(static_cast<unsigned char>(value >> shift));
}

std::vector<unsigned char> CapsuleKeyInfo(
    const ChunkId& root_chunk_id,
    const std::span<const unsigned char, 32> sender_account_id,
    const std::uint64_t sender_nonce,
    const std::uint64_t sender_key_epoch,
    const std::uint64_t recipient_key_epoch)
{
    std::vector<unsigned char> info(CAPSULE_KEY_DOMAIN.begin(), CAPSULE_KEY_DOMAIN.end());
    info.insert(info.end(), root_chunk_id.begin(), root_chunk_id.end());
    info.insert(info.end(), sender_account_id.begin(), sender_account_id.end());
    AppendU64Be(info, sender_nonce);
    AppendU64Be(info, sender_key_epoch);
    AppendU64Be(info, recipient_key_epoch);
    return info;
}

std::vector<unsigned char> CapsuleAad(
    const std::span<const unsigned char, 32> network_id,
    const ChunkId& root_chunk_id,
    const std::span<const unsigned char, 32> sender_account_id,
    const std::uint64_t sender_nonce,
    const std::uint64_t sender_key_epoch,
    const std::uint64_t recipient_key_epoch)
{
    std::vector<unsigned char> aad(CAPSULE_AAD_DOMAIN.begin(), CAPSULE_AAD_DOMAIN.end());
    aad.insert(aad.end(), network_id.begin(), network_id.end());
    aad.insert(aad.end(), root_chunk_id.begin(), root_chunk_id.end());
    aad.insert(aad.end(), sender_account_id.begin(), sender_account_id.end());
    AppendU64Be(aad, sender_nonce);
    AppendU64Be(aad, sender_key_epoch);
    AppendU64Be(aad, recipient_key_epoch);
    return aad;
}

} // namespace

std::optional<RootRecipientCapsule> CreateRootRecipientCapsule(
    const std::span<const unsigned char, 32> network_id,
    const std::span<const unsigned char, 32> sender_account_id,
    const std::uint64_t sender_nonce,
    const std::uint64_t sender_key_epoch,
    const ChunkId& root_chunk_id,
    const std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> recipient_public_key,
    const std::uint64_t recipient_key_epoch,
    const std::span<const unsigned char, 32> graph_content_key)
{
    if (IsZero(root_chunk_id) || IsZero(recipient_public_key) || IsZero(graph_content_key)) return std::nullopt;
    const auto encapsulated = EncapsulateXWing(recipient_public_key);
    if (!encapsulated) return std::nullopt;

    RootRecipientCapsule capsule;
    capsule.key_epoch = recipient_key_epoch;
    capsule.encapsulation = encapsulated->ciphertext;
    std::array<unsigned char, 32> wrapping_key{};
    CleanseOnExit cleanse_key{wrapping_key};
    const auto info = CapsuleKeyInfo(root_chunk_id, sender_account_id, sender_nonce,
        sender_key_epoch, recipient_key_epoch);
    const auto aad = CapsuleAad(network_id, root_chunk_id, sender_account_id,
        sender_nonce, sender_key_epoch, recipient_key_epoch);
    if (!crypto::HkdfSha256(encapsulated->shared_secret, network_id, info, wrapping_key)) return std::nullopt;

    std::array<unsigned char, ROOT_CAPSULE_NONCE_BYTES> nonce{};
    if (RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1) return std::nullopt;
    std::array<unsigned char, crypto::CHACHA20_POLY1305_TAG_SIZE + 32> ciphertext_and_tag{};
    if (!crypto::ChaCha20Poly1305Encrypt(wrapping_key, nonce, aad, graph_content_key, ciphertext_and_tag)) return std::nullopt;
    std::copy(nonce.begin(), nonce.end(), capsule.wrapped_content_key.begin());
    std::copy(ciphertext_and_tag.begin(), ciphertext_and_tag.end(), capsule.wrapped_content_key.begin() + nonce.size());
    return capsule;
}

std::optional<GraphContentKey> OpenRootRecipientCapsule(
    const std::span<const unsigned char, 32> network_id,
    const std::span<const unsigned char, 32> sender_account_id,
    const std::uint64_t sender_nonce,
    const std::uint64_t sender_key_epoch,
    const ChunkId& root_chunk_id,
    const RootRecipientCapsule& capsule,
    const std::span<const unsigned char, XWING_SEED_SIZE> recipient_seed)
{
    if (capsule.kem_profile != IDENTITY_KEM_PROFILE_XWING || IsZero(root_chunk_id)) return std::nullopt;
    auto shared_secret = DecapsulateXWing(recipient_seed, capsule.encapsulation);
    if (!shared_secret) return std::nullopt;
    CleanseOnExit cleanse_shared_secret{*shared_secret};

    std::array<unsigned char, 32> wrapping_key{};
    CleanseOnExit cleanse_key{wrapping_key};
    const auto info = CapsuleKeyInfo(root_chunk_id, sender_account_id, sender_nonce,
        sender_key_epoch, capsule.key_epoch);
    const auto aad = CapsuleAad(network_id, root_chunk_id, sender_account_id,
        sender_nonce, sender_key_epoch, capsule.key_epoch);
    if (!crypto::HkdfSha256(*shared_secret, network_id, info, wrapping_key)) return std::nullopt;

    const auto nonce = std::span<const unsigned char, ROOT_CAPSULE_NONCE_BYTES>{
        capsule.wrapped_content_key.data(), ROOT_CAPSULE_NONCE_BYTES};
    const auto ciphertext_and_tag = std::span<const unsigned char>{capsule.wrapped_content_key}.subspan(ROOT_CAPSULE_NONCE_BYTES);
    GraphContentKey content_key{};
    CleanseOnExit cleanse_content_key{content_key};
    if (!crypto::ChaCha20Poly1305Decrypt(wrapping_key, nonce, aad, ciphertext_and_tag, content_key) || IsZero(content_key)) {
        return std::nullopt;
    }
    return content_key;
}

} // namespace cybou
