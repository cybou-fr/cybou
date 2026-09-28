# 76 — Identity vault and recovery

Status: initial desktop creation, clean-machine device restore, and
finality-gated recovery-root rotation are integrated. Password change, vault
lock, and recovery at the active-device limit remain open.

The local `identity_crypto` module has fixed HKDF labels and deterministic
public-key test vectors. `recovery_phrase` now encodes and decodes 256-bit
entropy with the BIP-39 English list and eight checksum bits. The recovered
entropy is passed directly as the 32-byte secret to the identity HKDF; the
BIP-39 PBKDF2 wallet seed/passphrase scheme is not used. The local
`identity_vault` module seals and opens bounded in-memory CYBV2 envelopes.
`SaveNewIdentityVault` now writes a new file through a synced temporary file,
publishes it without overwriting an existing vault, and authenticates it by
reopening before returning success. The deployed DEV `CVID4` payload contains a
random AccountID, 256-bit recovery entropy, independent device signing secret,
and one 32-byte X-Wing seed, in that order after the five-byte payload magic.
The public hybrid KEM package is derived from this seed and validated before
the vault can be saved. AccountCreate and DeviceAdd publish the canonical
package and bind its commitment to the signing authorization; state retains
the commitment for each active device. The new runtime rejects CVID3 and does
not import old split X25519/ML-KEM material. Mail and cross-device Files remain
fail-closed pending their application-specific gates. See
[`89_IDENTITY_KEM_PUBLICATION.md`](89_IDENTITY_KEM_PUBLICATION.md) for the
frozen DEV profile and coordinated cutover requirements.
`IdentityMaterial`
clears all secret fields when destroyed. Password change and broader device
and vault management remain unimplemented.

The local Storage Key Ring is a separate encrypted CYBV2 sidecar bound to the
vault's AccountID. It stores contiguous random Storage Master Key epochs and
retains old epochs so prior objects remain readable. It is not distributed to
other devices or included in clean-machine restore; multi-device Files waits
for the reviewed device package and recovery flow. The target account-level
SMK envelope and clean-machine restoration contract is in
[`90_STORAGE_KEY_RECOVERY.md`](90_STORAGE_KEY_RECOVERY.md); until its
cryptographic, consensus, and Storage availability gates pass, Identity
recovery does not imply Files recovery.

An in-progress name claim is saved separately beside the identity vault as an
encrypted CYBV2 envelope. It binds NetworkID, AccountID, label, and random
salt, uses the identity vault password, and is saved and reopened before
NameCommit submission. Preserve this file until NameReveal finalizes.

## Recovery Root

Generate 256 bits from a cryptographic RNG and encode a checksummed 24-word
phrase using the BIP-39 English 2048-word list, pinned to SHA-256
`2f5eed53a4727b4bf8880d8f3f199efc90e58503646d9ff8eff3a2ed3b24dbda`.
Input is exactly 24 lowercase ASCII list words in canonical order; reject
unknown words and checksum mismatch. The 256 entropy bits plus the first eight
SHA-256 checksum bits form 24 consecutive 11-bit indices. This is BIP-39
mnemonic *encoding*, not BIP-39 PBKDF2 seed derivation. Derive independent
Ed25519 and ML-DSA-65 root seeds. RecoveryKeyID commits to the suite and both
public keys; consensus maps it to the stable random AccountID. Use root
material only for recovery and critical changes, then cleanse memory.

Local RecoveryKeyID is SHA-256 over the ASCII bytes
`CYBOU/RECOVERY-KEY-ID/V2`, bytes `02 01` (identifier version and hybrid root
suite), 32 raw Ed25519 public-key bytes, and 1952 raw ML-DSA-65 public-key
bytes, in that order. It rejects other key purposes, malformed sizes, and
all-zero public-key encodings. The consensus registry maps RecoveryKeyID to
AccountID and validates root rotation. Historical Mail authorization evidence
remains incomplete.

On a clean machine, derive the root from the phrase, find AccountID in verified
state, generate a fresh device keyset, durably save a new password-protected
vault, authorize DeviceAdd with root and new-device signatures, and wait for
finality. Loss of
all devices is recoverable if the phrase survives. Loss of devices and phrase
is unrecoverable.

### Recovery-root rotation transaction

The consensus transition already verifies the old root signature and new-root
proof of possession over the same network-bound digest. It atomically replaces
the RecoveryKeyID mapping and advances the shared root nonce. The desktop
implements recovery-root rotation around that transition using the following
transaction contract:

1. Generate a new 256-bit root entropy, derive its hybrid key and 24 words, and
   require the user to confirm the new phrase before any submission.
2. Build a password-protected candidate vault for the same AccountID and
   existing device secret, replacing only the recovery entropy. Save it to a
   distinct pending path and reopen it successfully. Keep the currently active
   vault intact.
3. Read the finalized root nonce, sign one `RecoveryRotate` with the old root,
   prove possession with the new root, and durably journal the exact operation
   bytes and OperationID before broadcast.
4. While delivery or finality is uncertain, retain both vaults and retry only
   the journaled bytes. Never select the old or new root based on an admission
   acknowledgment.
5. On verified finality, atomically promote the pending vault before clearing
   the operation journal. On a known rejection, keep the old vault active and
   discard the candidate only after rejection is durably reconciled. On a
   history conflict, preserve both vaults and require explicit recovery.

The current CYBV2 payload contains only one recovery entropy.
`PromoteIdentityVault` authenticates a same-directory candidate and provides
durable, idempotent replacement bound to the expected plaintext. The identity
service and `DeviceOperationCoordinator` now implement candidate creation,
exact `RecoveryRotate` journaling, reconciliation, finality-gated promotion,
and resume from the encrypted candidate. The Identity page confirms the new
phrase and offers resume after interruption. Automated UI acceptance across
process crashes and remote finality remains outstanding.

## Portable `CYBV2` vault

Use a versioned, length-bounded binary envelope. Its public header contains
format/suite versions, Argon2id parameters, random salt, AES-GCM nonces, and
lengths. Generate a random 256-bit DEK. Encrypt the payload with AES-256-GCM;
derive a KEK from the password using Argon2id and wrap the DEK with a distinct
nonce. Authenticate the complete header as associated data. Payload holds
recovery entropy, AccountID, current device secrets, and local metadata.

Reject unsupported versions and KDF parameters before allocation; impose
minimum security and maximum resource bounds. Wrong password, truncation, or
tampering must fail without partial output. Save via synced temporary file and
atomic replacement in the same directory. Creation must durably save and
reopen recovery material before account broadcast. Password change rewraps
the DEK without changing identity. Never log plaintext secrets.

Windows DPAPI may provide optional local unlock convenience, never the only
means to read the portable file. Obsolete DEV vaults are discarded at the
cutover and cannot define the canonical AccountID. OpenSSL 3.5 documents ML-DSA and Argon2;
pin provider behavior and verify deterministic keygen vectors before relying
on phrase recovery. See [ML-DSA](https://docs.openssl.org/3.5/man7/EVP_PKEY-ML-DSA/)
and [Argon2](https://docs.openssl.org/3.5/man7/EVP_KDF-ARGON2/).
