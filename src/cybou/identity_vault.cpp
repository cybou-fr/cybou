// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_vault.h>
#include <cybou/crypto/cleanse.h>

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace cybou {
namespace {
constexpr size_t HEADER_SIZE{61};
constexpr size_t SALT_OFFSET{17};
constexpr size_t WRAP_NONCE_OFFSET{33};
constexpr size_t PAYLOAD_NONCE_OFFSET{45};
constexpr size_t LENGTH_OFFSET{57};
constexpr size_t WRAPPED_DEK_SIZE{48};
constexpr size_t TAG_SIZE{16};
constexpr uint32_t MEMCOST{65536};
constexpr uint32_t ITERATIONS{3};
constexpr uint32_t LANES{1};
constexpr uint32_t MAX_PAYLOAD{65536};
constexpr size_t MAX_ENVELOPE{HEADER_SIZE + WRAPPED_DEK_SIZE + MAX_PAYLOAD + TAG_SIZE};
using CipherCtx = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
using Kdf = std::unique_ptr<EVP_KDF, decltype(&EVP_KDF_free)>;
using KdfCtx = std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)>;

class VaultFileLock final {
public:
    explicit VaultFileLock(const std::filesystem::path& path)
    {
        auto lock_path = path;
        lock_path += ".lock";
#ifdef _WIN32
        m_handle = CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (m_handle == INVALID_HANDLE_VALUE) return;
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(m_handle, &info) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
            GetFileType(m_handle) != FILE_TYPE_DISK) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
            return;
        }
        OVERLAPPED overlapped{};
        if (!LockFileEx(m_handle, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped)) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
            return;
        }
        m_overlapped = overlapped;
#else
        m_fd = open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
        if (m_fd < 0) return;
        struct stat info{};
        if (fstat(m_fd, &info) != 0 || !S_ISREG(info.st_mode) || flock(m_fd, LOCK_EX) != 0) {
            close(m_fd);
            m_fd = -1;
        }
#endif
    }

    ~VaultFileLock()
    {
#ifdef _WIN32
        if (m_handle != INVALID_HANDLE_VALUE) {
            UnlockFileEx(m_handle, 0, 1, 0, &m_overlapped);
            CloseHandle(m_handle);
        }
#else
        if (m_fd >= 0) {
            flock(m_fd, LOCK_UN);
            close(m_fd);
        }
#endif
    }

    explicit operator bool() const
    {
#ifdef _WIN32
        return m_handle != INVALID_HANDLE_VALUE;
#else
        return m_fd >= 0;
#endif
    }

private:
#ifdef _WIN32
    HANDLE m_handle{INVALID_HANDLE_VALUE};
    OVERLAPPED m_overlapped{};
#else
    int m_fd{-1};
#endif
};

void Store32(unsigned char* out, uint32_t value)
{
    for (int i{0}; i < 4; ++i) out[i] = static_cast<unsigned char>(value >> (8 * i));
}

uint32_t Load32(const unsigned char* in)
{
    uint32_t value{0};
    for (int i{0}; i < 4; ++i) value |= uint32_t{in[i]} << (8 * i);
    return value;
}

std::optional<std::filesystem::path> TemporaryPath(const std::filesystem::path& path)
{
    std::array<unsigned char, 16> nonce{};
    if (RAND_bytes(nonce.data(), nonce.size()) != 1) return std::nullopt;
    constexpr char digits[] = "0123456789abcdef";
    std::string suffix{".tmp."};
    for (const unsigned char byte : nonce) {
        suffix.push_back(digits[byte >> 4]);
        suffix.push_back(digits[byte & 15]);
    }
    auto temp = path;
    temp += suffix;
    return temp;
}

bool WriteNewFile(const std::filesystem::path& path, std::span<const unsigned char> bytes)
{
#ifdef _WIN32
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written{0};
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
        written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) DeleteFileW(path.c_str());
    return ok;
