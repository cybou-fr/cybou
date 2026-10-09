// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Реализация зашифрованной локальной Application DB одной Identity.

#include <cybou/private_application_store.h>

#include <cybou/crypto/chacha20_poly1305.h>
#include <cybou/crypto/cleanse.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace cybou {
namespace {

constexpr std::string_view CHECK_NAME{"__application_store_check__"};
constexpr std::string_view CHECK_VALUE{"CYBOU local application store"};
constexpr std::string_view CHECK_DB_KEY{"app/check"};
constexpr std::string_view ROW_DB_PREFIX{"app/row/"};
constexpr std::string_view RECORD_DOMAIN{"CYBOU/LOCAL-APPLICATION-STORE/record/"};
// Key-check жёстко привязывает app.db к текущей Identity, чтобы старая или чужая БД
// не открылась "частично верно" после смены recovery phrase или AccountID.
constexpr std::string_view KEY_CHECK_DOMAIN{"CYBOU/LOCAL-APPLICATION-STORE/key-check"};
// Верхние границы ограничивают rebuildable store и fail-closed отсеивают аномально большие plaintext.
constexpr std::size_t MAX_NAME_BYTES{512};
constexpr std::size_t MAX_VALUE_BYTES{4 * 1024 * 1024};
constexpr char HEX[] = "0123456789abcdef";

struct KeyCleaner {
    std::optional<std::array<unsigned char, 32>>& key;
    ~KeyCleaner() { if (key) crypto::CleanseMemory(key->data(), key->size()); }
};

std::string Hex(const std::span<const unsigned char> bytes)
{
    std::string result(bytes.size() * 2, '\0');
    for (std::size_t i{0}; i < bytes.size(); ++i) {
        result[2 * i] = HEX[bytes[i] >> 4];
        result[2 * i + 1] = HEX[bytes[i] & 0x0f];
    }
    return result;
}

std::span<const unsigned char> Bytes(const std::string_view text)
{
    return {reinterpret_cast<const unsigned char*>(text.data()), text.size()};
}

bool Mac(const std::span<const unsigned char, 32> key, const std::span<const unsigned char> data,
    std::array<unsigned char, 32>& output)
{
    std::size_t written{0};
    return EVP_Q_mac(nullptr, "HMAC", nullptr, "SHA256", nullptr, key.data(), key.size(),
        data.data(), data.size(), output.data(), output.size(), &written) && written == output.size();
}

// Имя записи аутентифицируется вместе с AccountID, чтобы одинаковые application keys разных Identity
// не делили namespace и не могли взаимно принимать ciphertext.
std::vector<unsigned char> AssociatedData(const AccountId& account, const std::string_view name)
{
    std::vector<unsigned char> result;
    result.reserve(RECORD_DOMAIN.size() + AccountId::SIZE + name.size());
    result.insert(result.end(), RECORD_DOMAIN.begin(), RECORD_DOMAIN.end());
    result.insert(result.end(), account.Value().begin(), account.Value().end());
    result.insert(result.end(), name.begin(), name.end());
    return result;
}

} // namespace

std::filesystem::path IdentityDataDirectory(const std::filesystem::path& data_dir, const AccountId& account)
{
    return data_dir / "identities" / Hex(std::span<const unsigned char>{account.Value().begin(), AccountId::SIZE});
}

