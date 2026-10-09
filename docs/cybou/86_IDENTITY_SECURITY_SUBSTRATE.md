# 86 — Identity security substrate

Status: CURRENT
Scope: Signing-role, descriptor and vault source audit at 75ba7969, 2026-10-09. Standards/composition and full desktop acceptance remain separate.

Recorded status: canonical user-service security architecture. This document defines
the target ownership and capability model. `26_IMPLEMENTATION_STATUS.md`
records which parts are wired into the current code.

## Purpose

CYBOU Identity is the security root for user services. One stable AccountID
owns recovery authority and the lifecycle of current Identity key epochs. Name, Wallet,
Mail, and Files are capabilities of that identity; they do not create separate
user identities or parallel authorization systems.

```text
                         CYBOU Identity
                               │
            ┌──────────────────┴──────────────────┐
            │                                     │
    Hybrid authorization                  DEV key agreement
    Ed25519 + ML-DSA-44                 X-Wing (draft-05)
            │                                     │
 IdentityOperationCoordinator               Key wrapping / CEKs
            │                                     │
    ┌───────┼────────┬───────┐           ┌────────┴──────────┐
    │       │        │       │           │                   │
   Name   Wallet    Mail    Files     Mail content       Files objects
            │                            + attachments
```

The Recovery Root is a separate authorization domain. The offline Network Private Key,
Release Signing, and Treasury keys are separate system domains. The operational
PoA key `P` is a distinct role of the ordinary `cybou.cybou` Identity; it is
not used for ordinary user-service authorization.

## Identity and system key domains

| Domain | Key material | Purpose | Current status |
|---|---|---|---|
| Recovery Root | Ed25519 + ML-DSA-65 | Recover account authority and authorize IdentityRotate | Implemented in the current identity path; encoding/vector work remains tracked by Identity docs |
| Identity authorization signing | Ed25519 + ML-DSA-44 | Authorize account operations and storage payout bindings | Implemented for Identity authorization |
| Identity key agreement | X-Wing (ML-KEM-768 + X25519) | Establish or wrap content keys for a current Identity key epoch | Draft-05 profile is published per current Identity key epoch in DEV |
| Network Key | Hybrid PQ (Public Key = NetworkID) | Offline creation-time root of trust; signs immutable genesis specification once | Compiled signed genesis verification exists; private derivation/signing is excluded from production builds |
| PoA finalizer P | Ed25519 + ML-DSA-65 | Authorized in genesis; signs canonical block certificates | Implemented for single-operator PoA |

Private keys remain client-controlled. DEV Identity binds one X-Wing public
package to AccountID and key_epoch. Its draft-05 wire format and
publication rules are frozen for DEV in `89_IDENTITY_KEM_PUBLICATION.md`.

Signing keys MUST NOT be converted into or reused as Mail, Files, or Storage
encryption keys. Recovery keys are not routine service-signing keys. Key
purpose, NetworkBinding, operation kind, key_epoch, and payload commitment
must be authenticated in their respective protocols.

## Service capability model

| Capability | Authorization | Confidentiality / key ownership | Finality or durability |
|---|---|---|---|
| Name | Identity hybrid signature through the coordinator | No content encryption capability | PoA finality for commit and reveal |
| Wallet | Identity hybrid signature through the coordinator | No content encryption capability | PoA finality for payment and lock operations |
| Mail | Identity-authorized generic RootPublication through the coordinator | Recipient KEM capsule and encrypted Mail schema inside the chunk tree | PoA finality authorizes chunk admission; storage durability (2 remote replicas) reported separately |
| Files | Identity-authorized manifest/root changes through the coordinator | Symmetric object encryption; account KEM capability wraps Files keys | Storage durability contract (2 remote replicas + local = 3 physical copies total) |

“Hybrid” does not mean every operation uses every key. Signatures authorize
actions; KEM establishes or wraps content keys; a standard symmetric AEAD
protects bulk content. PQ signature profiles are defined in `09_CRYPTO_PQ.md`;
the DEV KEM profile is defined in `89_IDENTITY_KEM_PUBLICATION.md`.

## Shared authorization lifecycle

The shared coordinator is the sole allocator and signer for Identity-authorized
service operations. A service supplies an operation kind, payload commitment,
and operation builder. It does not allocate a nonce or sign an operation outside the shared
Identity authorization contract. The coordinator persists the exact
operation before submission and reconciles uncertain delivery before another
operation can use that shared Identity nonce. See `87_IDENTITY_OPERATION_COORDINATOR.md`.

