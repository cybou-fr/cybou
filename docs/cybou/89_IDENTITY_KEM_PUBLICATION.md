# 89 — Identity KEM capability publication

Status: CURRENT
Scope: Encrypted-content/KEM and application recovery source audit at 531dc0da, 2026-10-09. Existing component regressions are identified; no fresh C++ suite, live acceptance or standards conformity is claimed.

Recorded status: DEV-only X-Wing draft-05 profile. Publication belongs to Identity. Mail/Files construction, scanning and recovery are implemented components; clean desktop and independent interoperability acceptance remain separate gates. The profile does not automatically follow later drafts or RFCs.

## Canonical Identity record

The current Identity record contains the Recovery public key, Authorization public key, one current KEM package commitment, one shared nonce, and key_epoch. It has no installation list, activation nonce, or per-installation KEM records.

The canonical X-Wing public package is 1218 bytes: KEM ID `0x647a` in little-endian byte order, and the 1216-byte ML-KEM-768 || X25519 public key. The mnemonic-derived 32-byte KEM seed is independent from both signing roles. The local implementation validates key derivation by encapsulation/decapsulation self-test; it does not define a custom proof-of-possession primitive.

The commitment is SHA-256 of the exact concatenation `"CYBOU/IDENTITY-KEM-PACKAGE" || 0x00 || NetworkBinding[32] || AccountID[32] || key_epoch_u64le || package_length_u16le || canonical_package_bytes`. The zero separator is present here; do not infer it for other domains. The package itself is carried by AccountCreate or IdentityRotate; state retains only the commitment. Runtime lookup resolves current or historical epochs from canonical verified operation history and checks the source operation; current-epoch lookup also compares its commitment with the current Identity record. Future epochs are refused.

## Frozen DEV profile

DEV pins IETF `draft-ietf-hpke-pq-05` and its normative hybrid KEM/HPKE dependencies. The KEM is `MLKEM768-X25519`, HPKE KEM ID `0x647a`, with ML-KEM-768 public key (1184 bytes) followed by X25519 public key (32 bytes). The adopted vector-suite target is HPKE base mode, HKDF-SHA256 KDF ID `0x0001`, and ChaCha20Poly1305 AEAD ID `0x0003`; this does not describe the deployed CYBOU capsule wire. The implemented capsule uses the [CYBOU wrapper transcript](ROOT_PUBLICATION.md#recipient-capsule). The profile is DEV-only. Beta/Mainnet remain disabled until separate approval and standards status review.

## Rotation and consuming services

IdentityRotate replaces the KEM package commitment with Recovery and
Authorization keys in one atomic transition and advances key_epoch.
RootPublication carries bounded recipient capsules without a public recipient
AccountID. Mail and Files consume the same DEV X-Wing capability through the
shared encrypted chunk tree; application-specific schemas remain encrypted.
PublicationService constructs and self-capsules encrypted publications;
ApplicationService opens capsules, rebuilds Mail/Files projections and imports
RecoveryBridge historical seeds today. Their clean desktop/live acceptance is
not implied by those component implementations.

No service keeps a parallel authoritative recipient-key registry. Historical operations remain necessary to prove which package and authorization key applied at a prior finalized height. Current-state lookup alone is not historical proof.

## Activation

The Identity record, AccountCreate/IdentityRotate encodings, RootPublication
capsules, and CYID vault are active on DEV. There is no legacy decoder, import
path, or dual operation decoder. A later suite change requires a new explicit
protocol decision and coordinated network cutover.

## Source evidence and remaining review

[identity_kem.h](../../src/cybou/identity_kem.h),
[identity_kem.cpp](../../src/cybou/identity_kem.cpp) and
[node_runtime_chain.cpp](../../src/cybou/node_runtime_chain.cpp) define package
encoding, exact commitment and historical lookup. KEM seed derivation uses
HKDF-SHA256 with recovery entropy as input, salt `"CYBOU/IDENTITY/KEM"`, info
`"X-Wing recipient key seed"`, output 32 bytes.

[Existing KEM tests](../../src/test/cybou_identity_kem_tests.cpp) contain round
trips and vectors explicitly sourced to concrete-hybrid-KEM drafts 03 and 04.
They do not establish full equivalence to the adopted HPKE draft-05 profile,
a later standard, or independent end-to-end capsule interoperability.
That construction/vector review remains open; no wire/domain/key change is
introduced by this documentation correction.
