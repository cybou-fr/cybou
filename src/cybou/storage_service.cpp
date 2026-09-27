// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/storage_service.h>

#include <cybou/crypto/cleanse.h>
#include <cybou/identity_vault.h>

#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace cybou {
namespace {
constexpr std::array<unsigned char, 5> PRIVATE_MANIFEST_MAGIC{'C', 'Y', 'F', 'M', '1'};
constexpr size_t PRIVATE_MANIFEST_SIZE{PRIVATE_MANIFEST_MAGIC.size() + 32 + 32 + 32 + 32 + 4 + 8 + 32};

class TempOutput final {
public:
    ~TempOutput() { Close(); }

    bool Open(const std::filesystem::path& path)
    {
#ifdef _WIN32
        m_handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        return m_handle != INVALID_HANDLE_VALUE;
#else
        m_fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
            S_IRUSR | S_IWUSR);
        return m_fd >= 0;
#endif
    }

    bool Write(std::span<const unsigned char> bytes)
    {
        size_t offset{0};
        while (offset < bytes.size()) {
#ifdef _WIN32
            const DWORD requested = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, MAXDWORD));
            DWORD written{0};
            if (!WriteFile(m_handle, bytes.data() + offset, requested, &written, nullptr) || written == 0) return false;
            offset += written;
#else
            const ssize_t written = write(m_fd, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) return false;
            offset += static_cast<size_t>(written);
#endif
        }
        return true;
    }

    bool SyncAndClose()
    {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE || !FlushFileBuffers(m_handle)) return false;
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
        return true;
#else
        if (m_fd < 0 || fsync(m_fd) != 0) return false;
        const bool closed = close(m_fd) == 0;
        m_fd = -1;
        return closed;
#endif
    }

    void Abort() { Close(); }

private:
    void Close()
    {
#ifdef _WIN32
        if (m_handle != INVALID_HANDLE_VALUE) CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
#else
        if (m_fd >= 0) close(m_fd);
        m_fd = -1;
#endif
    }

#ifdef _WIN32
    HANDLE m_handle{INVALID_HANDLE_VALUE};
#else
    int m_fd{-1};
#endif
};

bool IsZero(std::span<const unsigned char> bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](unsigned char byte) { return byte == 0; });
}

void Put32(std::vector<unsigned char>& out, const uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(static_cast<unsigned char>(value >> shift));
}

void Put64(std::vector<unsigned char>& out, const uint64_t value)
{
    for (unsigned shift = 0; shift < 64; shift += 8) out.push_back(static_cast<unsigned char>(value >> shift));
}

uint32_t Read32(const unsigned char* in)
{
    uint32_t value{0};
    for (unsigned shift = 0; shift < 32; shift += 8) value |= uint32_t{in[shift / 8]} << shift;
    return value;
}

uint64_t Read64(const unsigned char* in)
{
    uint64_t value{0};
    for (unsigned shift = 0; shift < 64; shift += 8) value |= uint64_t{in[shift / 8]} << shift;
    return value;
}

template <size_t N>
std::string ObjectIdHex(const std::array<unsigned char, N>& object_id)
{
    constexpr char HEX[] = "0123456789abcdef";
    std::string result;
    result.reserve(object_id.size() * 2);
    for (const unsigned char byte : object_id) {
        result.push_back(HEX[byte >> 4]);
        result.push_back(HEX[byte & 0x0f]);
    }
    return result;
}

std::vector<unsigned char> EncodePrivateManifest(
    const AccountId& account_id,
    std::span<const unsigned char, 32> network_id,
    const StorageObjectPrivateMetadata& metadata,
    const StorageChunkId& commitment)
{
    std::vector<unsigned char> payload;
    payload.reserve(PRIVATE_MANIFEST_SIZE);
    payload.insert(payload.end(), PRIVATE_MANIFEST_MAGIC.begin(), PRIVATE_MANIFEST_MAGIC.end());
    payload.insert(payload.end(), account_id.Value().begin(), account_id.Value().end());
    payload.insert(payload.end(), network_id.begin(), network_id.end());
    payload.insert(payload.end(), metadata.object_id.begin(), metadata.object_id.end());
    payload.insert(payload.end(), metadata.salt.begin(), metadata.salt.end());
    Put32(payload, metadata.key_epoch);
    Put64(payload, metadata.plaintext_size);
    payload.insert(payload.end(), commitment.begin(), commitment.end());
    return payload;
}

