// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PRIVATE_APPLICATION_STORE_H
#define CYBOU_PRIVATE_APPLICATION_STORE_H

#include <cybou/account_id.h>
#include <cybou/keystore.h>
#include <cybou/kv_store.h>

#include <array>
#include <stdexcept>
#include <map>
#include <string>
#include <utility>
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
/**
 * The Application DB was encrypted under different Identity key material,
 * typically before a completed IdentityRotate. The projection is rebuildable:
 * callers may discard it and open a fresh one; device-local-only records
 * (drafts) cannot be decrypted any more.
 */
class PrivateApplicationStoreKeyMismatch final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class PrivateApplicationStore final {
public:
    PrivateApplicationStore(CybouKeyStore& identity, const std::filesystem::path& data_dir);
    ~PrivateApplicationStore();

    PrivateApplicationStore(const PrivateApplicationStore&) = delete;
    PrivateApplicationStore& operator=(const PrivateApplicationStore&) = delete;

    /** False when locked, switched to another key, or storage fails. */
    bool Put(std::string_view name, std::span<const unsigned char> plaintext);
    std::optional<std::vector<unsigned char>> Get(std::string_view name) const;
    bool Has(std::string_view name) const;
    bool Erase(std::string_view name);
    bool IsUnlocked() const;

    /** One change of an atomic batch: a value to store, or nullopt to erase. */
    using Change = std::pair<std::string, std::optional<std::vector<unsigned char>>>;
    /** Applies every change atomically (one synced LevelDB batch) or none. */
    bool WriteBatch(std::span<const Change> changes);

    /**
     * Groups dependent writes (a record and its index, a block and its scan
     * checkpoint) so a crash can never persist only part of them. While a
     * Batch is open, Put/Erase are staged and Get/Has read through them;
     * Commit applies all of them atomically. Destroying an uncommitted Batch
     * discards the staged changes. Batches nest; the outermost one commits.
     */
    class Batch final {
    public:
        explicit Batch(PrivateApplicationStore& store);
        ~Batch();
        Batch(const Batch&) = delete;
        Batch& operator=(const Batch&) = delete;
        bool Commit();

    private:
        PrivateApplicationStore& m_store;
        bool m_outermost{false};
        bool m_done{false};
    };

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
    mutable std::recursive_mutex m_mutex;
    /** Staged changes of the open Batch, keyed by record name. */
    std::optional<std::map<std::string, std::optional<std::vector<unsigned char>>>> m_staged;
    bool m_staged_failed{false};
};

} // namespace cybou

#endif // CYBOU_PRIVATE_APPLICATION_STORE_H
