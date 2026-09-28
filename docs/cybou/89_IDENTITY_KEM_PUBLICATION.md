# 89 — Identity KEM capability publication

Status: the DEV draft-05 profile and protocol implementation are frozen and
deployed. The active DEV NetworkID is
`dfba1f18efba06839d3b4cc9e1fb8c90e9065583f1de0a03809ea7004d3c4e65`; its
network definition explicitly enables X-Wing publication. Beta and Mainnet
remain disabled by default. Mail and Files remain disabled until their separate
application gates pass. The old DEV genesis/state is archived for rollback but
is not accepted by the new network format.

## Current boundary

The fixed `IdentityAuthorization` descriptor binds the Recovery Root and
initial device signing keys. `IdentityDevice` stores a device signing key,
KEM package commitment, operation nonce, and activation nonce. AccountCreate
and DeviceAdd carry the canonical package and bind its commitment to signing
authorization. CVID4 stores the 32-byte X-Wing seed. The old CVID3 split
X25519/ML-KEM material is rejected and cannot be imported as this hybrid key.

KEM capability is not appended to `IdentityAuthorization`; it is carried by
the versioned canonical package fields. Operation, network-definition,
registry-state, and global-state encodings move directly to the new format.
The deployed DEV network remains untouched until the integrated PQ consensus
and name gate is ready. There is no legacy decoder, import, or dual-operation
path.

## Ownership and canonical records

Identity remains the only authority for device encryption capabilities.
Mail, Files, and Storage consume the capability published for an authorized
device; they do not maintain a second authoritative key registry.

The state record adds one commitment reference to each active device:

```text
IdentityDevice {
    signing_key
    kem_package_id
    next_nonce
    activation_nonce
}
```

`KEMPackageID` is a domain-separated digest of the exact canonical public
package bytes and their context. The finalized AccountCreate or DeviceAdd
operation carries those bounded bytes; consensus state stores the commitment,
not a second copy of the package. Clients discover the package from finalized
operation history and verify it against the active state commitment. A cache
or local index may accelerate lookup but is never authoritative.

`CybouNodeRuntime::FindActiveIdentityKemPackage` resolves an active package by
AccountID and DeviceKeyID. It reads the canonical active registry commitment,
scans the node's verified finalized history for the matching activation, and
returns the package, commitment, finalized state root/height, and source block
height/index/ID. A client can fetch that source block with
`GetBlockAtHeight` and verify its finality certificate independently before
using the bytes. A missing/mismatching active publication or broken local
history fails closed. `FindHistoricalIdentityKemPackage` resolves a specific
activation even after revocation; it returns the same source location so the
historical operation and its authorization can be checked against the
finalized block. These lookups are local-history APIs; the existing finalized
block feed remains the transport for remote clients.

The package is scoped to NetworkID, AccountID, device signing-key ID, and
device activation. Re-adding a signing key creates a new activation and
requires a new package commitment. A prior activation's package cannot
authorize encryption to the new activation.

The canonical package encodes, with strict lengths and bounded
allocation:

- package format identifier and format version;
- the reviewed KEM profile identifier;
- the public key component identifiers and exact public key bytes;
- any profile-required key identifiers or validity fields.

The package contains no private keys, recovery entropy, Storage Master Key,
or content key. Do not include fields that are not required by the selected
standard profile. Exact identifiers, byte order, limits, and wire version
are frozen below for the DEV cutover.

## Frozen DEV KEM and package profile

DEV pins the following draft set and does not follow newer draft revisions
automatically:

- IETF `draft-ietf-hpke-pq-05`;
- its normative IRTF dependencies `draft-irtf-cfrg-concrete-hybrid-kems-03`
  and `draft-irtf-cfrg-hybrid-kems-12`;
- its normative HPKE base dependency `draft-ietf-hpke-hpke-03`;
- NIST FIPS 203 and FIPS 202, plus RFC 7748.

The DEV KEM is `MLKEM768-X25519`, HPKE KEM ID `0x647a`. It is the
standardized-in-draft X-Wing construction: ML-KEM-768 public key (1184 bytes)
followed by X25519 public key (32 bytes), total 1216 bytes. The KEM seed and
decapsulation key are 32 bytes; ciphertext is 1120 bytes; shared secret is
32 bytes. Implementations must follow the pinned drafts and their test
vectors; no local combiner or independent component keys are allowed.

The initial HPKE suite used for DEV interoperability vectors is base mode,
KEM `0x647a`, HKDF-SHA256 KDF ID `0x0001`, and ChaCha20Poly1305 AEAD ID
`0x0003` (draft-05 Appendix A.5). This freezes a test suite, not permission to
enable Mail or Files. HPKE authenticated mode is unsupported by this KEM.

Canonical public package v1 is exactly 1219 bytes:

```text
format_version     u8       0x01
kem_id             u16 LE   0x647a
encapsulation_key  1216 B   ML-KEM-768 key || X25519 key
```

