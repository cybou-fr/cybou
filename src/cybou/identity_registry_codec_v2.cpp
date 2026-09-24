// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_registry_v2.h>

#include <algorithm>
#include <set>

namespace cybou {
namespace {
constexpr unsigned char VERSION{2};
constexpr size_t ROOT_PUBLIC_SIZE{32 + 1952};
constexpr size_t DEVICE_PUBLIC_SIZE{32 + 1312};
constexpr size_t MIN_ACCOUNT_SIZE{32 + ROOT_PUBLIC_SIZE + 8 + 1};
constexpr size_t DEVICE_SIZE{DEVICE_PUBLIC_SIZE + 8 + 8};

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
        uint32_t result{0};
        for (unsigned i{0}; i < 4; ++i) result |= uint32_t{m_bytes[m_offset++]} << (8 * i);
        return result;
    }

    std::optional<uint64_t> U64()
    {
        if (Remaining() < 8) return std::nullopt;
        uint64_t result{0};
        for (unsigned i{0}; i < 8; ++i) result |= uint64_t{m_bytes[m_offset++]} << (8 * i);
        return result;
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
} // namespace

std::optional<std::vector<unsigned char>> SerializeIdentityRegistryV2(const IdentityRegistryV2& registry)
{
    if (registry.m_accounts.size() > MAX_IDENTITY_REGISTRY_ACCOUNTS_V2 ||
        registry.m_accounts.size() != registry.m_recovery_index.size()) return std::nullopt;
    std::vector<unsigned char> out;
    out.push_back(VERSION);
    const auto count = static_cast<uint32_t>(registry.m_accounts.size());
    for (unsigned i{0}; i < 4; ++i) out.push_back(static_cast<unsigned char>(count >> (8 * i)));
    for (const auto& [account_id, record] : registry.m_accounts) {
        const auto root_id = ComputeRecoveryKeyId(record.recovery_root);
        if (account_id.IsNull() || !root_id || record.devices.size() > MAX_ACTIVE_DEVICES_V2 ||
            !registry.m_recovery_index.contains(*root_id) || registry.m_recovery_index.at(*root_id) != account_id) return std::nullopt;
        out.insert(out.end(), account_id.Value().begin(), account_id.Value().end());
        WritePublic(out, record.recovery_root);
        Write64(out, record.next_root_nonce);
        out.push_back(static_cast<unsigned char>(record.devices.size()));
        std::set<uint64_t> activations;
        for (const auto& [id, device] : record.devices) {
            if (ComputeDeviceKeyId(device.key) != id ||
                device.key.ed25519 == record.recovery_root.ed25519 ||
                device.activation_nonce > record.next_root_nonce ||
                !activations.emplace(device.activation_nonce).second) return std::nullopt;
            WritePublic(out, device.key);
            Write64(out, device.next_nonce);
            Write64(out, device.activation_nonce);
        }
    }
    return out;
}

std::optional<IdentityRegistryV2> DeserializeIdentityRegistryV2(std::span<const unsigned char> bytes)
{
    Reader reader{bytes};
    const auto version = reader.U8();
    const auto count = reader.U32();
    if (!version || *version != VERSION || !count || *count > MAX_IDENTITY_REGISTRY_ACCOUNTS_V2 ||
        *count > reader.Remaining() / MIN_ACCOUNT_SIZE) return std::nullopt;
    IdentityRegistryV2 registry;
    std::optional<AccountId> prior_account;
    for (uint32_t i{0}; i < *count; ++i) {
        uint256 raw_id;
        if (!reader.Read(std::span<unsigned char>{raw_id.begin(), raw_id.size()})) return std::nullopt;
        const AccountId account_id{raw_id};
        if (account_id.IsNull() || (prior_account && !(*prior_account < account_id))) return std::nullopt;
        prior_account = account_id;
        auto root = ReadPublic(reader, IdentityKeyPurpose::RECOVERY_ROOT, ROOT_PUBLIC_SIZE - 32);
        const auto nonce = reader.U64();
        const auto device_count = reader.U8();
        if (!root || !nonce || !device_count || *device_count > MAX_ACTIVE_DEVICES_V2 ||
            *device_count > reader.Remaining() / DEVICE_SIZE) return std::nullopt;
        const auto root_id = ComputeRecoveryKeyId(*root);
        if (!root_id || registry.m_recovery_index.contains(*root_id)) return std::nullopt;
        IdentityRecordV2 record{};
        record.recovery_root = std::move(*root);
        record.next_root_nonce = *nonce;
        std::optional<IdentityKeyIdV2> prior_device;
        std::set<uint64_t> activations;
        for (uint8_t j{0}; j < *device_count; ++j) {
            auto key = ReadPublic(reader, IdentityKeyPurpose::DEVICE, DEVICE_PUBLIC_SIZE - 32);
            const auto device_nonce = reader.U64();
            const auto activation_nonce = reader.U64();
            if (!key || !device_nonce || !activation_nonce || *activation_nonce > *nonce ||
                !activations.emplace(*activation_nonce).second ||
                key->ed25519 == record.recovery_root.ed25519) return std::nullopt;
            const auto device_id = ComputeDeviceKeyId(*key);
            if (!device_id || (prior_device && !(*prior_device < *device_id))) return std::nullopt;
            prior_device = *device_id;
            record.devices.emplace(*device_id, IdentityDeviceV2{std::move(*key), *device_nonce, *activation_nonce});
        }
        registry.m_accounts.emplace(account_id, std::move(record));
        registry.m_recovery_index.emplace(*root_id, account_id);
    }
    if (reader.Remaining()) return std::nullopt;
    return registry;
}
} // namespace cybou
