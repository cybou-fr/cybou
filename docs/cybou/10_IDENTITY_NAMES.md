# Identity and `.cybou` names

Status: canonical Identity and name protocol contract. Identity keys derive from a 24-word recovery phrase; AccountID is a separate random, permanent identifier.

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

The AccountCreate authorization descriptor is fixed-size and versioned inside its wire encoding. It carries the recovery and authorization public keys. The Identity registry stores those keys, the current KEM package commitment, one nonce, and one key_epoch. No machine installation, device list, activation number, or per-device nonce is a protocol concept.

## Recovery and key rotation

A clean-machine restore derives all current key roles from the phrase, resolves RecoveryKeyID in verified state, and checks the derived recovery key, authorization key, and KEM package against the current Identity record. It then writes and reopens a portable CYBV2 vault locally. Restore does not submit an authorization operation or alter consensus state.

IdentityRotate is one atomic operation. It binds NetworkID, AccountID, the current nonce and key_epoch, the next key_epoch, the new recovery and authorization public keys, and the new KEM package. The old recovery key signs the transition; the new recovery and authorization keys prove possession. Finalization replaces all roles and the package commitment together, increments the shared nonce, advances key_epoch, and updates the RecoveryKeyID index. AccountID and account-owned state remain unchanged. A phrase from an earlier key epoch cannot authorize the current account after rotation.

User-authorized operations use the current authorization key, current key_epoch, and shared nonce. One durable coordinator serializes operations and journals exact bytes before submission. It never signs a replacement while delivery is uncertain.

## Mail key capability

Mail encryption keys are separate derived roles. Identity publishes one X-Wing package per account/key_epoch; the commitment is bound to NetworkID, AccountID, key_epoch, and canonical package bytes. Initial Mail is one recipient, text-only, and uses one recipient capsule. The selected draft-05 X-Wing profile is DEV-only. Device registration and per-device key distribution are not protocol concepts.

## Names

The primary example is `stanislav.cybou`. A label is 5–32 lowercase ASCII bytes and follows the grammar and reserved-name rules in the [name registry](77_CYBOU_NAME_REGISTRY.md). Names finalize through commit, work, and reveal. The desktop flow is specified in the [Identity UX contract](78_IDENTITY_DESKTOP_UX.md).

## Network cutover

The canonical state joins monetary accounts, the Identity registry, and name ownership under one state root. Blocks are verified against the root-authorized PoA key assigned to their Authority epoch and height; validator sets are not state. Identity layout and wire encodings are canonical and unversioned at the source/API level. Wire-format version bytes remain inside serialized formats. A new official network generation destroys the entire old network-bound domain, including vault, AccountID, Recovery/Authorization/KEM roles, Names, Wallet and Application DB; there is no Identity migration, compatibility decoder, automatic import, or dual operation path.
