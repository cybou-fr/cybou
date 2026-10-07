// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Реализация гибридной криптографии Identity и вычисления идентификаторов ключей.

#include <cybou/identity_crypto.h>
#include <cybou/crypto/hkdf_sha256.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/crypto/sha256.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <algorithm>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cybou {
namespace {
using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using KeyCtx = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using MdCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

std::span<const unsigned char> Bytes(const std::string_view value)
{
    return {reinterpret_cast<const unsigned char*>(value.data()), value.size()};
}

// Роль определяет ML-DSA suite: Recovery/PoA требуют более сильный профиль, тогда как
// Authorization/Storage остаются компактнее для частых кандидат-операций и P2P идентификаторов.
const char* Algorithm(IdentityKeyPurpose purpose)
{
    switch (purpose) {
    case IdentityKeyPurpose::RECOVERY_ROOT: return "ML-DSA-65";
    case IdentityKeyPurpose::AUTHORIZATION:
    case IdentityKeyPurpose::STORAGE: return "ML-DSA-44";
    case IdentityKeyPurpose::POA_FINALIZER:
    case IdentityKeyPurpose::NETWORK_ROOT: return "ML-DSA-65";
    }
    return nullptr;
}

// Размеры фиксируются ролью, чтобы сериализация и проверка digest fail-closed
// отвергали несовместимый ключ вместо попытки "угадать" suite по длине входа.
size_t PublicSize(IdentityKeyPurpose purpose)
{
    return purpose == IdentityKeyPurpose::AUTHORIZATION || purpose == IdentityKeyPurpose::STORAGE ? 1312 : 1952;
}

// Тот же принцип для подписей: протокол принимает только точный формат роли.
size_t SignatureSize(IdentityKeyPurpose purpose)
{
    return purpose == IdentityKeyPurpose::AUTHORIZATION || purpose == IdentityKeyPurpose::STORAGE ? 2420 : 3309;
}

bool HasNonzero(std::span<const unsigned char> bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](unsigned char byte) { return byte != 0; });
}

// Раздельные HKDF context не дают одному recovery entropy переиспользоваться
// между Recovery, Authorization, Storage, PoA и offline Network Root.
std::optional<std::string_view> DerivationInfo(
    const IdentityKeyPurpose purpose, const std::string_view component)
{
    const bool ed25519 = component == "ED25519";
    switch (purpose) {
    case IdentityKeyPurpose::RECOVERY_ROOT:
        return ed25519 ? std::optional<std::string_view>{"CYBOU/IDENTITY/ROOT/ED25519"}
                       : std::optional<std::string_view>{"CYBOU/IDENTITY/ROOT/ML-DSA-65"};
    case IdentityKeyPurpose::AUTHORIZATION:
        return ed25519 ? std::optional<std::string_view>{"CYBOU/IDENTITY/AUTH/ED25519"}
                       : std::optional<std::string_view>{"CYBOU/IDENTITY/AUTH/ML-DSA-44"};
    case IdentityKeyPurpose::POA_FINALIZER:
        return ed25519 ? std::optional<std::string_view>{"CYBOU/IDENTITY/POA_FINALIZER/ED25519"}
                       : std::optional<std::string_view>{"CYBOU/IDENTITY/POA_FINALIZER/ML-DSA-65"};
    case IdentityKeyPurpose::STORAGE:
        return ed25519 ? std::optional<std::string_view>{"CYBOU/IDENTITY/STORAGE/ED25519"}
                       : std::optional<std::string_view>{"CYBOU/IDENTITY/STORAGE/ML-DSA-44"};
    case IdentityKeyPurpose::NETWORK_ROOT:
        return ed25519 ? std::optional<std::string_view>{"CYBOU/IDENTITY/NETWORK_ROOT/ED25519"}
                       : std::optional<std::string_view>{"CYBOU/IDENTITY/NETWORK_ROOT/ML-DSA-65"};
    }
    return std::nullopt;
}

std::optional<std::array<unsigned char, 32>> DeriveSeed(
    std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose,
    std::string_view component)
{
    if (!Algorithm(purpose)) return std::nullopt;
    constexpr std::string_view salt{"CYBOU/IDENTITY/HKDF-SHA256"};
    const auto info = DerivationInfo(purpose, component);
    if (!info) return std::nullopt;
    std::array<unsigned char, 32> seed{};
    if (!crypto::HkdfSha256(secret, Bytes(salt), Bytes(*info), seed)) return std::nullopt;
    return seed;
}