struct DecodedPrivateManifest {
    AccountId account_id;
    std::array<unsigned char, 32> network_id{};
    StorageObjectPrivateMetadata metadata;
    StorageChunkId commitment{};
};

std::optional<DecodedPrivateManifest> DecodePrivateManifest(std::span<const unsigned char> bytes)
{
    if (bytes.size() != PRIVATE_MANIFEST_SIZE ||
        !std::equal(PRIVATE_MANIFEST_MAGIC.begin(), PRIVATE_MANIFEST_MAGIC.end(), bytes.begin())) return std::nullopt;
    size_t offset{PRIVATE_MANIFEST_MAGIC.size()};
    const auto account = AccountId::FromBytes(bytes.subspan(offset, 32));
    if (!account) return std::nullopt;
    DecodedPrivateManifest result;
    result.account_id = *account;
    offset += 32;
    std::copy_n(bytes.begin() + offset, result.network_id.size(), result.network_id.begin());
    offset += result.network_id.size();
    std::copy_n(bytes.begin() + offset, result.metadata.object_id.size(), result.metadata.object_id.begin());
    offset += result.metadata.object_id.size();
    std::copy_n(bytes.begin() + offset, result.metadata.salt.size(), result.metadata.salt.begin());
    offset += result.metadata.salt.size();
    result.metadata.key_epoch = Read32(bytes.data() + offset);
    offset += 4;
    result.metadata.plaintext_size = Read64(bytes.data() + offset);
    offset += 8;
    std::copy_n(bytes.begin() + offset, result.commitment.size(), result.commitment.begin());
    if (IsZero(result.network_id) || result.metadata.object_id == StorageObjectId{} ||
        result.metadata.salt == std::array<unsigned char, 32>{} || IsZero(result.commitment) ||
        result.metadata.plaintext_size > STORAGE_OBJECT_MAX_BYTES) return std::nullopt;
    return result;
}

bool EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec && std::filesystem::is_directory(path, ec) && !ec;
}
} // namespace

StorageService::StorageService(const std::span<const unsigned char, 32> network_id,
    const AccountId account_id, const CybouKeyStore& keystore,
    StorageObjectStore& store, std::filesystem::path private_manifest_dir)
    : m_account_id{account_id}, m_keystore{keystore}, m_store{store},
      m_private_manifest_dir{std::move(private_manifest_dir)}
{
    std::copy(network_id.begin(), network_id.end(), m_network_id.begin());
}

std::filesystem::path StorageService::PrivateManifestPath(const StorageObjectId& object_id) const
{
    return m_private_manifest_dir / (ObjectIdHex(object_id) + ".cyfm.cybv2");
}

