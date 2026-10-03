// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_KEYSTORE_H
#define CYBOU_KEYSTORE_H

#include <cybou/account_id.h>
#include <cybou/chunk_id.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/identity_crypto.h>
#include <cybou/identity_kem.h>
#include <cybou/identity_material.h>
#include <cybou/poa_signer.h>
#include <cybou/validation_attestation.h>
#include <cybou/hash256.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace cybou {

struct RootRecipientCapsule;

/**
 * Local identity secrets backed by a portable password-protected CYBV vault.
 * AccountID is random and independent of mnemonic-derived Identity keys.
 */
class CybouKeyStore {
public:
    CybouKeyStore();
    ~CybouKeyStore();

    CybouKeyStore(const CybouKeyStore&) = delete;
    CybouKeyStore& operator=(const CybouKeyStore&) = delete;
    CybouKeyStore(CybouKeyStore&&) noexcept;
    CybouKeyStore& operator=(CybouKeyStore&&) noexcept;

    /** Generate a random AccountID and recovery entropy; all key roles derive from entropy. */
    bool GenerateNew();
    bool LoadMaterial(IdentityMaterial material);
    bool LoadFromFile(const std::filesystem::path& path, std::string_view password);
    bool SaveToFile(const std::filesystem::path& path, std::string_view password) const;
    std::optional<IdentityMaterial> CreateIdentityRotationMaterial(
        std::span<const unsigned char, 32> new_recovery_entropy) const;
    std::optional<RecoveryWords> GetRecoveryWords() const;
    /** True when the loaded recovery entropy derives exactly `key` for `purpose`; the entropy never leaves the store. */
    bool DerivesPublicKey(IdentityKeyPurpose purpose, const IdentityHybridPublicKey& key) const;
    std::optional<IdentityHybridPublicKey> GetPoaFinalizerPublicKey() const;
    std::optional<IdentityHybridSignature> SignPoaFinalizerMessage(
        std::span<const unsigned char> message) const;

    /** Securely wipe the in-memory key */
    void Clear();

    /** Inspect identity */
    bool HasKey() const;
    std::optional<AccountId> GetAccountId() const;
    std::optional<XWingPublicKey> GetIdentityXWingPublicKey() const;
    bool ValidateIdentityXWingKeyPair() const;

    /** Domain-separated post-quantum key roles derived from the Identity entropy. */
    std::optional<IdentityHybridPublicKey> GetAuthorizationPublicKey() const;
    std::optional<IdentityHybridPublicKey> GetRecoveryPublicKey() const;
    std::optional<IdentityHybridSignature> SignAuthorization(std::span<const unsigned char> digest) const;
    std::optional<IdentityHybridSignature> SignRecovery(std::span<const unsigned char> digest) const;

    /** Key for the rebuildable, per-Identity local application projection. */
    std::optional<std::array<unsigned char, 32>> DeriveApplicationStoreKey() const;

    /**
     * Opens a RootPublication capsule addressed to this Identity without
     * exporting KEM secrets. current_key_epoch is the Identity's finalized
     * epoch; older epochs open only with seeds imported from a verified
     * RecoveryBridge.
     */
    std::optional<ContentKey> OpenRootCapsule(std::span<const unsigned char, 32> network_binding,
        const AccountId& sender, std::uint64_t sender_nonce, std::uint64_t sender_key_epoch,
        const ChunkId& root_chunk_id, const RootRecipientCapsule& capsule,
        std::uint64_t current_key_epoch) const;
    /** Caller must first verify the seed against the canonical KEM commitment of that epoch. */
    bool ImportHistoricalKemSeed(std::uint64_t key_epoch, const XWingSeed& seed);
    bool HasKemSeedForEpoch(std::uint64_t key_epoch, std::uint64_t current_key_epoch) const;
    /** Every known KEM seed (historical plus current), only for sealing a RecoveryBridge. */
    std::vector<std::pair<std::uint64_t, XWingSeed>> KemSeedsForRecoveryBridge(std::uint64_t current_key_epoch) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/** A PoA signer view over an unlocked vault; the recovery entropy never leaves CybouKeyStore. */
class CybouKeyStorePoaSigner final : public PoaSigner {
public:
    explicit CybouKeyStorePoaSigner(const CybouKeyStore& keystore) : m_keystore{keystore} {}
    std::optional<IdentityHybridPublicKey> PublicKey() const override;
    std::optional<IdentityHybridSignature> Sign(std::span<const unsigned char> message) const override;

private:
    const CybouKeyStore& m_keystore;
};

/** Validation signer over an unlocked vault; it signs nothing once the vault is locked. */
class CybouKeyStoreValidationSigner final : public ValidationSigner {
public:
    explicit CybouKeyStoreValidationSigner(const CybouKeyStore& keystore) : m_keystore{keystore} {}
    std::optional<AccountId> Account() const override { return m_keystore.GetAccountId(); }
    std::optional<IdentityHybridSignature> SignAuthorization(std::span<const unsigned char> digest) const override
    {
        return m_keystore.SignAuthorization(digest);
    }

private:
    const CybouKeyStore& m_keystore;
};

} // namespace cybou

#endif // CYBOU_KEYSTORE_H