Key MakeKey(std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose,
    std::string_view component)
{
    auto seed = DeriveSeed(secret, purpose, component);
    if (!seed) return Key{nullptr, EVP_PKEY_free};
    Key key{nullptr, EVP_PKEY_free};
    if (component == "ED25519") {
        key.reset(EVP_PKEY_new_raw_private_key_ex(nullptr, "ED25519", nullptr, seed->data(), seed->size()));
    } else {
        KeyCtx ctx{EVP_PKEY_CTX_new_from_name(nullptr, Algorithm(purpose), nullptr), EVP_PKEY_CTX_free};
        if (ctx && EVP_PKEY_keygen_init(ctx.get()) == 1) {
            OSSL_PARAM params[] = {
                // Имя параметра оставлено literal'ом: часть OpenSSL 3.x умеет
                // deterministic seed import, но не экспортирует символическую macro.
                OSSL_PARAM_construct_octet_string("seed", seed->data(), seed->size()),
                OSSL_PARAM_construct_end(),
            };
            EVP_PKEY* raw{nullptr};
            if (EVP_PKEY_CTX_set_params(ctx.get(), params) == 1 && EVP_PKEY_keygen(ctx.get(), &raw) == 1) key.reset(raw);
        }
    }
    crypto::CleanseMemory(seed->data(), seed->size());
    return key;
}

std::optional<std::vector<unsigned char>> Sign(Key& key, std::span<const unsigned char> message)
{
    MdCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    if (!ctx || EVP_DigestSignInit_ex(ctx.get(), nullptr, nullptr, nullptr, nullptr, key.get(), nullptr) != 1) return std::nullopt;
    size_t length{0};
    if (EVP_DigestSign(ctx.get(), nullptr, &length, message.data(), message.size()) != 1) return std::nullopt;
    std::vector<unsigned char> signature(length);
    if (EVP_DigestSign(ctx.get(), signature.data(), &length, message.data(), message.size()) != 1) return std::nullopt;
    signature.resize(length);
    return signature;
}

struct KeyCacheLookup {
    uint8_t algo_id{0};
    std::span<const unsigned char> bytes;
};

struct KeyCacheKey {
    uint8_t algo_id{0};
    std::vector<unsigned char> bytes;
};

struct KeyCacheHasher {
    using is_transparent = void;

    size_t operator()(const KeyCacheKey& k) const noexcept {
        return Hash(k.algo_id, k.bytes);
    }
    size_t operator()(const KeyCacheLookup& l) const noexcept {
        return Hash(l.algo_id, l.bytes);
    }

private:
    static size_t Hash(uint8_t algo_id, std::span<const unsigned char> bytes) noexcept {
        uint64_t h = 14695981039346656037ULL ^ algo_id;
        h *= 1099511628211ULL;
        const size_t n = bytes.size();
        size_t i = 0;
        while (i + 8 <= n) {
            uint64_t val = 0;
            std::memcpy(&val, bytes.data() + i, 8);
            h ^= val;
            h *= 1099511628211ULL;
            i += 8;
        }
        while (i < n) {
            h ^= bytes[i];
            h *= 1099511628211ULL;
            ++i;
        }
        return static_cast<size_t>(h);
    }
};

struct KeyCacheEqual {
    using is_transparent = void;

    bool operator()(const KeyCacheKey& a, const KeyCacheKey& b) const noexcept {
        return a.algo_id == b.algo_id && a.bytes.size() == b.bytes.size() &&
            (a.bytes.empty() || std::memcmp(a.bytes.data(), b.bytes.data(), a.bytes.size()) == 0);
    }
    bool operator()(const KeyCacheKey& a, const KeyCacheLookup& b) const noexcept {
        return a.algo_id == b.algo_id && a.bytes.size() == b.bytes.size() &&
            (b.bytes.empty() || std::memcmp(a.bytes.data(), b.bytes.data(), a.bytes.size()) == 0);
    }
    bool operator()(const KeyCacheLookup& a, const KeyCacheKey& b) const noexcept {
        return a.algo_id == b.algo_id && a.bytes.size() == b.bytes.size() &&
            (a.bytes.empty() || std::memcmp(a.bytes.data(), b.bytes.data(), a.bytes.size()) == 0);
    }
};

class PublicKeyCache {
public:
    static constexpr size_t MAX_ENTRIES{1024};

