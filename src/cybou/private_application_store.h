// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PRIVATE_APPLICATION_STORE_H
#define CYBOU_PRIVATE_APPLICATION_STORE_H

#include <cybou/account_id.h>
#include <cybou/keystore.h>
#include <cybou/kv_store.h>

#include <array>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

/** Rebuildable encrypted local projection for one unlocked Identity.
 *
 * Names and values are encrypted or keyed before reaching LevelDB. The store
 * retains no decryption key; access requires the original unlocked key store.
 * Canonical balances, names, and authorization state do not belong here.
 */
class PrivateApplicationStore final {
public:
    PrivateApplicationStore(CybouKeyStore& identity, const std::filesystem::path& data_dir);
    ~PrivateApplicationStore();

    PrivateApplicationStore(const PrivateApplicationStore&) = delete;
    PrivateApplicationStore& operator=(const PrivateApplicationStore&) = delete;

    /** False when locked, switched to another key, or storage fails. */
    bool Put(std::string_view name, std::span<const unsigned char> plaintext);
    std::optional<std::vector<unsigned char>> Get(std::string_view name) const;
    bool Erase(std::string_view name);
    bool IsUnlocked() const;

    const AccountId& Account() const { return m_account; }
    const std::filesystem::path& Path() const { return m_path; }

private:
    std::optional<std::array<unsigned char, 32>> AccessKey() const;
    std::optional<std::string> RecordKey(std::span<const unsigned char, 32> key, std::string_view name) const;
    std::optional<std::vector<unsigned char>> Encrypt(std::span<const unsigned char, 32> key,
        std::string_view name, std::span<const unsigned char> plaintext) const;
    std::optional<std::vector<unsigned char>> Decrypt(std::span<const unsigned char, 32> key,
        std::string_view name, std::span<const unsigned char> encoded) const;

    CybouKeyStore& m_identity;
    AccountId m_account;
    std::filesystem::path m_path;
    std::array<unsigned char, 32> m_key_check{};
    std::unique_ptr<KVStore> m_db;
    mutable std::mutex m_mutex;
};

} // namespace cybou

#endif // CYBOU_PRIVATE_APPLICATION_STORE_H
