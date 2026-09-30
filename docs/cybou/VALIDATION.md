# CYBOU Validation

Status: implementation for a separate network version; DEV cutover is pending
acceptance tests and explicit agreement. Dedicated bound-node signing, durable
nonce reservations, CYP2 attestation requests, and canonical contribution receipts
are implemented. Desktop trust-policy wiring remains an implementation gate.

## 1. Meaning and boundary

The intended application meaning is:

```text
SUBMITTED -> VALIDATED -> FINALIZED
                local         PoA canonical
                pre-finalized
```

`VALIDATED` means a node's local policy accepts enough signed validation
attestations for a still non-canonical operation. It never means that an
operation is included, irreversible, or authorized for remote storage.
`FINALIZED` is reached only through the active PoA finalizer and independently
verified finalized blocks.

These invariants are fixed:

- PoA remains the only finalizer and the only source of canonical ordering.
- Every full node independently verifies PoA certificates, executes finalized
  operations, and checks the resulting state root.
- Validation does not change `CybouState`, canonical balances, nonces, names,
  Authority, budgets, or `StateStore::CommitFinalizedBlock()` semantics.
- Validation cannot authorize remote chunk admission. Providers still require
  a finalized `RootPublication` and its valid chunk-authorization proof.
- No BFT, validator-set consensus, quorum finality, or competing canonical
  chain is introduced.

Validation remains an optional evidence path alongside the operation and
finality paths. The finalizer may ignore attestations and must still perform
all existing deterministic checks.

## 2. Per-validator conflict exclusion

A validator checks an operation against the latest finalized state. Before it
emits `VALIDATED`, it reserves the operation's `(AccountID, nonce)` in a small
local `ValidationState`, scoped to that finalized base. While the reservation
is active, this validator must not attest to a conflicting operation for the
same Identity nonce against that same base. It reports `CONFLICT` and emits no
positive validation attestation for the second operation.

```text
finalized next admissible nonce = 10

Alice / nonce 10 -> Bob       reserve (Alice, 10), attest VALIDATED
Alice / nonce 10 -> Charlie  CONFLICT, no positive attestation
```

This reservation is local validator state, not consensus state. It must be
durable enough to survive validator restart and fail closed if its state is
unavailable or corrupt. When a newer PoA block finalizes, reservations against
the old base become historical and cannot affect canonical state or attestations
against the new base. The journal retains one latest finalized base and at most 4096 reservations. A newer verified base atomically replaces the old reservation set; rollback or a changed root at the same height fails closed.

Different validators may select different conflicting candidates. A client
applies its own policy to the attestations it receives, such as `OFF` or a
configured count of trusted validator identities. A possible future policy
could combine immutable Authority eligibility with an attestation count, but
no threshold is selected. The example `1,000,000` is not a protocol parameter.
Trust policy and any resulting `VALIDATED` display are local, not consensus.

## 3. First validation scope

Version one accepts only operations built directly on the latest
finalized state:

- At most one operation per Identity nonce may be locally `VALIDATED` by one
  validator against a given finalized base.
- The Identity record stores the next admissible nonce `n`; only nonce `n` may be checked.
- Nonce `n + 1` waits until nonce `n` is PoA-finalized.
- No chains of validated dependencies or speculative state overlay are part of
  version one.

A later, separately reviewed `ValidationOverlay` could allow opt-in local
reasoning over trusted validated operations. It would be rebuildable from
canonical state and fully reversible after every new PoA block. It must never
change canonical balances or nonce authority, enable automatic spending, or
authorize remote chunk admission.

## 4. Validator identity and eligibility

The version 7 NodeBinding operation binds a dedicated hybrid signing key and
NodeID to an AccountID. It is a service role, not a device or delegated Identity
credential. A binding may also prove a separate STORAGE_PROVIDER key. The
registry permits at most eight bound nodes per Identity; IdentityRotate revokes
its bindings. The node never receives Recovery, Authorization, or KEM secrets.
Binding does not grant PoA power. The optional local validation trust policy
selects trusted bound accounts; no canonical Authority threshold is required.

## 5. Signed attestation profile

The version 1 positive attestation signs NetworkID, exact OperationID,
finalized base height, block ID and state root, and NodeID with the bound
Ed25519 + ML-DSA-44 key. Verification checks the binding, signature and
operation against that exact finalized snapshot. It cannot be replayed against
a newer base. `INVALID`, `CONFLICT` and timeouts are local outcomes, not signed
fraud findings. A separate canonical receipt is required for any validation
Authority contribution or false-positive penalty.

## 6. Services, transport, and presentation

ValidationService uses a durable, bounded ValidationState journal outside
canonical state. CYP2 carries bounded validation requests and results, with
ingress admission before parsing or signature work. It does not create a global
pending pool. The PoA OperationPool remains independent.

Client trust policy defaults to `OFF`. Desktop trust-policy wiring and the
optional `VALIDATED` projection remain an acceptance gate. Validation never
changes `WAITING_FINALITY`, Mail `Sent`, Files `Protected`, or remote admission.
RootPublication admission remains finality-first; remote durability remains
separate from finality.

## 7. Remaining research and acceptance

The implementation profile and remaining gates are in
[IDENTITY_AUTHORITY_NETWORK_V7.md](IDENTITY_AUTHORITY_NETWORK_V7.md). A local
overlay for dependencies and rebasing needs separate review. CYP2 transport
encryption and authenticated finalizer transport also remain security gates.
Version 7 must not be started against the running version 6 DEV state before
acceptance tests and an agreed cutover.

Implementation boundaries: [VALIDATION_ARCHITECTURE.md](VALIDATION_ARCHITECTURE.md).
ValidationService never reuses the OperationPool candidate overlay.