PrivateApplicationStore::PrivateApplicationStore(CybouKeyStore& identity, const std::filesystem::path& identity_dir,
    std::string_view filename, bool preserve_data_key)
    : m_identity{identity}, m_account{identity.GetAccountId().value_or(AccountId{})}, m_preserve_data_key{preserve_data_key}
{
    if (m_account.IsNull() || identity_dir.empty()) {
        throw std::invalid_argument{"private application store needs an unlocked Identity and data directory"};
    }
    auto key = m_identity.DeriveApplicationStoreKey();
    KeyCleaner cleanse{key};
    if (!key) {
        throw std::runtime_error{"cannot derive private application store key"};
    }
    if (filename != "app.db" && filename != "local.db") throw std::invalid_argument{"invalid application store filename"};
    m_path = identity_dir / filename;
    m_db = std::make_unique<KVStore>(KVStoreOptions{.path = m_path, .cache_bytes = 4 << 20});

    if (m_preserve_data_key) {
        auto data_key = UnwrapKey(*key);
        KeyCleaner wipe_data{data_key};
        if (m_db->Exists(std::string{"app/data-key-present"})) {
            if (!data_key) throw PrivateApplicationStoreKeyMismatch{"local data key cannot be opened; data preserved"};
        } else {
            // Existing app.db keeps its exact encryption key. A new local.db
            // gets a random data key independent of future recovery phrases.
            data_key = *key;
            if (filename == "local.db" && RAND_bytes(data_key->data(), data_key->size()) != 1)
                throw std::runtime_error{"cannot generate local data key"};
            // Authenticate an existing store before adding access material.
            std::vector<unsigned char> old_check;
            if (m_db->Read(std::string{CHECK_DB_KEY}, old_check)) {
                auto decoded = Decrypt(*data_key, CHECK_NAME, old_check);
                if (!decoded || !std::equal(decoded->begin(), decoded->end(), CHECK_VALUE.begin(), CHECK_VALUE.end()))
                    throw PrivateApplicationStoreKeyMismatch{"existing application data cannot be authenticated"};
                crypto::CleanseMemory(decoded->data(), decoded->size());
            }
            if (!WrapKey(*key, *data_key)) throw std::runtime_error{"cannot persist local data key"};
        }
        crypto::CleanseMemory(key->data(), key->size());
        *key = *data_key;
    }
    if (!Mac(*key, Bytes(KEY_CHECK_DOMAIN), m_key_check)) throw std::runtime_error{"cannot authenticate data key"};

    std::vector<unsigned char> check;
    if (m_db->Read(std::string{CHECK_DB_KEY}, check)) {
        auto decoded = Decrypt(*key, CHECK_NAME, check);
        if (!decoded || !std::equal(decoded->begin(), decoded->end(), CHECK_VALUE.begin(), CHECK_VALUE.end())) {
            throw PrivateApplicationStoreKeyMismatch{"private application store belongs to other Identity keys"};
        }
        crypto::CleanseMemory(decoded->data(), decoded->size());
    } else {
        if (m_db->Exists(std::string{CHECK_DB_KEY})) {
            throw std::runtime_error{"corrupt private application store key check"};
        }
        bool has_rows{false};
        m_db->ForEachStringPrefix(std::string{ROW_DB_PREFIX}, std::string{ROW_DB_PREFIX}.size() + 64,
            [&](const std::string&, const std::string&) { has_rows = true; });
        if (has_rows) throw std::runtime_error{"private application store key check is missing"};
        // Пустая база инициализируется key-check записью сразу, чтобы все следующие открытия
        // либо однозначно подтверждали владельца, либо fail-closed останавливались.
        auto encoded = Encrypt(*key, CHECK_NAME, Bytes(CHECK_VALUE));
        if (!encoded) throw std::runtime_error{"cannot encrypt private application store key check"};
        m_db->Write(std::string{CHECK_DB_KEY}, *encoded, true);
    }
    if (m_preserve_data_key) {
        auto active = m_identity.DeriveApplicationStoreKey();
        KeyCleaner wipe_active{active};
        std::array<unsigned char, 32> digest{};
        std::vector<unsigned char> target;
        if (active && Mac(*active, Bytes(KEY_CHECK_DOMAIN), digest) &&
            m_db->Read(std::string{"app/rotation-access"}, target) && target == std::vector<unsigned char>{digest.begin(), digest.end()}) {
            // Only promotion to the prepared phrase retires old access. An
            // interrupted rotation reopened with the old vault keeps both.
            const auto keep = "app/access/" + Hex(digest);
            KVStore::Batch batch;
            m_db->ForEachStringPrefix("app/access/", std::string{"app/access/"}.size() + 64,
                [&](const std::string& name, const std::string&) { if (name != keep) batch.Erase(name); });
            batch.Erase(std::string{"app/rotation-access"});
            m_db->WriteBatch(batch, true);
        }
    }
}

