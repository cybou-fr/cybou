# CYBOU Validation

Status: future research only. Validation is not implemented, is not a Beta
prerequisite, and does not change active consensus, canonical state, or the
application data plane.

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
finalized nonce = 10

Alice / nonce 11 -> Bob       reserve (Alice, 11), attest VALIDATED
Alice / nonce 11 -> Charlie  CONFLICT, no positive attestation
```

This reservation is local validator state, not consensus state. It must be
durable enough to survive validator restart and fail closed if its state is
unavailable or corrupt. When a newer PoA block finalizes, reservations against
the old base become historical and cannot affect canonical state or attestations
against the new base. The exact expiry and cleanup rules remain to be designed.

Different validators may select different conflicting candidates. A client
applies its own policy to the attestations it receives, such as `OFF` or a
configured count of trusted validator identities. A possible future policy
could combine immutable Authority eligibility with an attestation count, but
no threshold is selected. The example `1,000,000` is not a protocol parameter.
Trust policy and any resulting `VALIDATED` display are local, not consensus.

## 3. First validation scope

Version one would accept only operations built directly on the latest
finalized state:

- At most one operation per Identity nonce may be locally `VALIDATED` by one
  validator against a given finalized base.
- Nonce `n + 1` may be checked against finalized nonce `n`.
- Nonce `n + 2` waits until nonce `n + 1` is PoA-finalized.
- No chains of validated dependencies or speculative state overlay are part of
  version one.

A later, separately reviewed `ValidationOverlay` could allow opt-in local
reasoning over trusted validated operations. It would be rebuildable from
canonical state and fully reversible after every new PoA block. It must never
change canonical balances or nonce authority, enable automatic spending, or
authorize remote chunk admission.

## 4. Validator identity and eligibility

A future validator daemon may use a dedicated node key and `NodeID` bound to an
`AccountID` by a separately specified canonical binding profile. `NodeID` is a
service identity, not a device identity or user credential. It does not receive
the Identity mnemonic, Recovery key, Authorization key, KEM key, or private
Mail and Files data.

Eligibility could depend on finalized Identity Authority and immutable
anti-Sybil rules. The binding format, eligibility rule, key rotation, limits,
and any Authority threshold remain open. Authority eligibility would never
grant PoA power. `ProviderID` and `NodeID` remain separate roles; proving a
storage `ProviderID` in CYP2 does not register or qualify a validator.

## 5. Attestation profile direction

A future signed `ValidationAttestation` should bind at least:

```text
NetworkID
OperationID and/or exact operation hash
BaseFinalizedHeight
BaseStateRoot
ValidatorNodeID
validation profile version
result and, if needed, a bounded reason code
signature by the dedicated node key
```

Encoding, signature domain, result vocabulary, evidence retention, expiry, and
transport remain unresolved. Attestations must not be replayable across
networks, operations, or finalized bases. A signature proves who made a claim;
it does not prove the claim is correct.

`INVALID` and `CONFLICT` are claims or local outcomes, not automatic proof of
fraud. Disagreement, timeout, or losing to a conflicting finalized operation
does not by itself justify an Authority reward or penalty. Any future global
effect requires immutable policy and canonical, attributable evidence.

## 6. Services, transport, and presentation

A future `ValidationService` could check operations and verify attestations.
Its local `ValidationState` and any bounded attestation index remain outside
canonical state. `OperationPool` remains the PoA-pending path. The active CYP2
profile has no validation messages or distributed global pending-operation
pool; delivery and dissemination require a separately specified, bounded
profile with admission, expiry, and abuse controls.

Client policy defaults to `OFF`. Until validation exists, clients continue to
show current operation and publication phases. If later enabled, `VALIDATED`
may be shown only as a local pre-finalized assessment under that client's
policy. It must not change current `WAITING_FINALITY`, Mail `Sent`, Files
`Protected`, or other canonical/durability outcomes.

For Mail and Files, a future informational sequence may be:

```text
SUBMITTED
-> optional local VALIDATED (pre-finalized)
-> PoA FINALIZED
-> SECURING
-> PROTECTED, after remote durability is met
```

The existing services do not depend on this sequence. RootPublication
admission remains finality-first, and remote durability remains separate from
finality.

## 7. Staged research only

1. Research whether signed operation checks justify their network and
   operational complexity.
2. If justified, separately specify NodeID binding, per-validator conflict
   exclusion, validation attestations, bounded distribution, and local trust
   policy. Do not add provisional dependencies or canonical effects.
3. Consider an opt-in local overlay only after separate review of conflict,
   rebasing, and rollback behavior.
4. Consider dependency or Authority policy only after measurable need,
   anti-farming analysis, and canonical attributable evidence.

Storage hardening and Mail/Files Beta remain ahead of this work. Validation
must not block Beta.

## 8. Open design questions

- What canonical binding and bounded registry associate NodeIDs with Accounts,
  and how are validator keys rotated or revoked?
- What immutable Authority eligibility rule, per-Account limits, and
  anti-Sybil controls are justified?
- How are local nonce reservations persisted, expired, and rebuilt safely when
  PoA finality advances or local state is corrupt?
- What exact operation bytes, finalized base, result, reason codes, signature
  domain, expiry, and retention belong in an attestation?
- How are pending operations delivered without an unbounded global mempool,
  and how are attestation spam and resource use controlled?
- What local evidence policy, if any, should a user configure or see?
- How should a future local overlay handle conflicts and rebasing without
  exposing provisional data as canonical wallet state?
- What canonical evidence could justify any future Authority effect for
  validation behavior?
