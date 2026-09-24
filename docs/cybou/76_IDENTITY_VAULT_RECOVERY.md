# 76 — Identity V2 vault and recovery

Status: target specification. Current desktop has no portable V2 vault or
mnemonic recovery.

The local `identity_crypto` module has fixed HKDF labels and deterministic
public-key test vectors. `recovery_phrase` now encodes and decodes 256-bit
entropy with the BIP-39 English list and eight checksum bits. The recovered
entropy is passed directly as the 32-byte secret to the Identity V2 HKDF; the
BIP-39 PBKDF2 wallet seed/passphrase scheme is not used. Vault and consensus
recovery remain unimplemented.

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

On a clean machine, derive the root from the phrase, find AccountID in verified
state, generate a fresh device keyset, authorize DeviceAdd with both root
signatures, wait for finality, and save a new password-protected vault. Loss of
all devices is recoverable if the phrase survives. Loss of devices and phrase
is unrecoverable under V2.

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
means to read the portable file. Legacy CYBK1 may be explicit DEV import input
but cannot define a V2 AccountID. OpenSSL 3.5 documents ML-DSA and Argon2;
pin provider behavior and verify deterministic keygen vectors before relying
on phrase recovery. See [ML-DSA](https://docs.openssl.org/3.5/man7/EVP_PKEY-ML-DSA/)
and [Argon2](https://docs.openssl.org/3.5/man7/EVP_KDF-ARGON2/).
