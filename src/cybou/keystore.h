// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_KEYSTORE_H
#define CYBOU_KEYSTORE_H

#include <cybou/account_id.h>
#include <cybou/signing.h>
#include <uint256.h>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr std::array<unsigned char, 5> KEYSTORE_MAGIC{'C', 'Y', 'B', 'K', '1'};

/**
 * CybouKeyStore encapsulates Ed25519 identity key generation, signing,
 * and encrypted storage at rest (using OS-protected DPAPI on Windows).
 * The raw 32-byte private key seed is kept in cleansed memory and never
 * exported via public interfaces.
 */
class CybouKeyStore {
public:
    CybouKeyStore();
    ~CybouKeyStore();

    CybouKeyStore(const CybouKeyStore&) = delete;
    CybouKeyStore& operator=(const CybouKeyStore&) = delete;
    CybouKeyStore(CybouKeyStore&&) noexcept;
    CybouKeyStore& operator=(CybouKeyStore&&) noexcept;

    /** Generate a new random identity key */
    bool GenerateNew();

    /** Load an existing identity from a 32-byte seed */
    bool LoadFromSeed(std::span<const unsigned char, 32> seed);

    /** Load protected key from file. Also supports migrating legacy 32-byte raw key files. */
    bool LoadFromFile(const std::filesystem::path& path);

    /** Save protected key to file (DPAPI encrypted on Windows) */
    bool SaveToFile(const std::filesystem::path& path) const;

    /** Securely wipe the in-memory key */
    void Clear();

    /** Inspect identity */
    bool HasKey() const;
    std::optional<uint256> GetPublicKey() const;
    std::optional<AccountId> GetAccountId() const;
    std::optional<uint256> GetX25519PublicKey() const;

    /** Sign a digest using the protected Ed25519 private key */
    std::optional<std::array<unsigned char, 64>> Sign(const uint256& digest) const;

    /** Derive a Diffie-Hellman shared secret with a peer X25519 public key using the internal key */
    std::optional<std::array<unsigned char, 32>> DeriveX25519SharedSecret(const uint256& peer_x25519_pubkey) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cybou

#endif // CYBOU_KEYSTORE_H