#else
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    size_t offset{0};
    while (offset < bytes.size()) {
        const ssize_t written = write(fd, bytes.data() + offset, bytes.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) break;
        offset += static_cast<size_t>(written);
    }
    const bool ok = offset == bytes.size() && fsync(fd) == 0;
    if (close(fd) != 0 || !ok) {
        unlink(path.c_str());
        return false;
    }
    return ok;
#endif
}

bool PublishNewFile(const std::filesystem::path& temp, const std::filesystem::path& target)
{
#ifdef _WIN32
    return MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (link(temp.c_str(), target.c_str()) != 0) return false;
    if (unlink(temp.c_str()) != 0) return false;
    const auto parent = target.parent_path().empty() ? std::filesystem::path{"."} : target.parent_path();
    const int dirfd = open(parent.c_str(), O_RDONLY);
    if (dirfd < 0) return false;
    const bool ok = fsync(dirfd) == 0;
    close(dirfd);
    return ok;
#endif
}

std::optional<std::vector<unsigned char>> ReadBoundedFile(const std::filesystem::path& path)
{
#ifdef _WIN32
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    BY_HANDLE_FILE_INFORMATION info{};
    LARGE_INTEGER size{};
    const bool valid = GetFileInformationByHandle(file, &info) &&
        !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
        GetFileType(file) == FILE_TYPE_DISK && GetFileSizeEx(file, &size) &&
        size.QuadPart > 0 && size.QuadPart <= static_cast<LONGLONG>(MAX_ENVELOPE);
    if (!valid) { CloseHandle(file); return std::nullopt; }
    std::vector<unsigned char> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read{0};
    const bool ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) &&
        read == bytes.size();
    CloseHandle(file);
    if (!ok) return std::nullopt;
    return bytes;
#else
    int flags = O_RDONLY;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    const int fd = open(path.c_str(), flags);
    if (fd < 0) return std::nullopt;
    struct stat st{};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
        st.st_size > static_cast<off_t>(MAX_ENVELOPE)) {
        close(fd);
        return std::nullopt;
    }
    std::vector<unsigned char> bytes(static_cast<size_t>(st.st_size));
    size_t offset{0};
    while (offset < bytes.size()) {
        const ssize_t got = read(fd, bytes.data() + offset, bytes.size() - offset);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) break;
        offset += static_cast<size_t>(got);
    }
    close(fd);
    if (offset != bytes.size()) return std::nullopt;
    return bytes;
#endif
}

bool DeriveKek(std::string_view password, const unsigned char* salt,
    uint32_t memcost, uint32_t iterations, uint32_t lanes,
    std::array<unsigned char, 32>& kek)
{
    if (password.size() < 12 || password.size() > 1024 || memcost != MEMCOST ||
        iterations != ITERATIONS || lanes != LANES) return false;
    Kdf kdf{EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr), EVP_KDF_free};
    if (!kdf) return false;
    KdfCtx ctx{EVP_KDF_CTX_new(kdf.get()), EVP_KDF_CTX_free};
    if (!ctx) return false;
    uint32_t threads{1};
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, const_cast<char*>(password.data()), password.size()),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<unsigned char*>(salt), 16),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iterations),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memcost),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_THREADS, &threads),
        OSSL_PARAM_construct_end(),
    };
    return EVP_KDF_derive(ctx.get(), kek.data(), kek.size(), params) == 1;
}

