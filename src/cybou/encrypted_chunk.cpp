// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/encrypted_chunk.h>

#include <cybou/crypto/chacha20_poly1305.h>
#include <cybou/crypto/hkdf_sha256.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>

namespace cybou {
namespace {

constexpr std::array<unsigned char, 4> MAGIC{'C', 'Y', 'C', 'H'};
constexpr unsigned char VERSION{1};
constexpr std::size_t MAGIC_OFFSET{0};
constexpr std::size_t VERSION_OFFSET{4};
constexpr std::size_t SALT_OFFSET{5};
constexpr std::size_t NONCE_OFFSET{SALT_OFFSET + 32};
constexpr std::size_t CIPHERTEXT_OFFSET{NONCE_OFFSET + crypto::CHACHA20_POLY1305_NONCE_SIZE};
constexpr std::array<std::size_t, 6> PAD_BUCKETS{1024, 4096, 16 * 1024, 64 * 1024, 256 * 1024, 512 * 1024};
constexpr std::string_view KEY_INFO_DOMAIN{"CYBOU/CHUNK-KEY/v1"};
constexpr std::string_view AAD_DOMAIN{"CYBOU/CHUNK-AAD/v1"};

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

std::optional<std::size_t> BucketFor(const std::size_t frame_bytes)
{
    const auto bucket = std::find_if(PAD_BUCKETS.begin(), PAD_BUCKETS.end(), [frame_bytes](const auto size) {
        return frame_bytes <= size;
    });
    if (bucket == PAD_BUCKETS.end()) return std::nullopt;
    return *bucket;
}

bool IsPadBucket(const std::size_t size)
{
    return std::find(PAD_BUCKETS.begin(), PAD_BUCKETS.end(), size) != PAD_BUCKETS.end();
}

std::vector<unsigned char> MakeKeyInfo(
    const std::span<const unsigned char, 32> network_id)
{
    std::vector<unsigned char> info(KEY_INFO_DOMAIN.begin(), KEY_INFO_DOMAIN.end());
    info.insert(info.end(), network_id.begin(), network_id.end());
    return info;
}

std::vector<unsigned char> MakeAad(
    const std::span<const unsigned char> header,
    const std::span<const unsigned char, 32> network_id)
{
    std::vector<unsigned char> aad(AAD_DOMAIN.begin(), AAD_DOMAIN.end());
    aad.insert(aad.end(), header.begin(), header.end());
    aad.insert(aad.end(), network_id.begin(), network_id.end());
    return aad;
}

bool DeriveChunkKey(
    const std::span<const unsigned char, 32> graph_key,
    const std::span<const unsigned char, 32> salt,
    const std::span<const unsigned char, 32> network_id,
    std::span<unsigned char, 32> output)
{
    const auto info = MakeKeyInfo(network_id);
    return crypto::HkdfSha256(graph_key, salt, info, output);
}

bool HasValidHeader(const std::span<const unsigned char> stored_bytes)
{
    return stored_bytes.size() >= CIPHERTEXT_OFFSET + crypto::CHACHA20_POLY1305_TAG_SIZE &&
        stored_bytes.size() <= ENCRYPTED_CHUNK_MAX_STORED_BYTES &&
        std::equal(MAGIC.begin(), MAGIC.end(), stored_bytes.begin() + MAGIC_OFFSET) &&
        stored_bytes[VERSION_OFFSET] == VERSION;
}

std::vector<unsigned char> MakeFrame(const std::span<const unsigned char> encoded, const std::size_t bucket)
{
    std::vector<unsigned char> frame(bucket);
    const auto length = static_cast<std::uint32_t>(encoded.size());
    frame[0] = static_cast<unsigned char>(length >> 24);
    frame[1] = static_cast<unsigned char>(length >> 16);
    frame[2] = static_cast<unsigned char>(length >> 8);
    frame[3] = static_cast<unsigned char>(length);
    std::copy(encoded.begin(), encoded.end(), frame.begin() + 4);
    auto* const padding = frame.data() + 4 + encoded.size();
    const auto padding_size = frame.size() - 4 - encoded.size();
    if (padding_size != 0 && RAND_bytes(padding, static_cast<int>(padding_size)) != 1) {
        OPENSSL_cleanse(frame.data(), frame.size());
        return {};
    }
    return frame;
}

std::optional<std::size_t> ReadFrameLength(const std::span<const unsigned char> frame)
{
    if (frame.size() < 4) return std::nullopt;
    const auto length = (std::uint32_t{frame[0]} << 24) |
        (std::uint32_t{frame[1]} << 16) |
        (std::uint32_t{frame[2]} << 8) |
        std::uint32_t{frame[3]};
    if (length > cbor_profile::MAX_ENCODED_BYTES || length > frame.size() - 4) return std::nullopt;
    const auto expected_bucket = BucketFor(4 + static_cast<std::size_t>(length));
    if (!expected_bucket || *expected_bucket != frame.size()) return std::nullopt;
    return static_cast<std::size_t>(length);
}

} // namespace

std::optional<GraphContentKey> GenerateGraphContentKey()
{
    GraphContentKey key{};
    if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1 || IsZero(key)) {
        OPENSSL_cleanse(key.data(), key.size());
        return std::nullopt;
    }
    return key;
}

