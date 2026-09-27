// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STORAGE_SERVICE_H
#define CYBOU_STORAGE_SERVICE_H

#include <cybou/keystore.h>
#include <cybou/storage_store.h>

#include <filesystem>
#include <string_view>

namespace cybou {

enum class StorageTransferStatus : uint8_t {
    STORED,
    RETRIEVED,
    INVALID,
    KEY_UNAVAILABLE,
    NOT_FOUND,
    IO_ERROR,
    PROVIDER_ERROR,
    INTEGRITY_ERROR,
};

struct StorageTransferResult {
    StorageTransferStatus status{StorageTransferStatus::INVALID};
    StorageObjectId object_id{};
    StorageChunkId manifest_commitment{};
    explicit operator bool() const
    {
        return status == StorageTransferStatus::STORED || status == StorageTransferStatus::RETRIEVED;
    }
};

/** Local client-side file upload/download against a durable ciphertext store. */
class StorageService final {
public:
    StorageService(std::span<const unsigned char, 32> network_id, AccountId account_id,
        const CybouKeyStore& keystore, StorageObjectStore& store,
        std::filesystem::path private_manifest_dir);

    StorageTransferResult UploadFile(
        const std::filesystem::path& source, std::string_view vault_password);
    StorageTransferResult DownloadFile(const StorageObjectId& object_id,
        const std::filesystem::path& destination, std::string_view vault_password) const;

private:
    std::filesystem::path PrivateManifestPath(const StorageObjectId& object_id) const;

    std::array<unsigned char, 32> m_network_id{};
    AccountId m_account_id;
    const CybouKeyStore& m_keystore;
    StorageObjectStore& m_store;
    std::filesystem::path m_private_manifest_dir;
};

} // namespace cybou

#endif // CYBOU_STORAGE_SERVICE_H