std::optional<std::vector<unsigned char>> Encrypt(
    std::span<const unsigned char, 32> key, const unsigned char* nonce,
    std::span<const unsigned char> aad, std::span<const unsigned char> plaintext)
{
    if (plaintext.size() > MAX_PAYLOAD) return std::nullopt;
    CipherCtx ctx{EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free};
    if (!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
        EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), nonce) != 1) return std::nullopt;
    int length{0};
    if (EVP_EncryptUpdate(ctx.get(), nullptr, &length, aad.data(), static_cast<int>(aad.size())) != 1) return std::nullopt;
    std::vector<unsigned char> result(plaintext.size() + TAG_SIZE);
    if (EVP_EncryptUpdate(ctx.get(), result.data(), &length, plaintext.data(), static_cast<int>(plaintext.size())) != 1 ||
        length != static_cast<int>(plaintext.size())) return std::nullopt;
    int final_length{0};
    if (EVP_EncryptFinal_ex(ctx.get(), result.data() + length, &final_length) != 1 || final_length != 0 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, TAG_SIZE, result.data() + plaintext.size()) != 1) return std::nullopt;
    return result;
}

std::optional<std::vector<unsigned char>> Decrypt(
    std::span<const unsigned char, 32> key, const unsigned char* nonce,
    std::span<const unsigned char> aad, std::span<const unsigned char> ciphertext)
{
    if (ciphertext.size() < TAG_SIZE || ciphertext.size() > MAX_PAYLOAD + TAG_SIZE) return std::nullopt;
    CipherCtx ctx{EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free};
    if (!ctx || EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
        EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), nonce) != 1) return std::nullopt;
    int length{0};
    if (EVP_DecryptUpdate(ctx.get(), nullptr, &length, aad.data(), static_cast<int>(aad.size())) != 1) return std::nullopt;
    const size_t data_size{ciphertext.size() - TAG_SIZE};
    std::vector<unsigned char> result(data_size);
    if (EVP_DecryptUpdate(ctx.get(), result.data(), &length, ciphertext.data(), static_cast<int>(data_size)) != 1 ||
        length != static_cast<int>(data_size) ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, TAG_SIZE,
            const_cast<unsigned char*>(ciphertext.data() + data_size)) != 1) return std::nullopt;
    int final_length{0};
    if (EVP_DecryptFinal_ex(ctx.get(), result.data() + length, &final_length) != 1 || final_length != 0) {
        crypto::CleanseMemory(result.data(), result.size());
        return std::nullopt;
    }
    return result;
}
} // namespace

std::optional<std::vector<unsigned char>> SealIdentityVault(
    std::string_view password, std::span<const unsigned char> payload)
{
    if (payload.empty() || payload.size() > MAX_PAYLOAD) return std::nullopt;
    std::vector<unsigned char> result(HEADER_SIZE);
    std::copy_n("CYBV2", 5, result.begin());
    Store32(result.data() + 5, MEMCOST);
    Store32(result.data() + 9, ITERATIONS);
    Store32(result.data() + 13, LANES);
    Store32(result.data() + LENGTH_OFFSET, static_cast<uint32_t>(payload.size()));
    if (RAND_bytes(result.data() + SALT_OFFSET, 16) != 1 ||
        RAND_bytes(result.data() + WRAP_NONCE_OFFSET, 12) != 1 ||
        RAND_bytes(result.data() + PAYLOAD_NONCE_OFFSET, 12) != 1) return std::nullopt;
    std::array<unsigned char, 32> kek{};
    std::array<unsigned char, 32> dek{};
    if (!DeriveKek(password, result.data() + SALT_OFFSET, MEMCOST, ITERATIONS, LANES, kek) ||
        RAND_bytes(dek.data(), dek.size()) != 1) {
        crypto::CleanseMemory(kek.data(), kek.size());
        return std::nullopt;
    }
    const auto wrapped = Encrypt(kek, result.data() + WRAP_NONCE_OFFSET, result, dek);
    const auto encrypted = Encrypt(dek, result.data() + PAYLOAD_NONCE_OFFSET, result, payload);
    crypto::CleanseMemory(kek.data(), kek.size());
    crypto::CleanseMemory(dek.data(), dek.size());
    if (!wrapped || !encrypted || wrapped->size() != WRAPPED_DEK_SIZE) return std::nullopt;
    result.insert(result.end(), wrapped->begin(), wrapped->end());
    result.insert(result.end(), encrypted->begin(), encrypted->end());
    return result;
}

