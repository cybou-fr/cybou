# Identity and `.cybou` names

Status: CURRENT
Scope: Identity operation/registry wire source audit at 4b606ce1, 2026-10-09. Name grammar, full coordinator/product and standards acceptance remain separately scoped.

Recorded status: canonical Identity and name protocol contract. Identity keys derive from a 24-word recovery phrase; AccountID is a separate random, permanent identifier.

## Identity model

| Field | Meaning | Lifecycle |
| --- | --- | --- |
| AccountID | Random nonzero 256-bit stable account identifier | Never changes |
| Recovery key | Mnemonic-derived Ed25519 + ML-DSA-65 key | Replaced atomically by IdentityRotate |
| Authorization key | Mnemonic-derived Ed25519 + ML-DSA-44 key | Replaced atomically by IdentityRotate |
| Identity KEM capability | Mnemonic-derived X-Wing recipient key package | Replaced atomically by IdentityRotate |
| key_epoch | Current key-set generation | Incremented by IdentityRotate |
| nonce | One account-wide Identity operation sequence | Incremented by every authorized operation and rotation |
| Primary `.cybou` name | Human-facing alias bound to AccountID | No transfer or recycling initially |

AccountID is independent of mnemonic and public keys. RecoveryKeyID indexes the current recovery key to AccountID. Rotating the phrase and all key roles preserves AccountID, balances, names, and access to content for which the new key set is authorized. A name is a pseudonymous alias, not a civil identity assertion.

The 24-word phrase derives each key role with a separate domain label. Recovery signatures require Ed25519 and ML-DSA-65. Authorization signatures require Ed25519 and ML-DSA-44. The X-Wing seed is derived under its own domain and is never reused for signing. Failed, malformed, or missing signature components fail closed. No custom cryptographic primitive or classical-only production fallback is allowed.

The AccountCreate authorization descriptor is fixed-size with one canonical wire encoding. It carries the recovery and authorization public keys. The Identity registry stores those keys, the current KEM package commitment, one nonce, and one key_epoch. No machine installation, device list, activation number, or per-device nonce is a protocol concept.

## Recovery and key rotation

A clean-machine restore derives all current key roles from the phrase, resolves RecoveryKeyID in verified state, and checks the derived recovery key, authorization key, and KEM package against the current Identity record. It then writes and reopens a portable CYBV vault locally. Restore does not submit an authorization operation or alter consensus state.

IdentityRotate is one atomic operation. It binds NetworkBinding, AccountID, the current shared nonce and next key_epoch (current epoch is checked from finalized state), the new recovery and authorization public keys, and the new KEM package. The old recovery key signs the transition; the new recovery and authorization keys prove possession. Finalization replaces all roles and the package commitment together, increments the shared nonce, advances key_epoch, and updates the RecoveryKeyID index. AccountID and account-owned state remain unchanged. A phrase from an earlier key epoch cannot authorize the current account after rotation.

User-authorized operations use the current authorization key, current key_epoch, and shared nonce. One durable coordinator serializes operations and journals exact bytes before submission. It never signs a replacement while delivery is uncertain.

## Mail key capability

Mail encryption keys are separate derived roles. Identity publishes one X-Wing package per account/key_epoch; the commitment is bound to NetworkBinding, AccountID, key_epoch, and canonical package bytes. Current Mail uses one recipient plus an owner self capsule for Sent/recovery; attachments use the shared encrypted-tree substrate. Multi-recipient To/Cc/Bcc remains a separate product target. The selected draft-05 X-Wing profile is DEV-only. Device registration and per-device key distribution are not protocol concepts.

## Names

The primary example is `stanislav.cybou`. A label is 5–32 lowercase ASCII bytes and follows the grammar and reserved-name rules in the [name registry](77_CYBOU_NAME_REGISTRY.md). Names finalize through commit, work, and reveal. The desktop flow is specified in the [Identity UX contract](78_IDENTITY_DESKTOP_UX.md).

## Network cutover

The canonical state joins monetary accounts, the Identity registry, and name ownership under one state root. Blocks are verified against the genesis-authorized PoA key. Identity layout and wire encodings are canonical and unversioned in both the API and serialized layouts. A new official network cutover (with a new Network Public Key, new NetworkID, and new genesis) destroys the entire old network-bound domain, including vault, AccountID, Recovery/Authorization/KEM roles, Names, Wallet and Application DB; there is no Identity migration, compatibility decoder, automatic import, or dual operation path.

## Current operation encoding

Integers below are little-endian; IDs are raw 32-byte values. Each complete
ProtocolOperation starts with a one-byte type tag, followed by its body, without
another body-length prefix. Blocks/transport supply their own lengths. External
tags are AccountCreate=1, Payment=2, IdentityRotate=3, SystemLock=4, NameCommit=5,
NameReveal=6, RootPublication=7, RevokePublication=8, StorageLease=9,
StorageSettlement=10. Unknown tags and trailing bytes are rejected.

### AccountCreate

The body is exactly 10549 bytes (10550 with the external tag), in this order:

