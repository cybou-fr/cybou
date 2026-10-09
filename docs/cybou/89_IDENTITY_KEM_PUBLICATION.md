# 89 — Identity KEM capability publication

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Recorded status: DEV-only X-Wing draft-05 profile. Publication belongs to Identity; Mail and Files remain unavailable until their full application integration gates pass. The profile does not automatically follow later drafts or RFCs.

## Canonical Identity record

The current Identity record contains the Recovery public key, Authorization public key, one current KEM package commitment, one shared nonce, and key_epoch. It has no installation list, activation nonce, or per-installation KEM records.

The canonical X-Wing public package is 1218 bytes: KEM ID `0x647a` in little-endian byte order, and the 1216-byte ML-KEM-768 || X25519 public key. The mnemonic-derived 32-byte KEM seed is independent from both signing roles. The local implementation validates key derivation by encapsulation/decapsulation self-test; it does not define a custom proof-of-possession primitive.

The commitment binds `CYBOU/IDENTITY-KEM-PACKAGE`, NetworkID, AccountID, key_epoch, package length, and exact canonical package bytes. The package itself is carried by AccountCreate or IdentityRotate; state retains only the commitment. Runtime lookup resolves the package for the finalized current key_epoch from canonical verified operation history and checks its commitment and source block.

## Frozen DEV profile

DEV pins IETF `draft-ietf-hpke-pq-05` and its normative hybrid KEM/HPKE dependencies. The KEM is `MLKEM768-X25519`, HPKE KEM ID `0x647a`, with ML-KEM-768 public key (1184 bytes) followed by X25519 public key (32 bytes). The HPKE suite for vectors is base mode, HKDF-SHA256 KDF ID `0x0001`, and ChaCha20Poly1305 AEAD ID `0x0003`. The profile is DEV-only. Beta/Mainnet remain disabled until separate approval and standards status review.

## Rotation and consuming services

IdentityRotate replaces the KEM package commitment with Recovery and
Authorization keys in one atomic transition and advances key_epoch.
RootPublication carries bounded recipient capsules without a public recipient
AccountID. Mail and Files consume the same DEV X-Wing capability through the
shared encrypted chunk tree; application-specific schemas remain encrypted.
Client construction, capsule scanning/opening, and recovery integration remain
product work.

No service keeps a parallel authoritative recipient-key registry. Historical operations remain necessary to prove which package and authorization key applied at a prior finalized height. Current-state lookup alone is not historical proof.

## Activation

The Identity record, AccountCreate/IdentityRotate encodings, RootPublication
capsules, and CYID vault are active on DEV. There is no legacy decoder, import
path, or dual operation decoder. A later suite change requires a new explicit
protocol decision and coordinated network cutover.