std::optional<std::vector<unsigned char>> OpenIdentityVault(
    std::string_view password, std::span<const unsigned char> envelope)
{
    if (envelope.size() < HEADER_SIZE + WRAPPED_DEK_SIZE + TAG_SIZE ||
        !std::equal(envelope.begin(), envelope.begin() + 5, "CYBV2")) return std::nullopt;
    const uint32_t memcost{Load32(envelope.data() + 5)};
    const uint32_t iterations{Load32(envelope.data() + 9)};
    const uint32_t lanes{Load32(envelope.data() + 13)};
    const uint32_t length{Load32(envelope.data() + LENGTH_OFFSET)};
    if (length == 0 || length > MAX_PAYLOAD ||
        envelope.size() != HEADER_SIZE + WRAPPED_DEK_SIZE + length + TAG_SIZE) return std::nullopt;
    std::array<unsigned char, 32> kek{};
    if (!DeriveKek(password, envelope.data() + SALT_OFFSET, memcost, iterations, lanes, kek)) return std::nullopt;
    const auto header = envelope.first(HEADER_SIZE);
    auto wrapped = Decrypt(kek, envelope.data() + WRAP_NONCE_OFFSET, header,
        envelope.subspan(HEADER_SIZE, WRAPPED_DEK_SIZE));
    crypto::CleanseMemory(kek.data(), kek.size());
    if (!wrapped || wrapped->size() != 32) return std::nullopt;
    const auto plaintext = Decrypt(std::span<const unsigned char, 32>{wrapped->data(), 32},
        envelope.data() + PAYLOAD_NONCE_OFFSET, header,
        envelope.subspan(HEADER_SIZE + WRAPPED_DEK_SIZE));
    crypto::CleanseMemory(wrapped->data(), wrapped->size());
    return plaintext;
}

bool SaveNewIdentityVault(const std::filesystem::path& path,
    std::string_view password, std::span<const unsigned char> payload)
{
    if (path.empty() || path.filename().empty()) return false;
    const auto envelope = SealIdentityVault(password, payload);
    const auto temp = TemporaryPath(path);
    if (!envelope || !temp) return false;
    if (!WriteNewFile(*temp, *envelope)) return false;
    if (!PublishNewFile(*temp, path)) {
        std::error_code ec;
        std::filesystem::remove(*temp, ec);
        return false;
    }
    auto reopened = LoadIdentityVault(path, password);
    if (!reopened) return false;
    const bool match = reopened->size() == payload.size() &&
        CRYPTO_memcmp(reopened->data(), payload.data(), payload.size()) == 0;
    crypto::CleanseMemory(reopened->data(), reopened->size());
    return match;
}