std::optional<EncryptedChunk> EncryptGraphChunk(
    const std::span<const unsigned char, 32> network_id,
    const std::span<const unsigned char, 32> graph_key,
    const CborValue& node)
{
    std::vector<std::uint8_t> encoded;
    try {
        encoded = EncodeCanonicalCbor(node);
    } catch (...) {
        return std::nullopt;
    }
    CleanseOnExit cleanse_encoded{std::span<unsigned char>{reinterpret_cast<unsigned char*>(encoded.data()), encoded.size()}};
    if (encoded.size() > cbor_profile::MAX_ENCODED_BYTES || encoded.size() > std::numeric_limits<std::uint32_t>::max()) return std::nullopt;
    const auto bucket = BucketFor(4 + encoded.size());
    if (!bucket) return std::nullopt;

    std::array<unsigned char, 32> salt{};
    std::array<unsigned char, crypto::CHACHA20_POLY1305_NONCE_SIZE> nonce{};
    std::array<unsigned char, crypto::CHACHA20_POLY1305_KEY_SIZE> chunk_key{};
    CleanseOnExit cleanse_key{chunk_key};
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1 ||
        RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1 ||
        !DeriveChunkKey(graph_key, salt, network_id, chunk_key)) {
        return std::nullopt;
    }

    std::array<unsigned char, ENCRYPTED_CHUNK_HEADER_SIZE> header{};
    std::copy(MAGIC.begin(), MAGIC.end(), header.begin() + MAGIC_OFFSET);
    header[VERSION_OFFSET] = VERSION;
    std::copy(salt.begin(), salt.end(), header.begin() + SALT_OFFSET);
    std::copy(nonce.begin(), nonce.end(), header.begin() + NONCE_OFFSET);
    const auto aad = MakeAad(header, network_id);

    auto frame = MakeFrame(encoded, *bucket);
    if (frame.empty()) return std::nullopt;
    CleanseOnExit cleanse_frame{frame};

    EncryptedChunk result;
    result.stored_bytes.reserve(header.size() + frame.size() + crypto::CHACHA20_POLY1305_TAG_SIZE);
    result.stored_bytes.insert(result.stored_bytes.end(), header.begin(), header.end());
    result.stored_bytes.resize(header.size() + frame.size() + crypto::CHACHA20_POLY1305_TAG_SIZE);
    if (!crypto::ChaCha20Poly1305Encrypt(
            chunk_key, nonce, aad, frame,
            std::span<unsigned char>{result.stored_bytes}.subspan(header.size()))) {
        return std::nullopt;
    }
    result.id = ComputeChunkId(result.stored_bytes);
    return result;
}

std::optional<CborValue> DecryptGraphChunk(
    const std::span<const unsigned char, 32> network_id,
    const std::span<const unsigned char, 32> graph_key,
    const ChunkId& expected_id,
    const std::span<const unsigned char> stored_bytes)
{
    if (!HasValidHeader(stored_bytes)) return std::nullopt;
    const auto actual_id = ComputeChunkId(stored_bytes);
    if (actual_id != expected_id) return std::nullopt;

    const auto encrypted_frame = stored_bytes.subspan(CIPHERTEXT_OFFSET);
    const auto frame_size = encrypted_frame.size() - crypto::CHACHA20_POLY1305_TAG_SIZE;
    if (!IsPadBucket(frame_size)) return std::nullopt;

    const auto salt = std::span<const unsigned char, 32>{stored_bytes.subspan(SALT_OFFSET, 32)};
    const auto nonce = std::span<const unsigned char, crypto::CHACHA20_POLY1305_NONCE_SIZE>{stored_bytes.subspan(NONCE_OFFSET, crypto::CHACHA20_POLY1305_NONCE_SIZE)};
    std::array<unsigned char, crypto::CHACHA20_POLY1305_KEY_SIZE> chunk_key{};
    CleanseOnExit cleanse_key{chunk_key};
    if (!DeriveChunkKey(graph_key, salt, network_id, chunk_key)) return std::nullopt;

    const auto header = stored_bytes.first(ENCRYPTED_CHUNK_HEADER_SIZE);
    const auto aad = MakeAad(header, network_id);
    std::vector<unsigned char> frame(frame_size);
    CleanseOnExit cleanse_frame{frame};
    if (!crypto::ChaCha20Poly1305Decrypt(chunk_key, nonce, aad, encrypted_frame, frame)) return std::nullopt;

    const auto encoded_size = ReadFrameLength(frame);
    if (!encoded_size) return std::nullopt;
    try {
        const auto encoded = std::span<const unsigned char>{frame}.subspan(4, *encoded_size);
        return DecodeCanonicalCbor(encoded);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace cybou