PrivateApplicationStore::~PrivateApplicationStore() = default;

std::optional<std::array<unsigned char, 32>> PrivateApplicationStore::AccessKey() const
{
    if (m_identity.GetAccountId() != std::optional<AccountId>{m_account}) return std::nullopt;
    auto key = m_identity.DeriveApplicationStoreKey();
    if (!key) return std::nullopt;
    if (m_preserve_data_key) {
        auto unwrapped = UnwrapKey(*key);
        crypto::CleanseMemory(key->data(), key->size());
        key = unwrapped;
        if (unwrapped) crypto::CleanseMemory(unwrapped->data(), unwrapped->size());
        if (!key) return std::nullopt;
    }
    std::array<unsigned char, 32> check{};
    const bool valid = Mac(*key, Bytes(KEY_CHECK_DOMAIN), check) &&
        CRYPTO_memcmp(check.data(), m_key_check.data(), check.size()) == 0;
    crypto::CleanseMemory(check.data(), check.size());
    if (!valid) {
        // Несовпадение ключа трактуется как полная блокировка доступа: rebuildable store
        // не должен отдавать ни одной записи, если Identity больше не совпадает.
        crypto::CleanseMemory(key->data(), key->size());
        return std::nullopt;
    }
    return key;
}

std::optional<std::array<unsigned char, 32>> PrivateApplicationStore::UnwrapKey(
    std::span<const unsigned char, 32> wrapping_key) const
{
    std::array<unsigned char, 32> digest{};
    if (!Mac(wrapping_key, Bytes(KEY_CHECK_DOMAIN), digest)) return std::nullopt;
    std::vector<unsigned char> encoded;
    if (!m_db->Read("app/access/" + Hex(digest), encoded)) return std::nullopt;
    auto decoded = Decrypt(wrapping_key, "__application_data_key__", encoded);
    if (!decoded || decoded->size() != 32) return std::nullopt;
    std::array<unsigned char, 32> result;
    std::copy(decoded->begin(), decoded->end(), result.begin());
    crypto::CleanseMemory(decoded->data(), decoded->size());
    return result;
}

bool PrivateApplicationStore::WrapKey(std::span<const unsigned char, 32> wrapping_key,
    std::span<const unsigned char, 32> data_key, bool prepared_rotation)
{
    std::array<unsigned char, 32> digest{};
    if (!Mac(wrapping_key, Bytes(KEY_CHECK_DOMAIN), digest)) return false;
    const auto encoded = Encrypt(wrapping_key, "__application_data_key__", data_key);
    if (!encoded) return false;
    KVStore::Batch batch;
    batch.Write("app/access/" + Hex(digest), *encoded);
    batch.Write(std::string{"app/data-key-present"}, std::vector<unsigned char>{1});
    if (prepared_rotation) batch.Write(std::string{"app/rotation-access"}, std::vector<unsigned char>{digest.begin(), digest.end()});
    try { m_db->WriteBatch(batch, true); return true; } catch (...) { return false; }
}

bool PrivateApplicationStore::PrepareKeyRotation(std::span<const unsigned char, 32> entropy)
{
    std::lock_guard lock{m_mutex};
    if (!m_preserve_data_key) return false;
    auto data_key = AccessKey();
    KeyCleaner wipe_data{data_key};
    auto material = m_identity.CreateIdentityRotationMaterial(entropy);
    CybouKeyStore candidate;
    if (!data_key || !material || !candidate.LoadMaterial(std::move(*material))) return false;
    auto key = candidate.DeriveApplicationStoreKey();
    KeyCleaner wipe_key{key};
    return key && WrapKey(*key, *data_key, true);
}

std::optional<std::string> PrivateApplicationStore::RecordKey(
    const std::span<const unsigned char, 32> key, const std::string_view name) const
{
    if (name.empty() || name.size() > MAX_NAME_BYTES || name == CHECK_NAME) return std::nullopt;
    const auto aad = AssociatedData(m_account, name);
    std::array<unsigned char, 32> digest{};
    if (!Mac(key, aad, digest)) return std::nullopt;
    auto result = std::string{ROW_DB_PREFIX};
    result += Hex(digest);
    crypto::CleanseMemory(digest.data(), digest.size());
    return result;
}

