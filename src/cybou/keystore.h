// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_KEYSTORE_H
#define CYBOU_KEYSTORE_H

#include <cybou/account_id.h>
#include <cybou/identity_crypto.h>
#include <cybou/identity_material.h>
#include <uint256.h>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/**
 * Local identity secrets backed by a portable password-protected CYBV2 vault.
 * AccountID is random and independent of root, device, and mail keys.
 */
class CybouKeyStore {
public:
    CybouKeyStore();
    ~CybouKeyStore();

    CybouKeyStore(const CybouKeyStore&) = delete;
    CybouKeyStore& operator=(const CybouKeyStore&) = delete;
    CybouKeyStore(CybouKeyStore&&) noexcept;
    CybouKeyStore& operator=(CybouKeyStore&&) noexcept;

    /** Generate a random AccountID, recovery entropy, and device secret. */
    bool GenerateNew();
    bool LoadMaterial(IdentityMaterial material);
    bool LoadFromFile(const std::filesystem::path& path, std::string_view password);
    bool SaveToFile(const std::filesystem::path& path, std::string_view password) const;
    std::optional<RecoveryWords> GetRecoveryWords() const;

    /** Securely wipe the in-memory key */
    void Clear();

    /** Inspect identity */
    bool HasKey() const;
    std::optional<uint256> GetPublicKey() const;
    std::optional<AccountId> GetAccountId() const;
    std::optional<uint256> GetX25519PublicKey() const;

    /** Derive a Diffie-Hellman shared secret with a peer X25519 public key using the internal key */
    std::optional<std::array<unsigned char, 32>> DeriveX25519SharedSecret(const uint256& peer_x25519_pubkey) const;

    /** Post-Quantum device and recovery root access */
    std::optional<IdentityHybridPublicKey> GetDevicePublicKey() const;
    std::optional<IdentityHybridPublicKey> GetRecoveryPublicKey() const;
    std::optional<std::array<unsigned char, 32>> GetDeviceId() const;
    std::optional<IdentityHybridSignature> SignDevice(std::span<const unsigned char> digest) const;
    std::optional<IdentityHybridSignature> SignRecovery(std::span<const unsigned char> digest) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cybou

#endif // CYBOU_KEYSTORE_H
