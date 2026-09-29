# Provisional validation research target

Status: future research only. This document describes a possible signed
pre-finality evidence layer. It is not implemented, is not a Beta prerequisite,
and does not change the active consensus or application data plane.

## 1. Purpose and boundary

Provisional validation would let eligible service nodes publish independently
checkable claims that a `ProtocolOperation` is valid against a named finalized
state. It may improve visibility or help a PoA operator prioritize its work.
It does not finalize an operation.

```text
network validated != PoA-finalized
```

These invariants are fixed:

- PoA remains the only finalizer and the only source of canonical ordering.
- Every full node continues to verify the PoA certificate, execute every
  finalized operation and independently check the resulting state root.
- The finalizer may ignore all validation attestations and still must execute
  and validate the block itself.
- Validation does not change `CybouState`, canonical balances, nonces, names,
  Authority, budgets or `StateStore::CommitFinalizedBlock()` semantics.
- Validation cannot authorize remote chunk admission. Providers still require
  a finalized `RootPublication` and its valid chunk-authorization proof.
- No BFT, validator-set consensus, quorum finality or competing canonical
  chain is introduced.

Validation is a sidecar to the existing operation and finality paths:

```text
                         ┌─> PoA finalizer ─> finalized block
ProtocolOperation ───────┤                     └─> canonical state
                         │
                         └─> optional ValidationService
                                  └─> signed attestations / local index
                                      (non-canonical evidence)
```

The sidecar does not become a prerequisite for PoA finality.

## 2. Validator identity and eligibility

A validator daemon would use a dedicated node key and a `NodeID` bound to an
`AccountID` by a future canonical binding profile. `NodeID` is a service
identity, not a device identity or user credential. It does not receive the
Identity mnemonic, Recovery key, Authorization key, KEM key or private Mail and
Files data.

Eligibility could be derived from the canonical NodeID binding and finalized
Identity Authority, with exclusions defined by immutable network rules. This
is only a design direction: the binding format, eligibility rule, anti-Sybil
limits and Authority threshold remain open. The example threshold
`1,000,000` is not adopted as a protocol parameter. Authority eligibility
would never grant PoA power.

ProviderID and NodeID remain different roles. A storage provider proves its
ProviderID in the CYP2 session; that proof alone does not register or qualify
it as a validator.

## 3. Attestation profile direction

A future `ValidationAttestation` should bind a signed result to at least:

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

The exact encoding, signature domain, result vocabulary, evidence retention,
expiry and transport are unresolved. An attestation must not be replayable
across networks, operations or finalized bases. A validator's signature proves
who made a claim; it does not prove the claim is correct.

An `INVALID` result is also a claim. A disagreement, a timeout or an operation
that later loses to a conflicting finalized operation is not by itself proof
of fraud. No Authority reward or penalty follows from an attestation alone.
Any later global effect requires immutable policy and canonical, attributable
evidence.

## 4. First validation scope

The simplest first profile would validate operations against the latest
finalized state only. A validator independently checks exact serialization,
Identity authorization, nonce, balance, fees, names, publication rules and all
other deterministic checks applicable at that base.

The first profile would not validate an operation on top of another
unfinalized or merely validated operation. In particular, an account's next
nonce cannot advance through a chain of provisional operations before the
preceding operation is finalized. Attestations for competing operations may
coexist; they mean only “valid against this named finalized base,” never “will
be included.”

When a new block finalizes, old attestations remain historical evidence about
their stated base. They do not change the new canonical result. An operation
that conflicts with the finalized history is stale or not included; that alone
does not make its validator fraudulent.

## 5. Services, pools and transport

A future `ValidationService` could validate operations and verify received
attestations. A separate local `ValidationIndex` or pool could retain bounded
operation and attestation data. Initially, `OperationPool` remains the
PoA-pending path and canonical state remains owned by the existing runtime.

The current P2P profile has no validation messages and no distributed global
pending-operation pool. If validation is pursued, operation delivery to
participating validators and attestation dissemination need a separately
specified, resource-bounded CYP2 profile. New message types, admission rules,
limits, expiry and abuse controls are not decided here.

The PoA finalizer may use verified attestations as an optional prioritization
hint. It must still run the existing deterministic execution and state-root
checks over every operation it finalizes.

## 6. Client presentation and local state

`FINALIZED_ONLY` remains the default: clients continue to show canonical state
and the current publication phases. An observation mode could display
non-canonical validation evidence, but its wording and any threshold for a
summary such as “validated by the network” must be specified before use.
Observed validation does not change `WAITING_FINALITY`, Mail `Sent`, Files
`Protected` or other canonical/durability outcomes.

A later, separately gated stage could offer an explicit local
`ValidationOverlay` for users who accept provisional results. Such an overlay
must remain outside `CybouState`, must be rebuilt from the latest finalized
state, and must never affect canonical wallet balances, nonce authority,
Identity Authority, automatic spending or remote storage admission. If a new
finalized block conflicts with overlay entries, the client discards or
recomputes those entries from the new canonical base. Dependent provisional
operations and rollback rules need their own design before this stage.

For Mail and Files, any future informational sequence is conceptually:

```text
Submitted
-> optional pre-finality evidence (still waiting for PoA)
-> PoA-finalized
-> Securing
-> Protected
```

The existing application services and storage lifecycle do not depend on this
sequence. RootPublication admission remains finality-first, and remote
durability remains separate from finality.

## 7. Staged research only

1. **Research/simulation:** measure whether signed operation checks provide
   enough value to justify the added network and operational complexity.
2. **Possible first implementation:** bind eligible NodeIDs, validate
   operations against finalized bases, distribute bounded signed attestations
   and optionally display them. No provisional dependencies, canonical effects
   or rewards/penalties.
3. **Possible local overlay:** only after separate review, with explicit user
   opt-in, clear stale/conflict handling and canonical-state isolation.
4. **Possible dependency or Authority policy:** separate research only after
   measurable need, anti-farming analysis and canonical attributable evidence.

Storage hardening and Mail/Files Beta remain ahead of this work. Provisional
validation must not block Beta.

## 8. Open design questions

- What canonical binding and bounded registry should associate NodeIDs with
  Accounts, and how are validator keys rotated or revoked?
- What immutable Authority eligibility threshold, per-Account limits and
  anti-Sybil controls are justified? No numeric threshold is selected.
- What exact operation bytes, base state, result, reason codes, signature
  domain, expiry and retention belong in an attestation?
- How are pending operations delivered without creating an unbounded global
  mempool, and how are attestation spam and resource use controlled?
- What evidence threshold, if any, may be presented as a network summary?
- How should a future local overlay handle conflicts, rebasing and dependent
  provisional operations without exposing it as canonical wallet state?
- What canonical evidence could justify any future Authority credit or penalty
  for validation behavior?