std::optional<std::vector<unsigned char>> PrivateApplicationStore::Encrypt(
    const std::span<const unsigned char, 32> key, const std::string_view name,
    const std::span<const unsigned char> plaintext) const
{
    if (plaintext.size() > MAX_VALUE_BYTES) return std::nullopt;
    constexpr auto NONCE_SIZE = crypto::CHACHA20_POLY1305_NONCE_SIZE;
    constexpr auto TAG_SIZE = crypto::CHACHA20_POLY1305_TAG_SIZE;
    std::vector<unsigned char> encoded(NONCE_SIZE + plaintext.size() + TAG_SIZE);
    if (RAND_bytes(encoded.data(), NONCE_SIZE) != 1) return std::nullopt;
    const auto aad = AssociatedData(m_account, name);
    if (!crypto::ChaCha20Poly1305Encrypt(key,
            std::span<const unsigned char, NONCE_SIZE>{encoded.data(), NONCE_SIZE},
            aad, plaintext, std::span<unsigned char>{encoded}.subspan(NONCE_SIZE))) return std::nullopt;
    return encoded;
}

std::optional<std::vector<unsigned char>> PrivateApplicationStore::Decrypt(
    const std::span<const unsigned char, 32> key, const std::string_view name,
    const std::span<const unsigned char> encoded) const
{
    constexpr auto NONCE_SIZE = crypto::CHACHA20_POLY1305_NONCE_SIZE;
    constexpr auto TAG_SIZE = crypto::CHACHA20_POLY1305_TAG_SIZE;
    if (encoded.size() < NONCE_SIZE + TAG_SIZE ||
        encoded.size() > NONCE_SIZE + MAX_VALUE_BYTES + TAG_SIZE) return std::nullopt;
    std::vector<unsigned char> plaintext(encoded.size() - NONCE_SIZE - TAG_SIZE);
    const auto aad = AssociatedData(m_account, name);
    if (!crypto::ChaCha20Poly1305Decrypt(key,
            std::span<const unsigned char, NONCE_SIZE>{encoded.data(), NONCE_SIZE},
            aad, encoded.subspan(NONCE_SIZE), plaintext)) return std::nullopt;
    return plaintext;
}

