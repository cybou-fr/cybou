# 86 — Identity security substrate

Status: canonical user-service security architecture. This document defines
the target ownership and capability model. `26_IMPLEMENTATION_STATUS.md`
records which parts are wired into the current code.

## Purpose

CYBOU Identity is the security root for user services. One stable AccountID
owns recovery authority and the lifecycle of authorized devices. Name, Wallet,
Mail, and Files are capabilities of that identity; they do not create separate
user identities or parallel authorization systems.

```text
                         CYBOU Identity
                              │
            ┌─────────────────┴──────────────────┐
            │                                    │
    Hybrid authorization                 DEV key agreement
    Ed25519 + ML-DSA-44                X-Wing (draft-05)
            │                                    │
 DeviceOperationCoordinator              Key wrapping / CEKs
            │                                    │
    ┌───────┼────────┬───────┐          ┌────────┴──────────┐
    │       │        │       │          │                   │
   Name   Wallet    Mail    Files    Mail content       Files objects
                                          + attachments
```

The Recovery Root is a separate authorization domain. Validator, Operator
Authority, Release Signing, and Treasury keys are separate system domains and
are not user-service keys.

## Identity and device key domains

| Domain | Key material | Purpose | Current status |
|---|---|---|---|
| Recovery Root | Ed25519 + ML-DSA-65 | Recover account authority and authorize recovery/device lifecycle operations | Implemented in the current identity path; encoding/vector work remains tracked by Identity docs |
| Device signing | Ed25519 + ML-DSA-44 | Authorize user-service operations and prove device possession | Implemented for current device authorization |
| Device key agreement | X-Wing (ML-KEM-768 + X25519) | Establish or wrap content keys for an authorized device | Draft-05 profile is published per active device in DEV; Mail/Files do not consume it yet |
| Validator signing | Ed25519 + ML-DSA-65 | Validator consensus signatures | Separate target domain; production signature wiring remains incomplete |

Private keys remain client-controlled. DEV Identity binds each X-Wing public
package to AccountID and device activation. Its draft-05 wire format and
publication rules are frozen for DEV in `89_IDENTITY_KEM_PUBLICATION.md`; this
does not freeze Mail application framing or make KEM-dependent services
available.

Signing keys MUST NOT be converted into or reused as Mail, Files, or Storage
encryption keys. Recovery keys are not routine service-signing keys. Key
purpose, NetworkID, operation kind, device activation, and payload commitment
must be authenticated in their respective protocols.

## Service capability model

| Capability | Authorization | Confidentiality / key ownership | Finality or durability |
|---|---|---|---|
| Name | Device hybrid signature through the coordinator | No content encryption capability | BFT finality for commit and reveal |
| Wallet | Device hybrid signature through the coordinator | No content encryption capability | BFT finality for payment and lock operations |
| Mail | Device-authorized Mail operation through the coordinator (target) | DEV Identity X-Wing capability; Mail ciphertext/profile not frozen, service disabled | BFT registration; Mail content and attachments use Storage when required |
| Files | Device-authorized manifest/root changes through the coordinator (target) | Symmetric object encryption; device key agreement wraps Files keys (target) | Storage durability contract, not chain inclusion alone |

“Hybrid” does not mean every operation uses every key. Signatures authorize
actions; KEM establishes or wraps content keys; a standard symmetric AEAD
protects bulk content. Exact PQ/T profile details belong to `49` and `88`.

## Shared authorization lifecycle

The shared coordinator is the sole allocator and signer for device-authorized
service operations. A service supplies an operation kind, payload commitment,
and operation builder. It does not read `device.next_nonce`, choose a nonce,
or sign `DeviceAuthorization` itself. The coordinator persists the exact
operation before submission and reconciles uncertain delivery before another
operation can use that device sequence. See `87_DEVICE_OPERATION_COORDINATOR.md`.

Recovery Root operations such as account recovery or root-authorized device
management remain distinct protocol actions. They use their own required root
authorization and still follow the common submission/reconciliation contract;
they are not a reason for a service to allocate a device nonce independently.

## Product surface

```text
Create or restore one Identity
        │
        ├── choose a .cybou name
        ├── use Wallet
        ├── use Mail
        └── use Files
```

The user does not create a separate Wallet account, Mail profile, or Storage
account. Files is the Beta Drive-like product surface; “Drive-like” is an
interaction reference, not a second service. Backup is a post-Beta application
of the same Storage layer.

## Implementation boundary

The current runtime implements the shared coordinator for Wallet payments,
System Balance locks, Name commit/reveal, recovery's root-authorized
DeviceAdd and DeviceRevoke, and RecoveryRotate with finality-gated vault
promotion. Mail and Files are not yet integrated. The current identity record
also lacks published KEM capabilities. Local key material does not imply an
interoperable hybrid profile or service support. These gaps must remain visible
in implementation status and UI capabilities; the architecture target is not
a claim that Beta security is complete.

## Authority and related documents

- `10_IDENTITY_NAMES.md` owns identity, recovery, device, and name protocol rules.
- `76_IDENTITY_VAULT_RECOVERY.md` owns phrase and portable-vault behavior.
- `87_DEVICE_OPERATION_COORDINATOR.md` owns operation/nonce lifecycle.
- `88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md` owns content-key and object privacy.
- `89_IDENTITY_KEM_PUBLICATION.md` owns the pending device KEM publication design and cutover gates.
- `81_BETA_PRODUCT_SCOPE.md` owns Beta readiness.
- `26_IMPLEMENTATION_STATUS.md` owns current implementation status.
