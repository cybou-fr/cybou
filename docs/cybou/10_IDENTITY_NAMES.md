# 10 — Identity V2 and `.cybou` names

Status: frozen target for the next disposable DEV protocol break. Current DEV
still runs Ed25519-only AccountCreateOpV1, one active authorization key, and
the CYBK1 local keystore. Nothing on this page claims V2 is deployed.

## Identity layers

| Layer | Meaning | Rotation |
|---|---|---|
| AccountID | Random nonzero 256-bit permanent consensus identifier | Never |
| Primary `.cybou` name | Human-facing alias bound to AccountID | No transfer or recycling in V1 |
| Recovery Root | Replaceable hybrid root authorization recovered from 24 words | Yes |
| Device authorization | Bounded set of hybrid operational keys with independent nonces | Add/revoke |
| Mail encryption keys | Recipient device confidentiality keys | Yes |

AccountID is generated independently of every mnemonic, public key, and
authorization descriptor. Consensus binds a versioned RecoveryKeyID to the
AccountID. Rotating root or device keys cannot change the AccountID, name,
balances, or finalized mail history. A `.cybou` name is a pseudonymous alias,
not a civil identity assertion.

Recovery Root V2 requires both Ed25519 and ML-DSA-65 signatures. Daily device
authorization requires both Ed25519 and ML-DSA-44 signatures. Missing,
malformed, or failed components fail closed; V1 signatures cannot be parsed as
V2. Domain-separated signed bytes, canonical ordering, size bounds, and suite
identifiers are mandatory before consensus activation. Mail confidentiality
uses a separate X25519 + ML-KEM-768 target profile; signing keys are not
encryption keys. See [vault and recovery](76_IDENTITY_VAULT_RECOVERY.md).

The local `IdentityAuthorizationV2` draft has one canonical 3331-byte form:
`02` version, `01` root suite, 32 Ed25519 root public-key bytes, 1952
ML-DSA-65 root public-key bytes, `01` device suite, 32 Ed25519 device
public-key bytes, and 1312 ML-DSA-44 device public-key bytes. It rejects
other versions, lengths, purposes, all-zero keys, and reuse of the same
Ed25519 public key for root and device. Its commitment is SHA-256 of ASCII
`CYBOU/IDENTITY-AUTH-COMMIT/V2` followed by those exact 3331 bytes. This
format is implemented locally but is not yet a consensus operation.

DeviceAdd, DeviceRevoke, and RecoveryRotate are versioned operations. Each
device has its own nonce. Revocation changes future authorization and does
not erase already received ciphertext or historical signatures. Historical
authorization proofs remain available for MailEvidenceBundle verification.
The local V2 registry prototype computes DeviceKeyID as SHA-256 over ASCII
`CYBOU/DEVICE-KEY-ID/V2`, the two bytes `02 01`, and the Ed25519 and
ML-DSA-44 device public keys. It limits active devices to eight. Root-signed
add/revoke/rotate requests bind the network, AccountID, root nonce, and target
key ID to distinct SHA-256 domains. This prototype is not yet consensus state.
The local device-operation authorization binds NetworkID, AccountID,
DeviceKeyID, device nonce, activation nonce, operation kind, and the canonical
payload commitment. Re-adding a revoked key assigns a new activation nonce,
so signatures from its earlier activation cannot be replayed. Payment and Mail
payload encodings and their atomic state transitions are still pending.
The standalone registry snapshot uses version byte `02`, a little-endian
account count, then ascending AccountID records. Each record stores AccountID,
raw hybrid root public keys, root nonce, device count, and ascending DeviceKeyID
entries with raw hybrid device keys, device nonce, and activation nonce.
RecoveryKeyID and DeviceKeyID are derived during decoding; they are not trusted
from the snapshot. It feeds the standalone V2 state envelope described below;
neither component is active in DEV.
The standalone `CybouStateV2` prototype now wraps that registry with monetary
account fields, network pools, and the validator set, using version byte `02`
and `CYBOU/STATE/V2` for its hash. It validates that every monetary AccountID
has exactly one identity record. Names and operation dispatch are not yet in
that prototype, so it is not the final active state format.
The local Payment V2 payload is `02` version, 32 recipient AccountID bytes,
and an eight-byte little-endian amount. Its SHA-256 commitment uses
`CYBOU/PAYMENT-PAYLOAD/V2`; the device signature binds that commitment and
the payment kind. The transition consumes the signer's device nonce while
updating balances and the pending fee pool in the same candidate V2 state.

The target primary example is `stanislav.cybou`. The earlier frozen
`stan.cybou` example (DEC-004) is superseded by Identity V2's five-character
minimum. Name grammar, reservations, work, and ordering are defined in
[the name registry](77_CYBOU_NAME_REGISTRY.md). Desktop create/restore
semantics are in [the UX contract](78_IDENTITY_DESKTOP_UX.md).

## Migration boundary

Identity V2 introduces new operation and state versions; V1 fields are never
silently reinterpreted. Local crypto, phrase, and vault code can land without a
network reset. Once V2 authorization, account creation, and name registry are
integrated together, advance the network definition and perform one intentional
CYBOU-DEV reset. Beta/Mainnet genesis and economic parameters remain separate.
