# Validation architecture

Status: implementation for the separate network version 7. DEV cutover requires
acceptance tests and explicit agreement. [VALIDATION.md](VALIDATION.md) defines
behavior; the C++ headers define exact types and wire encoding.

## Independent paths

IdentityOperationCoordinator journals exact authorized operation bytes and
submits them to PoA. A configured ValidationService can inspect those same
bytes. PoA ignores attestations and independently executes each candidate
batch before signing. Validation never calls OperationPool::Admit, changes
canonical funds or nonces, or admits remote chunks. Only the next nonce in a
finalized Identity record is eligible for a positive attestation.

## Finalized snapshot and durable reservation

`CybouNodeRuntime::ReadFinalizedValidationSnapshot` captures NetworkID, height,
block ID, state root, state and immutable parameters under the runtime lock.
`ValidateAgainstFinalizedBase` checks the exact operation on a disposable copy
of this one state. It does not include pending operations.

The separately stored ValidationState journal binds NetworkID and NodeID,
retains one finalized base and at most 4096 `(AccountID, nonce) -> OperationID`
reservations. It writes a first reservation durably before signing. Identical
bytes are idempotent; a different operation at the same nonce conflicts. A
verified newer base replaces the old set. Corrupt or unavailable journal state
fails closed; the retained journal rejects an older or conflicting base. An
operator must preserve the journal across restart because deletion cannot be
detected from the journal alone. The snapshot lock spans validation,
reservation and signing, so finality cannot advance between those steps.

## Binding, transport and verification

NodeBinding canonically binds a dedicated Ed25519 + ML-DSA-44 service key to
an AccountID. It does not delegate Identity spending or PoA finality. At most
eight nodes may be bound per Identity, and IdentityRotate revokes all bindings.
An optional, separately proven STORAGE_PROVIDER key remains a distinct role.

CYP2 carries bounded validation request and result frames. Ingress budget is
charged before receiving the complete operation or running PQ checks. Positive
attestations sign the exact OperationID and finalized base with the bound node
key. `VerifyValidationAttestation` independently checks the binding, signature
and operation against the same finalized snapshot. `HasLocalValidation` counts
distinct trusted Accounts, not raw NodeIDs; its default policy is OFF.

The desktop has not yet wired that trust policy into a `VALIDATED` display.
Even when it does, verified PoA finality supersedes local assessment and
remote durability remains separate. There is no global provisional chain,
pending pool or speculative dependency overlay.

## Canonical contribution boundary

A separately signed ServiceEvidence validation receipt can earn a bounded
Authority point only when another Identity's exact operation is included in
the same finalized block and is independently valid against its parent.
A demonstrably false positive adds penalty debt. A timeout, conflict or
negative local result is not fraud. The immutable version 7 Authority policy
and accumulators are committed in NetworkID and canonical state.

See [IDENTITY_AUTHORITY_NETWORK_V7.md](IDENTITY_AUTHORITY_NETWORK_V7.md) for
remaining provider admission, client scheduling and security acceptance gates.
