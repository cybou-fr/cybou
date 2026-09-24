// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/keystore.h>
#include <random.h>
#include <support/cleanse.h>

#include <algorithm>
#include <fstream>

#ifdef WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace cybou {

struct CybouKeyStore::Impl {
    std::optional<std::array<unsigned char, 32>> seed;
    std::optional<uint256> public_key;
    std::optional<AccountId> account_id;

    ~Impl()
    {
        Clear();
    }

    void Clear()
    {
        if (seed.has_value()) {
            memory_cleanse(seed->data(), seed->size());
            seed.reset();
        }
        public_key.reset();
        account_id.reset();
    }

    bool SetSeed(std::span<const unsigned char, 32> in_seed)
    {
        Clear();
        std::array<unsigned char, 32> s{};
        std::copy(in_seed.begin(), in_seed.end(), s.begin());
        const auto pub = DeriveEd25519PublicKey(s);
        if (!pub) {
            memory_cleanse(s.data(), s.size());
            return false;
        }
        seed = s;
        public_key = *pub;
        account_id = AccountId{*pub};
        memory_cleanse(s.data(), s.size());
        return true;
    }
};

CybouKeyStore::CybouKeyStore() : m_impl{std::make_unique<Impl>()} {}
CybouKeyStore::~CybouKeyStore() = default;
CybouKeyStore::CybouKeyStore(CybouKeyStore&&) noexcept = default;
CybouKeyStore& CybouKeyStore::operator=(CybouKeyStore&&) noexcept = default;

bool CybouKeyStore::GenerateNew()
{
    std::array<unsigned char, 32> rand_seed{};
    GetStrongRandBytes(rand_seed);
    const bool ok = m_impl->SetSeed(rand_seed);
    memory_cleanse(rand_seed.data(), rand_seed.size());
    return ok;
}

bool CybouKeyStore::LoadFromSeed(std::span<const unsigned char, 32> seed)
{
    return m_impl->SetSeed(seed);
}

void CybouKeyStore::Clear()
{
    m_impl->Clear();
}

bool CybouKeyStore::HasKey() const
{
    return m_impl->seed.has_value();
}

std::optional<uint256> CybouKeyStore::GetPublicKey() const
{
    return m_impl->public_key;
}

std::optional<AccountId> CybouKeyStore::GetAccountId() const
{
    return m_impl->account_id;
}

std::optional<std::array<unsigned char, 64>> CybouKeyStore::Sign(const uint256& digest) const
{
    if (!m_impl->seed.has_value()) return std::nullopt;
    return SignUserMessage(*m_impl->seed, digest);
}