bool PromoteIdentityVault(const std::filesystem::path& candidate_path,
    const std::filesystem::path& active_path, std::string_view password,
    std::span<const unsigned char> expected_payload)
{
    if (candidate_path.empty() || active_path.empty() || candidate_path == active_path ||
        candidate_path.filename().empty() || active_path.filename().empty() || expected_payload.empty()) return false;
    std::error_code ec;
    const auto candidate_parent = std::filesystem::absolute(candidate_path.parent_path().empty()
        ? std::filesystem::current_path() : candidate_path.parent_path(), ec).lexically_normal();
    if (ec) return false;
    const auto active_parent = std::filesystem::absolute(active_path.parent_path().empty()
        ? std::filesystem::current_path() : active_path.parent_path(), ec).lexically_normal();
    if (ec || candidate_parent != active_parent) return false;

    auto candidate_plaintext = LoadIdentityVault(candidate_path, password);
    if (!candidate_plaintext) {
        // A prior call may have renamed the candidate successfully but failed
        // while syncing the parent directory. Accept only the exact payload.
        if (std::filesystem::exists(candidate_path, ec) || ec) return false;
        auto active_plaintext = LoadIdentityVault(active_path, password);
        if (!active_plaintext) return false;
        const bool already_promoted = active_plaintext->size() == expected_payload.size() &&
            CRYPTO_memcmp(active_plaintext->data(), expected_payload.data(), expected_payload.size()) == 0;
        crypto::CleanseMemory(active_plaintext->data(), active_plaintext->size());
        return already_promoted;
    }
    const bool candidate_matches = candidate_plaintext->size() == expected_payload.size() &&
        CRYPTO_memcmp(candidate_plaintext->data(), expected_payload.data(), expected_payload.size()) == 0;
    crypto::CleanseMemory(candidate_plaintext->data(), candidate_plaintext->size());
    if (!candidate_matches) return false;

#ifdef _WIN32
    if (!MoveFileExW(candidate_path.c_str(), active_path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
#else
    if (::rename(candidate_path.c_str(), active_path.c_str()) != 0) return false;
    const auto parent = active_path.parent_path().empty() ? std::filesystem::path{"."} : active_path.parent_path();
    const int dirfd = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd < 0) return false;
    const bool synced = fsync(dirfd) == 0;
    close(dirfd);
    if (!synced) return false;
#endif

    auto promoted_plaintext = LoadIdentityVault(active_path, password);
    if (!promoted_plaintext) return false;
    const bool promoted_matches = promoted_plaintext->size() == expected_payload.size() &&
        CRYPTO_memcmp(promoted_plaintext->data(), expected_payload.data(), expected_payload.size()) == 0;
    crypto::CleanseMemory(promoted_plaintext->data(), promoted_plaintext->size());
    return promoted_matches;
}

bool ReplaceIdentityVault(const std::filesystem::path& path,
    std::string_view password, std::span<const unsigned char> expected_payload,
    std::span<const unsigned char> replacement_payload)
{
    if (path.empty() || path.filename().empty() || expected_payload.empty() || replacement_payload.empty() ||
        expected_payload.size() > MAX_PAYLOAD || replacement_payload.size() > MAX_PAYLOAD) return false;

    const VaultFileLock lock{path};
    if (!lock) return false;

    auto current = LoadIdentityVault(path, password);
    if (!current) return false;
    const bool expected = current->size() == expected_payload.size() &&
        CRYPTO_memcmp(current->data(), expected_payload.data(), expected_payload.size()) == 0;
    crypto::CleanseMemory(current->data(), current->size());
    if (!expected) return false;

    const auto envelope = SealIdentityVault(password, replacement_payload);
    const auto temp = TemporaryPath(path);
    if (!envelope || !temp || !WriteNewFile(*temp, *envelope)) return false;
#ifdef _WIN32
    if (!MoveFileExW(temp->c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp->c_str());
        return false;
    }
#else
    if (::rename(temp->c_str(), path.c_str()) != 0) {
        std::error_code ec;
        std::filesystem::remove(*temp, ec);
        return false;
    }
    const auto parent = path.parent_path().empty() ? std::filesystem::path{"."} : path.parent_path();
    const int dirfd = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd < 0) return false;
    const bool synced = fsync(dirfd) == 0;
    close(dirfd);
    if (!synced) return false;
#endif

    auto reopened = LoadIdentityVault(path, password);
    if (!reopened) return false;
    const bool matches = reopened->size() == replacement_payload.size() &&
        CRYPTO_memcmp(reopened->data(), replacement_payload.data(), replacement_payload.size()) == 0;
    crypto::CleanseMemory(reopened->data(), reopened->size());
    return matches;
}

std::optional<std::vector<unsigned char>> LoadIdentityVault(
    const std::filesystem::path& path, std::string_view password)
{
    const auto envelope = ReadBoundedFile(path);
    if (!envelope) return std::nullopt;
    return OpenIdentityVault(password, *envelope);
}

} // namespace cybou