StorageTransferResult StorageService::UploadFile(
    const std::filesystem::path& source, const std::string_view vault_password)
{
    if (m_account_id.IsNull() || IsZero(m_network_id) || source.empty() || m_private_manifest_dir.empty()) return {};
    const auto active_account = m_keystore.GetAccountId();
    if (!active_account || *active_account != m_account_id) return {};
    std::error_code ec;
    const uint64_t plaintext_size = std::filesystem::file_size(source, ec);
    if (ec || plaintext_size > STORAGE_OBJECT_MAX_BYTES) return {.status = StorageTransferStatus::IO_ERROR};
    if (!EnsureDirectory(m_private_manifest_dir)) return {.status = StorageTransferStatus::IO_ERROR};

    const auto epoch = m_keystore.GetCurrentStorageKeyEpoch();
    if (!epoch) return {.status = StorageTransferStatus::KEY_UNAVAILABLE};
    StorageMasterKey master_key{};
    if (!m_keystore.CopyStorageMasterKey(*epoch, master_key)) return {.status = StorageTransferStatus::KEY_UNAVAILABLE};
    const auto metadata = CreateStorageObjectMetadata(plaintext_size, *epoch);
    if (!metadata) {
        crypto::CleanseMemory(master_key.data(), master_key.size());
        return {.status = StorageTransferStatus::IO_ERROR};
    }
    auto context = StorageObjectCryptoContext::Create(m_network_id, master_key, *metadata);
    crypto::CleanseMemory(master_key.data(), master_key.size());
    if (!context) return {.status = StorageTransferStatus::INVALID};

    std::ifstream input(source, std::ios::binary);
    if (!input) return {.status = StorageTransferStatus::IO_ERROR};
    std::vector<StorageChunkDescriptor> descriptors;
    descriptors.reserve(context->ChunkCount());
    uint32_t stored_count{0};
    auto abort = [&] { (void)m_store.AbortUncommittedObject(metadata->object_id, stored_count); };
    for (uint32_t index = 0; index < context->ChunkCount(); ++index) {
        const auto expected_size = context->ExpectedPlaintextChunkSize(index);
        if (!expected_size) {
            abort();
            return {.status = StorageTransferStatus::INVALID};
        }
        std::vector<unsigned char> plaintext(*expected_size);
        if (!plaintext.empty()) {
            input.read(reinterpret_cast<char*>(plaintext.data()), static_cast<std::streamsize>(plaintext.size()));
            if (input.gcount() != static_cast<std::streamsize>(plaintext.size())) {
                crypto::CleanseMemory(plaintext.data(), plaintext.size());
                abort();
                return {.status = StorageTransferStatus::IO_ERROR};
            }
        }
        auto chunk = context->EncryptChunk(index, plaintext);
        crypto::CleanseMemory(plaintext.data(), plaintext.size());
        if (!chunk) {
            abort();
            return {.status = StorageTransferStatus::INVALID};
        }
        const auto write = m_store.PutChunk(metadata->object_id, *chunk);
        if (!write) {
            abort();
            return {.status = StorageTransferStatus::PROVIDER_ERROR};
        }
        ++stored_count;
        descriptors.push_back({.chunk_id = chunk->chunk_id,
            .ciphertext_size = static_cast<uint32_t>(chunk->ciphertext_and_tag.size())});
    }
    char trailing_byte{};
    input.read(&trailing_byte, 1);
    if (input.gcount() != 0 || !input.eof()) {
        abort();
        return {.status = StorageTransferStatus::IO_ERROR};
    }

    const auto manifest = BuildStoragePublicManifestFromDescriptors(m_network_id, metadata->object_id, descriptors);
    if (!manifest) {
        abort();
        return {.status = StorageTransferStatus::INVALID};
    }
    auto private_payload = EncodePrivateManifest(m_account_id, m_network_id, *metadata, manifest->commitment);
    const auto private_manifest_path = PrivateManifestPath(metadata->object_id);
    const bool saved = SaveNewIdentityVault(private_manifest_path, vault_password, private_payload);
    crypto::CleanseMemory(private_payload.data(), private_payload.size());
    if (!saved) {
        abort();
        return {.status = StorageTransferStatus::IO_ERROR};
    }
    const auto committed = m_store.CommitManifest(*manifest);
    if (!committed) {
        abort();
        std::filesystem::remove(private_manifest_path, ec);
        return {.status = StorageTransferStatus::PROVIDER_ERROR};
    }
    return {.status = StorageTransferStatus::STORED,
        .object_id = metadata->object_id, .manifest_commitment = manifest->commitment};
}