bool PrivateApplicationStore::Put(const std::string_view name, const std::span<const unsigned char> plaintext)
{
    std::lock_guard lock{m_mutex};
    if (m_staged) {
        if (name.empty() || name.size() > MAX_NAME_BYTES || name == CHECK_NAME || plaintext.size() > MAX_VALUE_BYTES ||
            !IsUnlocked()) {
            m_staged_failed = true;
            return false;
        }
        (*m_staged)[std::string{name}] = std::vector<unsigned char>{plaintext.begin(), plaintext.end()};
        return true;
    }
    auto key = AccessKey();
    KeyCleaner cleanse{key};
    if (!key) return false;
    const auto record_key = RecordKey(*key, name);
    const auto encoded = record_key ? Encrypt(*key, name, plaintext) : std::nullopt;
    if (!encoded) return false;
    try {
        m_db->Write(*record_key, *encoded, true);
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<std::vector<unsigned char>> PrivateApplicationStore::Get(const std::string_view name) const
{
    std::lock_guard lock{m_mutex};
    if (m_staged) {
        if (const auto staged = m_staged->find(std::string{name}); staged != m_staged->end()) return staged->second;
    }
    auto key = AccessKey();
    KeyCleaner cleanse{key};
    if (!key) return std::nullopt;
    const auto record_key = RecordKey(*key, name);
    if (!record_key) return std::nullopt;
    try {
        std::vector<unsigned char> encoded;
        if (!m_db->Read(*record_key, encoded)) return std::nullopt;
        return Decrypt(*key, name, encoded);
    } catch (...) {
        return std::nullopt;
    }
}

bool PrivateApplicationStore::Has(const std::string_view name) const
{
    std::lock_guard lock{m_mutex};
    if (m_staged) {
        if (const auto staged = m_staged->find(std::string{name}); staged != m_staged->end()) {
            return staged->second.has_value();
        }
    }
    auto key = AccessKey();
    KeyCleaner cleanse{key};
    if (!key) return false;
    const auto record_key = RecordKey(*key, name);
    if (!record_key) return false;
    try { return m_db->Exists(*record_key); }
    catch (...) { return false; }
}

bool PrivateApplicationStore::Erase(const std::string_view name)
{
    std::lock_guard lock{m_mutex};
    if (m_staged) {
        (*m_staged)[std::string{name}] = std::nullopt;
        return true;
    }
    auto key = AccessKey();
    KeyCleaner cleanse{key};
    if (!key) return false;
    const auto record_key = RecordKey(*key, name);
    if (!record_key) return false;
    try {
        m_db->Erase(*record_key, true);
        return true;
    } catch (...) {
        return false;
    }
}

bool PrivateApplicationStore::WriteBatch(const std::span<const Change> changes)
{
    std::lock_guard lock{m_mutex};
    auto key = AccessKey();
    KeyCleaner cleanse{key};
    if (!key) return false;
    KVStore::Batch batch;
    for (const auto& [name, value] : changes) {
        const auto record_key = RecordKey(*key, name);
        if (!record_key) return false;
        if (!value) {
            batch.Erase(*record_key);
            continue;
        }
        const auto encoded = Encrypt(*key, name, *value);
        if (!encoded) return false;
        batch.Write(*record_key, *encoded);
    }
    try {
        m_db->WriteBatch(batch, true);
        return true;
    } catch (...) {
        return false;
    }
}

PrivateApplicationStore::Batch::Batch(PrivateApplicationStore& store) : m_store{store}
{
    store.m_mutex.lock();
    if (!store.m_staged) {
        store.m_staged.emplace();
        store.m_staged_failed = false;
        m_outermost = true;
    } else {
        m_savepoint = *store.m_staged;
        m_savepoint_failed = store.m_staged_failed;
    }
}

void PrivateApplicationStore::Batch::Cleanse(Staged& staged)
{
    for (auto& [_, value] : staged) {
        if (value) crypto::CleanseMemory(value->data(), value->size());
    }
    staged.clear();
}

void PrivateApplicationStore::Batch::RollBack()
{
    Cleanse(*m_store.m_staged);
    *m_store.m_staged = std::move(m_savepoint);
    m_store.m_staged_failed = m_savepoint_failed;
    m_savepoint.clear();
}

PrivateApplicationStore::Batch::~Batch()
{
    if (m_outermost && m_store.m_staged) {
        // Uncommitted: nothing reaches disk. Wipe staged plaintext.
        Cleanse(*m_store.m_staged);
        m_store.m_staged.reset();
    } else if (!m_outermost && !m_done && m_store.m_staged) {
        RollBack(); // abandoned savepoint: only its own changes disappear
    }
    Cleanse(m_savepoint);
    m_store.m_mutex.unlock();
}

bool PrivateApplicationStore::Batch::Commit()
{
    if (m_done) return false;
    m_done = true;
    if (!m_outermost) {
        // Keep the changes in the enclosing batch, unless one of them failed.
        if (!m_store.m_staged_failed) return true;
        RollBack();
        return false;
    }
    auto staged = std::move(*m_store.m_staged);
    const bool failed = m_store.m_staged_failed;
    m_store.m_staged.reset();
    std::vector<Change> changes;
    changes.reserve(staged.size());
    for (auto& [name, value] : staged) changes.emplace_back(name, std::move(value));
    const bool ok = !failed && m_store.WriteBatch(changes);
    for (auto& [_, value] : changes) {
        if (value) crypto::CleanseMemory(value->data(), value->size());
    }
    return ok;
}

bool PrivateApplicationStore::IsUnlocked() const
{
    auto key = AccessKey();
    KeyCleaner cleanse{key};
    return key.has_value();
}

} // namespace cybou