    std::shared_ptr<EVP_PKEY> GetOrCreate(const char* algorithm, std::span<const unsigned char> raw_key)
    {
        if (!algorithm || raw_key.empty()) return nullptr;
        uint8_t algo_id = 0;
        if (std::strcmp(algorithm, "ED25519") == 0) {
            algo_id = 1;
        } else if (std::strcmp(algorithm, "ML-DSA-44") == 0) {
            algo_id = 2;
        } else if (std::strcmp(algorithm, "ML-DSA-65") == 0) {
            algo_id = 3;
        } else {
            return nullptr;
        }

        const KeyCacheLookup lookup{algo_id, raw_key};
        std::unique_lock lock(m_mutex);
        auto it = m_map.find(lookup);
        if (it != m_map.end()) {
            m_list.splice(m_list.begin(), m_list, it->second);
            return it->second->second;
        }

        lock.unlock();

        EVP_PKEY* raw = EVP_PKEY_new_raw_public_key_ex(nullptr, algorithm, nullptr, raw_key.data(), raw_key.size());
        if (!raw) return nullptr;

        std::shared_ptr<EVP_PKEY> pkey{raw, &EVP_PKEY_free};

        lock.lock();
        it = m_map.find(lookup);
        if (it != m_map.end()) {
            m_list.splice(m_list.begin(), m_list, it->second);
            return it->second->second;
        }

        if (m_map.size() >= MAX_ENTRIES) {
            // Кэш ускоряет повторные проверки публичных ключей, но жёсткий LRU-лимит
            // не даёт атакующему раздувать память потоком одноразовых ключей.
            auto lru = --m_list.end();
            m_map.erase(lru->first);
            m_list.pop_back();
        }

        KeyCacheKey key{algo_id, std::vector<unsigned char>(raw_key.begin(), raw_key.end())};
        m_list.push_front({key, pkey});
        m_map.emplace(std::move(key), m_list.begin());

        return pkey;
    }

private:
    std::mutex m_mutex;
    std::list<std::pair<KeyCacheKey, std::shared_ptr<EVP_PKEY>>> m_list;
    std::unordered_map<KeyCacheKey, decltype(m_list)::iterator, KeyCacheHasher, KeyCacheEqual> m_map;
};

bool Verify(const char* algorithm, std::span<const unsigned char> public_key,
    std::span<const unsigned char> signature, std::span<const unsigned char> message)
{
    static PublicKeyCache cache;
    auto key = cache.GetOrCreate(algorithm, public_key);
    if (!key) return false;
    MdCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    return ctx && EVP_DigestVerifyInit_ex(ctx.get(), nullptr, nullptr, nullptr, nullptr, key.get(), nullptr) == 1 &&
        EVP_DigestVerify(ctx.get(), signature.data(), signature.size(), message.data(), message.size()) == 1;
}
} // namespace

std::optional<IdentityHybridPublicKey> DeriveIdentityPublicKey(
    std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose)
{
#if !defined(CYBOU_OFFLINE_PROVISIONING)
    if (purpose == IdentityKeyPurpose::NETWORK_ROOT) return std::nullopt;
#endif
    if (!Algorithm(purpose)) return std::nullopt;
    auto ed = MakeKey(secret, purpose, "ED25519");
    auto pq = MakeKey(secret, purpose, "ML-DSA");
    if (!ed || !pq) return std::nullopt;
    IdentityHybridPublicKey result{.purpose = purpose, .ed25519 = {}, .ml_dsa = {}};
    size_t ed_size{result.ed25519.size()};
    result.ml_dsa.resize(PublicSize(purpose));
    size_t pq_size{result.ml_dsa.size()};
    if (EVP_PKEY_get_raw_public_key(ed.get(), result.ed25519.data(), &ed_size) != 1 || ed_size != result.ed25519.size() ||
        EVP_PKEY_get_raw_public_key(pq.get(), result.ml_dsa.data(), &pq_size) != 1 || pq_size != result.ml_dsa.size()) return std::nullopt;
    return result;
}

std::optional<IdentityHybridSignature> SignIdentityMessage(
    std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose,
    std::span<const unsigned char> message)
{
#if !defined(CYBOU_OFFLINE_PROVISIONING)
    if (purpose == IdentityKeyPurpose::NETWORK_ROOT) return std::nullopt;
#endif
    if (!Algorithm(purpose)) return std::nullopt;
    auto ed = MakeKey(secret, purpose, "ED25519");
    auto pq = MakeKey(secret, purpose, "ML-DSA");
    if (!ed || !pq) return std::nullopt;
    auto ed_sig = Sign(ed, message);
    auto pq_sig = Sign(pq, message);
    if (!ed_sig || !pq_sig || ed_sig->size() != 64 || pq_sig->size() != SignatureSize(purpose)) return std::nullopt;
    IdentityHybridSignature result;
    std::copy(ed_sig->begin(), ed_sig->end(), result.ed25519.begin());
    result.ml_dsa = std::move(*pq_sig);
    return result;
}

struct RetainedIdentityKey::Impl {
    Key ed{nullptr, EVP_PKEY_free};
    Key pq{nullptr, EVP_PKEY_free};
    IdentityHybridPublicKey public_key;
};

RetainedIdentityKey::RetainedIdentityKey(std::unique_ptr<Impl> impl) : m_impl{std::move(impl)} {}
RetainedIdentityKey::~RetainedIdentityKey() = default;

