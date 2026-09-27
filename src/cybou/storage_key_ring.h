// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STORAGE_KEY_RING_H
#define CYBOU_STORAGE_KEY_RING_H

#include <cybou/account_id.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

using StorageMasterKey = std::array<unsigned char, 32>;
inline constexpr uint32_t STORAGE_KEY_RING_MAX_EPOCHS{1024};

/**
 * Local, AccountID-bound Storage Master Key epochs. Persist this key ring in
 * its own password-protected CYBV2 envelope. Secrets are move-only and wiped
 * when the ring is destroyed.
 */
class StorageKeyRing final {
public:
    static std::optional<StorageKeyRing> Create(AccountId account_id);
    static std::optional<StorageKeyRing> LoadFromFile(
        const std::filesystem::path& path, std::string_view password);

    StorageKeyRing(StorageKeyRing&& other) noexcept;
    StorageKeyRing& operator=(StorageKeyRing&& other) noexcept;
    StorageKeyRing(const StorageKeyRing&) = delete;
    StorageKeyRing& operator=(const StorageKeyRing&) = delete;
    ~StorageKeyRing();

    bool SaveNewToFile(const std::filesystem::path& path, std::string_view password) const;
    bool RotateAndSave(const std::filesystem::path& path, std::string_view password);
    AccountId BoundAccount() const { return m_account_id; }
    uint32_t CurrentEpoch() const;
    bool CopyMasterKey(uint32_t epoch, std::span<unsigned char, 32> out) const;

private:
    struct EpochKey {
        uint32_t epoch{0};
        StorageMasterKey key{};
    };

    explicit StorageKeyRing(AccountId account_id);
    void Clear() noexcept;
    std::optional<std::vector<unsigned char>> Serialize() const;
    static std::optional<StorageKeyRing> Parse(std::span<const unsigned char> payload);

    AccountId m_account_id;
    std::vector<EpochKey> m_epochs;
};

} // namespace cybou

#endif // CYBOU_STORAGE_KEY_RING_H
