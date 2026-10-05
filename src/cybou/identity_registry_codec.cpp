// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Каноническая сериализация и десериализация реестра Identity.

#include <cybou/identity_registry.h>

#include <algorithm>

namespace cybou {
namespace {
constexpr size_t RECOVERY_PUBLIC_SIZE{32 + 1952};
constexpr size_t AUTHORIZATION_PUBLIC_SIZE{32 + 1312};
constexpr size_t ACCOUNT_SIZE{32 + RECOVERY_PUBLIC_SIZE + AUTHORIZATION_PUBLIC_SIZE + 32 + 8 + 8};

void Write64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

class Reader
{
public:
    explicit Reader(std::span<const unsigned char> bytes) : m_bytes{bytes} {}
    bool Read(std::span<unsigned char> out)
    {
        if (out.size() > Remaining()) return false;
        std::copy_n(m_bytes.begin() + m_offset, out.size(), out.begin());
        m_offset += out.size();
        return true;
    }
    std::optional<uint8_t> U8()
    {
        if (!Remaining()) return std::nullopt;
        return m_bytes[m_offset++];
    }
    std::optional<uint32_t> U32()
    {
        if (Remaining() < 4) return std::nullopt;
        uint32_t value{0};
        for (unsigned i{0}; i < 4; ++i) value |= uint32_t{m_bytes[m_offset++]} << (8 * i);
        return value;
    }
    std::optional<uint64_t> U64()
    {
        if (Remaining() < 8) return std::nullopt;
        uint64_t value{0};
        for (unsigned i{0}; i < 8; ++i) value |= uint64_t{m_bytes[m_offset++]} << (8 * i);
        return value;
    }
    size_t Remaining() const { return m_bytes.size() - m_offset; }

private:
    std::span<const unsigned char> m_bytes;
    size_t m_offset{0};
};

void WritePublic(std::vector<unsigned char>& out, const IdentityHybridPublicKey& key)
{
    out.insert(out.end(), key.ed25519.begin(), key.ed25519.end());
    out.insert(out.end(), key.ml_dsa.begin(), key.ml_dsa.end());
}

std::optional<IdentityHybridPublicKey> ReadPublic(Reader& reader, IdentityKeyPurpose purpose, size_t pq_size)
{
    IdentityHybridPublicKey key{};
    key.purpose = purpose;
    key.ml_dsa.resize(pq_size);
    if (!reader.Read(key.ed25519) || !reader.Read(key.ml_dsa)) return std::nullopt;
    return key;
}

bool Nonzero(std::span<const unsigned char> bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](unsigned char b) { return b != 0; });
}
} // namespace

std::optional<std::vector<unsigned char>> SerializeIdentityRegistry(const IdentityRegistry& registry)
{
    if (registry.m_accounts.size() > MAX_IDENTITY_REGISTRY_ACCOUNTS ||
        registry.m_accounts.size() != registry.m_recovery_index.size()) return std::nullopt;
    std::vector<unsigned char> out;
    const auto count = static_cast<uint32_t>(registry.m_accounts.size());
    out.reserve(4 + registry.m_accounts.size() * ACCOUNT_SIZE);
    for (unsigned i{0}; i < 4; ++i) out.push_back(static_cast<unsigned char>(count >> (8 * i)));
    for (const auto& [account_id, record] : registry.m_accounts) {
        const auto recovery_id = ComputeRecoveryKeyId(record.recovery_key);
        const auto authorization_id = ComputeAuthorizationKeyId(record.authorization_key);
        if (account_id.IsNull() || !recovery_id || !authorization_id || !Nonzero(record.kem_package_id) ||
            !registry.m_recovery_index.contains(*recovery_id) || registry.m_recovery_index.at(*recovery_id) != account_id) return std::nullopt;
        out.insert(out.end(), account_id.Value().begin(), account_id.Value().end());
        WritePublic(out, record.recovery_key);
        WritePublic(out, record.authorization_key);
        out.insert(out.end(), record.kem_package_id.begin(), record.kem_package_id.end());
        Write64(out, record.nonce);
        Write64(out, record.key_epoch);
    }
    return out;
}

std::optional<IdentityRegistry> DeserializeIdentityRegistry(std::span<const unsigned char> bytes)
{
    Reader reader{bytes};
    const auto count = reader.U32();
    if (!count || *count > MAX_IDENTITY_REGISTRY_ACCOUNTS ||
        *count > reader.Remaining() / ACCOUNT_SIZE) return std::nullopt;
    IdentityRegistry registry;
    std::optional<AccountId> prior_account;
    for (uint32_t i{0}; i < *count; ++i) {
        cybou::Hash256 raw_id;
        if (!reader.Read(std::span<unsigned char>{raw_id.begin(), raw_id.size()})) return std::nullopt;
        const AccountId account_id{raw_id};
        if (account_id.IsNull() || (prior_account && !(*prior_account < account_id))) return std::nullopt;
        prior_account = account_id;
        auto recovery = ReadPublic(reader, IdentityKeyPurpose::RECOVERY_ROOT, RECOVERY_PUBLIC_SIZE - 32);
        auto authorization = ReadPublic(reader, IdentityKeyPurpose::AUTHORIZATION, AUTHORIZATION_PUBLIC_SIZE - 32);
        IdentityKeyId package_id{};
        const bool package_read = reader.Read(package_id);
        const auto nonce = reader.U64();
        const auto epoch = reader.U64();
        if (!recovery || !authorization || !package_read || !Nonzero(package_id) || !nonce || !epoch) return std::nullopt;
        const auto recovery_id = ComputeRecoveryKeyId(*recovery);
        if (!recovery_id || registry.m_recovery_index.contains(*recovery_id) || !ComputeAuthorizationKeyId(*authorization)) return std::nullopt;
        IdentityRecord record{
            .recovery_key = std::move(*recovery),
            .authorization_key = std::move(*authorization),
            .kem_package_id = package_id,
            .nonce = *nonce,
            .key_epoch = *epoch,
        };
        registry.m_recovery_index.emplace(*recovery_id, account_id);
        registry.m_accounts.emplace(account_id, std::move(record));
    }
    if (reader.Remaining()) return std::nullopt;
    return registry;
}

} // namespace cybou