StorageTransferResult StorageService::DownloadFile(const StorageObjectId& object_id,
    const std::filesystem::path& destination, const std::string_view vault_password) const
{
    if (m_account_id.IsNull() || IsZero(m_network_id) || object_id == StorageObjectId{} ||
        destination.empty() || m_private_manifest_dir.empty()) return {};
    const auto active_account = m_keystore.GetAccountId();
    if (!active_account || *active_account != m_account_id) return {};
    const auto manifest_path = PrivateManifestPath(object_id);
    auto payload = LoadIdentityVault(manifest_path, vault_password);
    if (!payload) return {.status = StorageTransferStatus::NOT_FOUND};
    auto private_manifest = DecodePrivateManifest(*payload);
    crypto::CleanseMemory(payload->data(), payload->size());
    if (!private_manifest || private_manifest->account_id != m_account_id ||
        private_manifest->network_id != m_network_id || private_manifest->metadata.object_id != object_id) {
        return {.status = StorageTransferStatus::INTEGRITY_ERROR};
    }
    const auto manifest = m_store.GetManifest(object_id);
    if (!manifest || manifest->commitment != private_manifest->commitment ||
        !VerifyStoragePublicManifest(m_network_id, *manifest)) return {.status = StorageTransferStatus::INTEGRITY_ERROR};

    StorageMasterKey master_key{};
    if (!m_keystore.CopyStorageMasterKey(private_manifest->metadata.key_epoch, master_key)) {
        return {.status = StorageTransferStatus::KEY_UNAVAILABLE};
    }
    auto context = StorageObjectCryptoContext::Create(m_network_id, master_key, private_manifest->metadata);
    crypto::CleanseMemory(master_key.data(), master_key.size());
    if (!context || context->ChunkCount() != manifest->chunk_count) return {.status = StorageTransferStatus::INTEGRITY_ERROR};

    std::error_code ec;
    if (std::filesystem::exists(destination, ec) || ec) return {.status = StorageTransferStatus::IO_ERROR};
    if (!EnsureDirectory(destination.parent_path().empty() ? std::filesystem::current_path() : destination.parent_path())) {
        return {.status = StorageTransferStatus::IO_ERROR};
    }
    std::array<unsigned char, 12> random_suffix{};
    if (RAND_bytes(random_suffix.data(), random_suffix.size()) != 1) return {.status = StorageTransferStatus::IO_ERROR};
    auto temporary = destination;
    temporary += ".cybou-tmp-";
    temporary += ObjectIdHex(random_suffix);
    TempOutput output;
    if (!output.Open(temporary)) return {.status = StorageTransferStatus::IO_ERROR};

    uint64_t written{0};
    for (uint32_t index = 0; index < manifest->chunk_count; ++index) {
        auto chunk = m_store.GetChunk(object_id, index);
        if (!chunk || chunk->chunk_id != manifest->chunks[index].chunk_id ||
            chunk->ciphertext_and_tag.size() != manifest->chunks[index].ciphertext_size) {
            output.Abort();
            std::filesystem::remove(temporary, ec);
            return {.status = StorageTransferStatus::INTEGRITY_ERROR};
        }
        auto plaintext = context->DecryptChunk(*chunk);
        if (!plaintext) {
            output.Abort();
            std::filesystem::remove(temporary, ec);
            return {.status = StorageTransferStatus::INTEGRITY_ERROR};
        }
        const bool write_ok = output.Write(*plaintext);
        written += plaintext->size();
        crypto::CleanseMemory(plaintext->data(), plaintext->size());
        if (!write_ok) {
            output.Abort();
            std::filesystem::remove(temporary, ec);
            return {.status = StorageTransferStatus::IO_ERROR};
        }
    }
    if (written != private_manifest->metadata.plaintext_size) {
        output.Abort();
        std::filesystem::remove(temporary, ec);
        return {.status = StorageTransferStatus::INTEGRITY_ERROR};
    }
    if (!output.SyncAndClose()) {
        output.Abort();
        std::filesystem::remove(temporary, ec);
        return {.status = StorageTransferStatus::IO_ERROR};
    }
    std::filesystem::create_hard_link(temporary, destination, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return {.status = StorageTransferStatus::IO_ERROR};
    }
    std::filesystem::remove(temporary, ec);
    return {.status = StorageTransferStatus::RETRIEVED,
        .object_id = object_id, .manifest_commitment = manifest->commitment};
}

} // namespace cybou