| Field | Bytes |
|---|---|
| AccountID | 32 |
| IdentityAuthorization descriptor | 3330 |
| IdentityKemPackage | 1218 |
| AccountCreationWork | 112 |
| recovery proof of possession | Ed25519 64 + ML-DSA-65 3309 |
| authorization proof of possession | Ed25519 64 + ML-DSA-44 2420 |

Work is NetworkBinding[32], AccountID[32], authorization commitment[32],
work_epoch u64, work nonce u64. Both AccountID copies must match in validation.
Let A be the descriptor commitment and K the KEM package commitment for epoch 0.
The work's authorization commitment is SHA-256 of
`"CYBOU/ACCOUNT-AUTHORIZATION" || A || K`.
Both proofs of possession sign the same SHA-256 digest of
`"CYBOU/ACCOUNT-POP" || NetworkBinding || AccountID || authorization_commitment`.
Work hash is SHA-256 of `"CYBOU/ACCOUNT-CREATE-WORK" || exact_work_bytes`.
There is no extra separator in these three transcripts.

Validation checks format, enabled KEM profile, network/account/commitment,
work epoch, work difficulty and both hybrid proofs. In current DEVNET work
requires 25 leading zero bits; `epoch = floor(block_height / 1024)` and work
may be from the current or preceding epoch (lag 1), never a future epoch. Lag
is an allowed age window, not a mandatory one-epoch waiting period. Registry
and state execution additionally check uniqueness, limits and Treasury funding.
Success initializes nonce and key_epoch to zero. Creating an Identity does not
require global peer freshness and is not finalized merely by serialization.

### Shared service-operation authorization

The prefix is exactly 2565 bytes: AccountID[32], nonce u64, current key_epoch u64,
kind u8, payload commitment[32], Ed25519 signature[64], ML-DSA-44 signature[2420].
It precedes the operation-specific payload inside the external tagged body.
Internal kinds are Payment=1, SystemLock=2, NameCommit=3, NameReveal=4,
RootPublication=5, RevokePublication=6, StorageLease=7. These are not the external
tags; matching both to the expected operation is required.

The signed digest is SHA-256 of
`"CYBOU/IDENTITY-OP-DIGEST" || "CYBOU/IDENTITY-OP" || NetworkBinding ||
AccountID || nonce_u64le || current_key_epoch_u64le || kind_u8 || payload_commitment`.
Both literal domains are present, with no separator. NetworkBinding participates
in the digest; it is not a separate field in this prefix. Execution checks the
payload commitment, exact current nonce/epoch and both Authorization signatures.
Only an accepted transition advances the shared nonce. The prefix is not a
canonical pending-state record or an independent permission for arbitrary bytes.

### IdentityRotate

The body is exactly 13824 bytes (13825 with the external tag): AccountID[32],
new Recovery public key (32+1952), new Authorization public key (32+1312),
new KEM package[1218], current shared nonce u64, next key_epoch u64, old Recovery
signature (64+3309), new Recovery proof (64+3309), new Authorization proof
(64+2420). Public keys here have fixed sizes and no descriptor suite markers.
There is no separately encoded old epoch or generic service-authorization prefix.

Let R/A be the new Recovery/Authorization key IDs and K the new package commitment
at the next epoch. All three signatures cover SHA-256 of
`"CYBOU/IDENTITY-ROTATE-DIGEST" || "CYBOU/IDENTITY-ROTATE" || NetworkBinding ||
AccountID || nonce_u64le || next_key_epoch_u64le || R || A || K`.
Both domains are exact existing bytes; no separator or signature bytes are added.
The registry requires current nonce, next epoch exactly current+1, no exhausted
counter and an unclaimed new RecoveryKeyID. Success atomically replaces the three
roles, updates the RecoveryKeyID index and increments the shared nonce. Parsing
alone does not verify signatures, current-state context or recovery durability.
A RecoveryBridge is an application prerequisite for historical content, not a
field or enforced durability proof inside IdentityRotate consensus execution.

## Registry snapshot and defining sources

IdentityRegistry bytes are u32 account count followed by strictly AccountID-ordered
3408-byte records: AccountID[32], Recovery key (32+1952), Authorization key
(32+1312), KEM commitment[32], nonce u64, key_epoch u64. Key purposes are implicit
from position; no descriptor suite bytes or public KEM package are stored here.
The RecoveryKeyID index is rebuilt and checked, not serialized as a second map.

Sources: [account_creation.h](../../src/cybou/account_creation.h),
[account_creation.cpp](../../src/cybou/account_creation.cpp),
[protocol_operation.h](../../src/cybou/protocol_operation.h),
[protocol_operation.cpp](../../src/cybou/protocol_operation.cpp),
[identity_registry.cpp](../../src/cybou/identity_registry.cpp), and
[identity_registry_codec.cpp](../../src/cybou/identity_registry_codec.cpp).
Existing [AccountCreate tests](../../src/test/cybou_account_creation_tests.cpp)
cover work/POP context and malformed bytes;
[registry tests](../../src/test/cybou_identity_registry_tests.cpp) cover nonce
replay, rotation and snapshot round trip;
[state tests](../../src/test/cybou_state_tests.cpp) cover rotation wire/execution
and Treasury flows. These are identified component regressions, not a fresh test
run, independent implementation vectors or clean GUI/live acceptance.