std::unique_ptr<RetainedIdentityKey> RetainedIdentityKey::Derive(
    std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose)
{
    if (purpose == IdentityKeyPurpose::NETWORK_ROOT || !Algorithm(purpose)) return nullptr;
    auto public_key = DeriveIdentityPublicKey(secret, purpose);
    auto impl = std::make_unique<Impl>();
    impl->ed = MakeKey(secret, purpose, "ED25519");
    impl->pq = MakeKey(secret, purpose, "ML-DSA");
    if (!public_key || !impl->ed || !impl->pq) return nullptr;
    impl->public_key = std::move(*public_key);
    return std::unique_ptr<RetainedIdentityKey>{new RetainedIdentityKey{std::move(impl)}};
}

const IdentityHybridPublicKey& RetainedIdentityKey::PublicKey() const { return m_impl->public_key; }

std::optional<IdentityHybridSignature> RetainedIdentityKey::Sign(std::span<const unsigned char> message) const
{
    if (message.empty()) return std::nullopt;
    auto ed_sig = cybou::Sign(m_impl->ed, message);
    auto pq_sig = cybou::Sign(m_impl->pq, message);
    if (!ed_sig || !pq_sig || ed_sig->size() != 64 || pq_sig->size() != SignatureSize(m_impl->public_key.purpose)) return std::nullopt;
    IdentityHybridSignature result;
    std::copy(ed_sig->begin(), ed_sig->end(), result.ed25519.begin());
    result.ml_dsa = std::move(*pq_sig);
    return result;
}

bool VerifyIdentityMessage(const IdentityHybridPublicKey& key,
    const IdentityHybridSignature& signature, std::span<const unsigned char> message)
{
    const char* algorithm = Algorithm(key.purpose);
    if (!algorithm || key.ml_dsa.size() != PublicSize(key.purpose) || signature.ml_dsa.size() != SignatureSize(key.purpose)) return false;
    return Verify("ED25519", key.ed25519, signature.ed25519, message) &&
        Verify(algorithm, key.ml_dsa, signature.ml_dsa, message);
}

std::optional<std::array<unsigned char, 32>> ComputeRecoveryKeyId(
    const IdentityHybridPublicKey& recovery_key)
{
    if (recovery_key.purpose != IdentityKeyPurpose::RECOVERY_ROOT ||
        recovery_key.ml_dsa.size() != PublicSize(IdentityKeyPurpose::RECOVERY_ROOT) ||
        !HasNonzero(recovery_key.ed25519) || !HasNonzero(recovery_key.ml_dsa)) return std::nullopt;

    constexpr std::string_view domain{"CYBOU/RECOVERY-KEY-ID"};
    constexpr std::array<unsigned char, 1> suite{1}; // hybrid root suite
    std::array<unsigned char, 32> id{};
    if (!crypto::ComputeSha256({
        crypto::Sha256Bytes(domain), suite, recovery_key.ed25519, recovery_key.ml_dsa,
    }, id.data())) return std::nullopt;
    return id;
}

std::optional<std::array<unsigned char, 32>> ComputeAuthorizationKeyId(
    const IdentityHybridPublicKey& authorization_key)
{
    if (authorization_key.purpose != IdentityKeyPurpose::AUTHORIZATION ||
        authorization_key.ml_dsa.size() != PublicSize(IdentityKeyPurpose::AUTHORIZATION) ||
        !HasNonzero(authorization_key.ed25519) || !HasNonzero(authorization_key.ml_dsa)) return std::nullopt;
    constexpr std::string_view domain{"CYBOU/IDENTITY-AUTH-KEY-ID"};
    constexpr std::array<unsigned char, 2> suite{2, 1};
    std::array<unsigned char, 32> id{};
    if (!crypto::ComputeSha256({
        crypto::Sha256Bytes(domain), suite, authorization_key.ed25519, authorization_key.ml_dsa,
    }, id.data())) return std::nullopt;
    return id;
}

std::optional<std::array<unsigned char, 32>> ComputePoaFinalizerKeyId(
    const IdentityHybridPublicKey& poa_finalizer_key)
{
    if (poa_finalizer_key.purpose != IdentityKeyPurpose::POA_FINALIZER ||
        poa_finalizer_key.ml_dsa.size() != PublicSize(IdentityKeyPurpose::POA_FINALIZER) ||
        !HasNonzero(poa_finalizer_key.ed25519) || !HasNonzero(poa_finalizer_key.ml_dsa)) {
        return std::nullopt;
    }
    constexpr std::string_view domain{"CYBOU/POA-FINALIZER-KEY-ID"};
    std::array<unsigned char, 32> id{};
    if (!crypto::ComputeSha256({
        crypto::Sha256Bytes(domain), poa_finalizer_key.ed25519, poa_finalizer_key.ml_dsa,
    }, id.data())) return std::nullopt;
    return id;
}

} // namespace cybou
