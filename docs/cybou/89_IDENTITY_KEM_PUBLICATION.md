# 89 — Identity KEM capability publication

Status: protocol design gate; not implemented and not a frozen wire format.
This document defines the shape and security requirements for publishing
device encryption capabilities through the canonical Identity registry. It
does not select a hybrid KEM combiner or authorize Mail/Files encryption.

## Current boundary

The current fixed `IdentityAuthorization` descriptor binds the Recovery Root
and initial device signing keys. `IdentityDevice` stores a device signing key,
operation nonce, and activation nonce. `DeviceAdd` binds the new device
signing-key ID; neither account creation nor device-add state publishes a
KEM key. The CVID3 local vault already stores independent X25519 and ML-KEM-768
private material, but that does not make either key discoverable or usable by
another client.

Do not append KEM material to `IdentityAuthorization` or silently change an
existing operation or state decoder. The eventual change is part of the
planned direct DEV cutover after the integrated PQ consensus and name gate.
There is no legacy decoder, import, or dual-operation path.

## Ownership and canonical records

Identity remains the only authority for device encryption capabilities.
Mail, Files, and Storage consume the capability published for an authorized
device; they do not maintain a second authoritative key registry.

The target state record adds one commitment reference to each active device:

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

The package is scoped to NetworkID, AccountID, device signing-key ID, and
device activation. Re-adding a signing key creates a new activation and
requires a new package commitment. A prior activation's package cannot
authorize encryption to the new activation.

The eventual canonical package must encode, with strict lengths and bounded
allocation:

- package format identifier and format version;
- the reviewed KEM profile identifier;
- the public key component identifiers and exact public key bytes;
- any profile-required key identifiers or validity fields.

The package contains no private keys, recovery entropy, Storage Master Key,
or content key. Do not include fields that are not required by the selected
standard profile. Exact identifiers, byte order, limits, and wire version
remain a separate freeze review.

## Commitment and authorization requirements

The eventual package commitment must hash an unambiguous canonical encoding
and bind its context. Its reviewed preimage must include a dedicated domain
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

The architecture target remains separate X25519 and ML-KEM-768 device
capabilities. This names algorithms, not a CYBOU hybrid KEM, combiner,
encapsulation transcript, or Mail HPKE profile. Reuse a finalized standard
profile and its exact encodings when available; do not define a local hybrid
combiner or infer persistent package semantics from a TLS key-share format.

Before implementation, freeze and review together:

1. the standardized hybrid KEM / HPKE profile and implementation APIs;
2. package and operation encodings, size bounds, IDs, and domain separation;
3. initial AccountCreate and DeviceAdd signatures over the same commitment;
4. state-root and snapshot serialization changes;
5. KEM rotation, revocation, activation, and historical-proof rules;
6. vault durability, restore, key-pair self-test, and service fail-closed UX;
7. test vectors, adversarial validation, and the coordinated DEV cutover plan.

Until all gates pass, Mail and cross-device Files key wrapping remain
unavailable. Local key generation or successful standalone KEM tests do not
change that status.

## Related authority

- `10_IDENTITY_NAMES.md` — canonical Identity and device rules.
- `49_EMAIL_E2EE_HPKE_PQ.md` — Mail encryption profile and evidence.
- `76_IDENTITY_VAULT_RECOVERY.md` — portable vault, creation, and restore.
- `86_IDENTITY_SECURITY_SUBSTRATE.md` — key ownership and service boundary.
- `88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md` — object and content-key model.
- `26_IMPLEMENTATION_STATUS.md` — implementation status and integration gaps.