bool CybouKeyStore::SaveToFile(const std::filesystem::path& path) const
{
    if (!m_impl->seed.has_value()) return false;

    std::vector<unsigned char> file_bytes;
#ifdef WIN32
    DATA_BLOB plain_blob;
    plain_blob.pbData = const_cast<unsigned char*>(m_impl->seed->data());
    plain_blob.cbData = static_cast<DWORD>(m_impl->seed->size());

    DATA_BLOB cipher_blob{};
    if (!CryptProtectData(&plain_blob, L"CYBOU Identity Key", nullptr, nullptr, nullptr, 0, &cipher_blob)) {
        return false;
    }

    file_bytes.reserve(KEYSTORE_MAGIC.size() + 4 + cipher_blob.cbData);
    file_bytes.insert(file_bytes.end(), KEYSTORE_MAGIC.begin(), KEYSTORE_MAGIC.end());
    const uint32_t len = static_cast<uint32_t>(cipher_blob.cbData);
    for (unsigned i = 0; i < 4; ++i) file_bytes.push_back(static_cast<unsigned char>(len >> (8 * i)));
    file_bytes.insert(file_bytes.end(), cipher_blob.pbData, cipher_blob.pbData + cipher_blob.cbData);
    LocalFree(cipher_blob.pbData);
#else
    file_bytes.reserve(KEYSTORE_MAGIC.size() + 4 + m_impl->seed->size());
    file_bytes.insert(file_bytes.end(), KEYSTORE_MAGIC.begin(), KEYSTORE_MAGIC.end());
    const uint32_t len = static_cast<uint32_t>(m_impl->seed->size());
    for (unsigned i = 0; i < 4; ++i) file_bytes.push_back(static_cast<unsigned char>(len >> (8 * i)));
    file_bytes.insert(file_bytes.end(), m_impl->seed->begin(), m_impl->seed->end());
#endif

    const auto tmp_path = path.string() + ".tmp";
    const auto bak_path = path.string() + ".bak";

    std::ofstream out(tmp_path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(file_bytes.data()), file_bytes.size());
    out.flush();
    if (!out.good()) {
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return false;
    }
    out.close();

#ifdef WIN32
    HANDLE hFile = CreateFileW(std::filesystem::path(tmp_path).c_str(),
                               GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }
#endif

    std::error_code ec;
    // Create backup of existing file if present
    if (std::filesystem::exists(path, ec)) {
        std::filesystem::copy_file(path, bak_path, std::filesystem::copy_options::overwrite_existing, ec);
    }

    // Atomic replace
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        std::filesystem::rename(tmp_path, path, ec);
        if (ec) return false;
    }
    return true;
}

bool CybouKeyStore::LoadFromFile(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) return false;
    const auto file_size = std::filesystem::file_size(path);

    // Support legacy unencrypted 32-byte key file and migrate immediately to protected format
    if (file_size == 32) {
        std::array<unsigned char, 32> raw_seed{};
        {
            std::ifstream in(path, std::ios::binary);
            if (!in || !in.read(reinterpret_cast<char*>(raw_seed.data()), 32)) return false;
        }
        const bool ok = m_impl->SetSeed(raw_seed);
        memory_cleanse(raw_seed.data(), raw_seed.size());
        if (ok) {
            // Immediately migrate to protected format on disk
            SaveToFile(path);
        }
        return ok;
    }

    if (file_size < KEYSTORE_MAGIC.size() + 4) return false;

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    std::array<unsigned char, KEYSTORE_MAGIC.size()> magic{};
    in.read(reinterpret_cast<char*>(magic.data()), magic.size());
    if (magic != KEYSTORE_MAGIC) return false;

    uint32_t payload_len{0};
    in.read(reinterpret_cast<char*>(&payload_len), sizeof(payload_len));
    if (payload_len == 0 || payload_len > 1024 * 1024 ||
        static_cast<size_t>(payload_len) != file_size - KEYSTORE_MAGIC.size() - 4) {
        return false;
    }

    std::vector<unsigned char> payload(payload_len);
    if (!in.read(reinterpret_cast<char*>(payload.data()), payload_len)) return false;

#ifdef WIN32
    DATA_BLOB cipher_blob;
    cipher_blob.pbData = payload.data();
    cipher_blob.cbData = static_cast<DWORD>(payload.size());

    DATA_BLOB plain_blob{};
    if (!CryptUnprotectData(&cipher_blob, nullptr, nullptr, nullptr, nullptr, 0, &plain_blob)) {
        return false;
    }

    if (plain_blob.cbData != 32) {
        memory_cleanse(plain_blob.pbData, plain_blob.cbData);
        LocalFree(plain_blob.pbData);
        return false;
    }

    std::span<const unsigned char, 32> s{plain_blob.pbData, 32};
    const bool ok = m_impl->SetSeed(s);
    memory_cleanse(plain_blob.pbData, plain_blob.cbData);
    LocalFree(plain_blob.pbData);
    return ok;
#else
    if (payload.size() != 32) return false;
    std::span<const unsigned char, 32> s{payload.data(), 32};
    const bool ok = m_impl->SetSeed(s);
    memory_cleanse(payload.data(), payload.size());
    return ok;
#endif
}

} // namespace cybou
