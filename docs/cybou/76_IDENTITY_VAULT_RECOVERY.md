# 76 — Identity vault and recovery

Status: CURRENT
Scope: CYBV/CYID source review at 75ba7969, 2026-10-09; existing tests referenced, not newly executed. Product restore/rotation acceptance remains separate.

## Recovery phrase

The current 24-word mnemonic encodes the recovery entropy from which current
Recovery, Authorization and X-Wing KEM roles are derived under separate domains.

AccountID remains stable across Identity rotation.

## Portable vault

CYBOU Identity Vault stores stable AccountID plus recovery entropy inside the protected
portable vault format.

The vault must be durably saved and reopened before AccountCreate broadcast.

## Basic clean restore

From the current mnemonic:

1. validate mnemonic;
2. derive current Identity roles;
3. resolve AccountID from verified finalized state;
4. verify current Recovery/Authorization/KEM commitment against current
   `key_epoch`;
5. create/reopen the local vault;
6. read canonical Balance and System Balance from finalized AccountState;
7. rebuild private Mail/Files through publication discovery.

Restore itself does not authorize a new protocol operation.

## Historical content after rotation

Old RootPublications may have recipient/self capsules addressed to old KEM
epochs. A new mnemonic does not automatically derive those old KEM seeds.

Therefore rotation that must preserve historical content recovery uses a
private `IDENTITY_RECOVERY_BRIDGE`.

Before submitting IdentityRotate:

```text
generate/confirm new mnemonic
-> derive new KEM
-> build encrypted RecoveryBridge containing required old KEM recovery material
-> capsule bridge to the new KEM
-> publish with current old Identity authorization
-> obtain PoA finality
-> reach remote durability
-> verify bridge can be opened using the new mnemonic
-> only then submit IdentityRotate
```

Recovered historical KEM seeds are accepted only after deriving their public
packages and matching the canonical historical KEM commitments.

Do not combine an unprotected bridge and irreversible rotation into one step.

## Local application state

The per-Identity Application DB and previous provider placement database are
rebuildable caches and are not required inputs for clean recovery. Available
finalized history, accessible capsules/bridges and retrievable ciphertext are
still required; a phrase does not recreate lost encrypted content.

## Current portable bytes

CYBV is the encrypted envelope. Its header is exactly 60 bytes; integers are
u32 little-endian. There is no version field or legacy decoder.

| Offset | Field | Bytes / value |
|---|---|---|
| 0 | magic | 4, CYBV |
| 4 | Argon2 memory cost | 4, 65536 KiB |
| 8 | iterations | 4, 3 |
| 12 | lanes | 4, 1 |
| 16 | salt | 16, random |
| 32 | DEK-wrap nonce | 12, random |
| 44 | payload nonce | 12, random |
| 56 | plaintext payload length | 4, 1–65536 |
| 60 | wrapped DEK | 32 ciphertext + 16 tag |
| 108 | encrypted payload | payload length + 16 tag |

Argon2id derives a 32-byte KEK from password bytes and salt using the fixed
parameters above; the implementation requests one thread. Both DEK wrapping
and payload encryption use AES-256-GCM and authenticate the complete 60-byte
header. The random 32-byte DEK encrypts the payload; the KEK encrypts the DEK.
Password API bounds are 12–1024 bytes, not Unicode character counts. Different
KDF parameters, wrong password, authentication failure, malformed length and
trailing bytes are rejected. Total envelope length is `124 + payload_length`.

IdentityMaterial's payload is exactly 68 bytes: `CYID[4] || AccountID[32] ||
recovery_entropy[32]`. Its CYBV envelope is therefore 192 bytes. It contains no
network binding, key epoch, signing journal, historical KEM seeds, app indexes
or encrypted content. Derived roles/KEM are checked when material is parsed;
matching the current network Identity is an additional restore/session check,
not guaranteed by successful vault decryption alone. CYID and CYBV name different
layers, not successive supported versions.

## File publication and retry boundary

SaveNewIdentityVault writes a random-suffixed temporary file in the same directory,
flushes it, publishes without overwriting an existing target, then authenticates
and compares the reopened payload. Load rejects non-regular files and symbolic links/reparse
points under the implemented platform checks. Promotion authenticates the candidate,
checks the expected payload and same parent directory, replaces the active file
and reopens it. Replace authenticates the expected current payload before writing
its replacement. Promotion/replacement use a per-vault file lock.

A `false` result is not a blanket rollback guarantee. A rename/publication can
succeed before a later directory sync or reopen fails. Reconcile by authenticated
read and expected payload before retrying; do not delete or regenerate material
on that result. Promotion already recognizes an absent candidate plus an exact
matching active payload as a completed retry. This does not certify crash behavior
for every filesystem, power-loss point or independent writer.

Sources: [identity_vault.cpp](../../src/cybou/identity_vault.cpp),
[identity_material.cpp](../../src/cybou/identity_material.cpp),
[keystore.cpp](../../src/cybou/keystore.cpp).
Existing [vault tests](../../src/test/cybou_identity_vault_tests.cpp) cover header/
payload tampering, bounds, no-overwrite and authenticated promotion/retry.
[Material tests](../../src/test/cybou_identity_material_tests.cpp) cover random
AccountID and recovery entropy through encrypted save/load. Post-rename I/O-failure
injection and clean GUI rotation/recovery remain separate acceptance work.