IdentityRotate uses the same shared Identity nonce and atomically replaces
Recovery, Authorization, and KEM roles. It requires authorization by the old
Recovery key and proofs of possession for the new Recovery and Authorization
keys. It follows the common submission and reconciliation contract.

## Product surface

```text
Create or restore one Identity
        │
        ├── choose a .cybou name
        ├── use Wallet
        ├── use Mail
        └── use Files
```

The user does not create a separate Wallet account, Mail profile, or Storage
account. Files is the Beta Drive-like product surface; “Drive-like” is an
interaction reference, not a second service. Backup is a post-Beta application
of the same Storage layer.

## Implementation boundary

The current runtime implements the shared coordinator for Wallet payments,
System Balance locks, Name commit/reveal, RootPublication, revocation, leases
and IdentityRotate.
The current identity record publishes a draft-05 X-Wing KEM capability.

## Authority and related documents

- `10_IDENTITY_NAMES.md` owns identity, recovery, and name protocol rules.
- `76_IDENTITY_VAULT_RECOVERY.md` owns phrase and portable-vault behavior.
- `87_IDENTITY_OPERATION_COORDINATOR.md` owns operation/nonce lifecycle.
- `ENCRYPTED_CHUNK_TREE.md` and `ROOT_PUBLICATION.md` own encrypted content
  keys, publication, and chunk privacy.
- `89_IDENTITY_KEM_PUBLICATION.md` owns the account KEM publication design and DEV profile.
- `81_BETA_PRODUCT_SCOPE.md` owns Beta readiness.
- `26_IMPLEMENTATION_STATUS.md` owns current implementation status.

## Current signing-role bytes

[identity_crypto.h](../../src/cybou/identity_crypto.h) defines purpose values;
[identity_crypto.cpp](../../src/cybou/identity_crypto.cpp) defines algorithm sizes
and derivation. Every row uses Ed25519 (public key 32, signature 64 bytes) plus
the listed ML-DSA component; verification requires both signatures over the same
supplied message. The caller provides the domain-separated message and enforces
the expected purpose. The generic helper does not add a universal network envelope.

| Purpose | Value | ML-DSA | Public / signature bytes | HKDF info prefix |
|---|---|---|---|---|
| Recovery | 1 | 65 | 1952 / 3309 | CYBOU/IDENTITY/ROOT/ |
| Authorization | 2 | 44 | 1312 / 2420 | CYBOU/IDENTITY/AUTH/ |
| PoA | 7 | 65 | 1952 / 3309 | CYBOU/IDENTITY/POA_FINALIZER/ |
| Storage | 8 | 44 | 1312 / 2420 | CYBOU/IDENTITY/STORAGE/ |
| Offline Network Root | 9 | 65 | 1952 / 3309 | CYBOU/IDENTITY/NETWORK_ROOT/ |

HKDF-SHA256 takes the role secret as input, salt `"CYBOU/IDENTITY/HKDF-SHA256"`,
and the exact prefix above followed by `ED25519` or `ML-DSA-44`/`ML-DSA-65` as
info, yielding a separate 32-byte seed for each component. Removed purpose values
are rejected. Production derivation/signing and retained-key APIs refuse Network
Root; verification of the compiled public root remains available. Storage is a
node service key, not another user Identity or network role.

The IdentityAuthorization descriptor is exactly 3330 bytes:
`0x01 || recovery_ed25519[32] || recovery_ml_dsa_65[1952] || 0x01 ||
authorization_ed25519[32] || authorization_ml_dsa_44[1312]`.
These fixed suite markers exist in current bytes; they are not a selectable
historical-version decoder. Unknown markers, wrong size, wrong purposes, zero
components and identical Ed25519 recovery/authorization keys are rejected.
Commitment is SHA-256 of `"CYBOU/IDENTITY-AUTH-COMMIT" || descriptor_bytes`.
See [identity_authorization.cpp](../../src/cybou/identity_authorization.cpp).

RetainedIdentityKey may hold a role's private keys after the originating entropy
is cleared. The desktop's already active PoA signer can therefore continue after
Vault lock; lock is not a signer pause or process shutdown. The vault itself does
not store a PoA signing journal. See [finality](POA_FINALITY.md) and
[vault bytes](76_IDENTITY_VAULT_RECOVERY.md#current-portable-bytes).

Existing [role tests](../../src/test/cybou_identity_crypto_tests.cpp) cover separate
role derivation, both-signature verification and rejected removed purposes;
[descriptor tests](../../src/test/cybou_identity_authorization_tests.cpp) cover
fixed encoding, bounds and commitment vectors. Historical test names containing
"device" do not introduce a protocol Device. These sources are component evidence,
not independent interoperability or certification of the hybrid composition.
