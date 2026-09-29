# 76 — Identity vault and recovery

Status: the current Identity path uses one mnemonic-derived key set, the CVID5
payload, and finalized key-epoch restore. Password change, vault lock, and
remaining desktop security controls are separate product work.

## Recovery phrase and key derivation

Generate 256 bits from a cryptographic RNG and encode a checksummed 24-word phrase using the pinned BIP-39 English word list. Input is exactly 24 lowercase ASCII words in canonical order; reject unknown words and checksum mismatch. This is BIP-39 mnemonic encoding, not BIP-39 PBKDF2 wallet-seed derivation. The recovered 32-byte entropy feeds role-separated, domain-separated derivations for Recovery Ed25519 + ML-DSA-65, Authorization Ed25519 + ML-DSA-44, and the X-Wing KEM seed. Never reuse a signing key as a KEM key or introduce custom cryptographic primitives.

RecoveryKeyID commits to the current recovery suite and public key. Consensus maps it to the stable random AccountID. The Identity record contains the recovery key, authorization key, current KEM package commitment, one nonce, and key_epoch.

## Portable `CYBV2` vault

The encrypted vault payload is CVID5: five-byte payload magic, random AccountID (32 bytes), and recovery entropy (32 bytes). All current key roles are rederived from the entropy; no separate device secret is stored. The outer portable CYBV2 envelope uses bounded parameters, password-based key wrapping, authenticated encryption, and atomic durable file publication. Reject unsupported envelope/payload versions, malformed lengths, wrong passwords, truncation, and tampering without partial output. Do not log plaintext secrets.

Before AccountCreate broadcast, the complete vault must be durably saved and authenticated by reopening. Rotation writes a candidate CVID5 vault for the same AccountID before submission. Keep the old vault active while delivery/finality is uncertain. Promote the candidate only after verified finality. On known rejection retain the old vault. Preserve recoverable files and the exact operation journal through conflicts.

## Clean-machine restore

1. Decode and validate the 24 words.
2. Derive all three key roles and the current X-Wing public package.
3. Resolve RecoveryKeyID using verified finalized Identity state.
4. Require the derived Recovery key, Authorization key, and package commitment for that key_epoch to match finalized state.
5. Save and reopen a local CVID5 vault for the same AccountID.

Restore is local and does not change consensus state. If the phrase is from an earlier key_epoch, report that it cannot restore the current identity; do not silently rotate or import keys. If verified state is unavailable, wait for synchronization.

## Atomic Identity rotation

IdentityRotate replaces Recovery, Authorization, and KEM roles in one transition. The old recovery key authorizes the transition; the new recovery and authorization keys prove possession. The operation consumes the shared nonce and advances key_epoch. AccountID, balances, names, and account history remain stable.

The desktop confirms the new 24-word phrase, saves and reopens the candidate vault, and durably journals exact operation bytes before submission. Retry only the same bytes while outcome is uncertain. Promote the vault after verified finality and reconcile the finalized Identity record before clearing the journal. The identity coordinator contract is in [87](87_IDENTITY_OPERATION_COORDINATOR.md).

## Storage keys

Mail and Files content-encryption keys remain separate from Identity signing
keys. Clean-machine content recovery is governed by
`IDENTITY_DISCOVERY_AND_RECOVERY.md`; Identity restore alone does not imply
that encrypted content has been retrieved.