There are no optional fields, validity timestamps, extra key IDs, or trailing
bytes. Unknown versions and KEM IDs are rejected. Public-key validation and
the selected implementation's keypair self-test must pass before publication.
The active DEV network is the only network allowed to accept this profile;
Beta and Mainnet parameters must not enable it by inheriting DEV defaults.

The package commitment is SHA-256 over the following exact byte string:

```text
"CYBOU/IDENTITY-KEM-PACKAGE/V1" || 0x00
|| NetworkID[32] || AccountID[32] || DeviceKeyID[32]
|| ActivationNonce[u64 LE] || PackageLength[u16 LE] || PackageBytes[1219]
```

AccountCreate and DeviceAdd carry package v1 bytes. The account creation work
commitment and both account-creation signatures bind a domain-separated
authorization commitment containing the IdentityAuthorization commitment
and this KEM package commitment. DeviceAdd's root signature and device proof
of possession bind the same package commitment and the activation derived from
its root nonce. Consensus state stores the package commitment beside each
active device; finalized operations remain the source of package bytes.

The portable vault moves directly to CVID4 in the same DEV cutover. Its device
KEM secret is the 32-byte X-Wing seed; the public package is derived from that
seed and validated before durable save. CVID3 is not imported or decoded by
the new runtime. This follows the direct-cutover rule: obsolete DEV state and
vaults are discarded when the integrated PQ consensus and name gate is ready.

## Commitment and authorization requirements

The package commitment hashes an unambiguous canonical encoding and binds its
context. Its frozen preimage includes a dedicated domain
separator, NetworkID, AccountID, device signing-key ID, activation nonce,
package length, and exact package bytes. It must reject null identities,
unknown profile identifiers, malformed public keys, non-canonical encodings,
trailing bytes, and sizes above the protocol limit.

For initial AccountCreate, the Recovery Root and initial-device signing-key
proofs must authenticate the package commitment as part of the account
creation authorization. For DeviceAdd, both the Recovery Root signature and
the new device's hybrid signing-key proof of possession must authenticate the
same package commitment, device identity, and activation context. This keeps
the advertised package inseparable from the account and device activation it
belongs to.

The signing-key proof demonstrates control of the device authorization key;
it does not prove possession of a KEM private key. The desktop must derive the
public KEM key from the private material it is about to save and perform the
selected standard's local keypair self-test before making a package eligible
for publication. Do not invent a consensus KEM proof-of-possession scheme.

## Lifecycle and historical verification

- **Create:** save and reopen the complete portable vault before AccountCreate
  broadcast. The account-create operation binds the initial device's package.
- **Add or restore:** construct a fresh device activation and package; save and
  reopen the vault before submitting DeviceAdd; activate only after finality.
- **Rotate KEM material:** publish a replacement under explicit
  root-authorized, nonce-protected semantics. The state transition must replace
  the active commitment atomically and preserve the prior finalized operation
  for historical verification. The exact operation kind and authorization
  shape remain open for review.
- **Revoke device:** remove its active capability at finality. Historical
  package bytes and authorization evidence remain available to verify old
  ciphertext and evidence; revocation excludes the device from future
  recipient selection.
- **Rotate signing key:** treat the new signing key as a new activation and
  require a package bound to that activation.

Mail evidence and future Files sharing evidence must identify the historical
device activation and prove that the package commitment was authorized at the
relevant finalized height. Current registry state alone is not sufficient
after rotation or revocation. A recipient must fail closed if the package is
missing, its commitment does not match, its activation is inactive, or its
profile is unsupported. No silent downgrade is permitted.

## Cryptographic profile gate

The DEV KEM profile is pinned above. It does not freeze a Mail ciphertext
transcript, recipient privacy mechanism, or Files key-wrapping protocol. Do
not infer those application semantics from the HPKE vectors or a TLS key-share
format.

The implementation enables publication only when the network definition's
frozen DEV profile flag is set. The new network format is deployed to DEV.
Core tests cover the pinned X-Wing public-key vector, encapsulation/decapsulation
agreement, authorization binding, state persistence, CVID4 vault behavior, and
runtime package lookup for AccountCreate, DeviceAdd, and revoked activations.

Mail and cross-device Files remain disabled until their remaining gates pass:

1. client-side verification of the source finalized block's BFT certificate
   and historical authorization evidence when consuming a returned package;
2. Mail ciphertext transcript, recipient privacy, and Files key-wrapping
   profiles, with independent interoperability and adversarial review.

Identity KEM publication is enabled on DEV. Mail and cross-device Files key
wrapping remain unavailable until their application gates pass. Draft-05 is
DEV-only; its eventual RFC or replacement requires a separately reviewed
network upgrade and never an automatic switch.

## Related authority

- `10_IDENTITY_NAMES.md` — canonical Identity and device rules.
- `49_EMAIL_E2EE_HPKE_PQ.md` — Mail encryption profile and evidence.
- `76_IDENTITY_VAULT_RECOVERY.md` — portable vault, creation, and restore.
- `86_IDENTITY_SECURITY_SUBSTRATE.md` — key ownership and service boundary.
- `88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md` — object and content-key model.
- `26_IMPLEMENTATION_STATUS.md` — implementation status and integration gaps.
