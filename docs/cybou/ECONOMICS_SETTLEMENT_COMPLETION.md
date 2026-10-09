# Economics-first settlement completion

Status: CURRENT
Scope: Operator-authorized implementation target, 2026-10-09. Current DEVNET
wire, genesis and canonical execution remain unchanged until a separate,
reviewed protocol transition. This is not evidence of completed payouts.

## Delivery order

ECONOMICS-P0-01 (accounting), P0-02 (assignment and evidence), P0-03
(end-to-end settlement), then tariffs/Wallet and Beta hardening. New cosmetic,
Network and Enterprise work is deferred. Data-loss, security, crashes and CI
remain urgent. Beta requires a completed storage economic cycle.

## P0-01: funded terms and cumulative entitlement

Use a separately funded term for each initial lease or renewal. A renewal must
not rescale rights already accrued under an earlier term. With immutable rate
R, assigned chunk units U, period count P, period duration S and replica count N:

```
D = 2048 * 86400
T = U * P * S
replica share B = ceil(T * R / D)
term escrow = N * B
entitlement = floor(B * cumulative verified unit-seconds / T)
new payout = entitlement - finalized paid for this term/assignment
```

Positive service/rate inputs fund at least one whole CYBOU per replica. Never
round each daily payout up. The funded share includes the minimum-service price;
partial service earns a proportional entitlement rounded down. Fractional
entitlement stays within this term until subsequent confirmed service completes
a whole CYBOU; at closure unpaid escrow refunds to the payer with its origin.
There is no cross-publication debt, mint, burn or transferable onboarding payout.

For two replicas, separately rounding their shares adds at most one CYBOU to
the current combined escrow ceil. One unit over 30 days at rate 5 reserves 2
instead of 1: neither provider earns a whole CYBOU on day one; each earns 1
after a fully verified term. Simulation checks 1–2048 units and 1/2/30/365
periods. This selects the simpler minimum-per-provider Beta model; tariff
rebalancing remains a separate decision, not an implicit parameter change.

`ComputeAssignedStorageBudget` and `ComputeAssignedStoragePayout` implement
these pure target calculations. They are deliberately not called by deployed
lease funding or settlement execution yet. Current canonical entries cannot
prove an assignment, term or accumulated service. DOC-020 stays open until those
inputs and cumulative paid limits are enforced by canonical execution.

Preparing/retrying a quote never advances paid. Only finality does. Persisted
verified units and canonical paid must reproduce exactly the same entitlement
after restart. Gaps, failures, future-period evidence, duplicate proofs and
overlapping intervals do not increase service. Checked arithmetic rejects
overflow or paid greater than entitlement without partial mutation.

Settlement preparation now fails explicitly beyond its entry limit; it does
not truncate, send a partial list or advance the period on that failure. GUI
preparation failure must not become an empty successful settlement. Handling
more than 1024 actual obligations needs an explicit protocol batching decision;
silently skipping them is forbidden.

## P0-02: assignment and evidence

Bind each assignment to NetworkBinding, PublicationID, ChunkID, StorageId,
PayoutAccountID, AssignmentEpoch, funded term and service interval. Freeze the
eligible input set and finalized randomness; deterministic selection and PoA
attestation must be replayable from retained inputs. Validate StoragePayoutBinding
against finalized authorization keys. A changed endpoint does not change an
assignment. Distinct StorageIds and payout accounts are necessary but do not
prove host/operator independence; Beta needs operator-verified independent hosts
and failure domains, with Sybil limitations disclosed.

Provider replacement does not mint another funded replica share. Evidence for
successive assignment epochs consumes disjoint intervals of the same funded
replica slot; aggregate verified unit-seconds cannot exceed that slot's T.
Provider-specific cumulative floors and paid history remain distinct, including
after an endpoint disconnects. Assignment replay must enforce these constraints.

Retain verified evidence with assignment/period binding and duplicate/overlap
exclusion. Receipts establish admission, not continuous service. Audits and full
GET verification justify bounded service intervals under an explicit policy.
Offline providers retain already established entitlement; failure never creates
successful evidence. The current latest-timestamp placement helper is not this
ledger and must not be advertised as completed economic aggregation.

## P0-03: protocol and recoverable execution gate

Before activating changes, define exact settlement/state serialization and
vectors, assignment/evidence commitments and cumulative term accounting. Full
Nodes validate PoA attestation, periods, assignment/economic identity constraints,
exact entitlement, escrow bounds, origin-preserving transfers and conservation.
Raw audit verification remains outside block execution; nodes do not claim to
have independently observed physical service. Expensive checks happen before
PoA signing. Persist exact preparation/submission and reconcile finality on
restart; never generate a second payout because a response was lost.

Transition checklist: inventory live leases and paid history; specify their
deterministic disposition and exact activation boundary; replay pre/post-boundary
fixtures; coordinate all participating Full Nodes and the sole signer; archive
operator recovery material; authorize deployment separately. Do not guess missing
historical service, add a legacy decoder or overwrite genesis/signing history.
No network cutover or reset is authorized by this document. Until an agreed
transition exists, target code must not silently replace current canonical rules.

Close the gate with real providers: correct honest payout; deleted/corrupt chunks
unpaid; one provider offline while the other is paid for its own service; PoA
restart between preparation/submission/finality without double payout. Include
1/2/30/365 periods, partial/missed periods, 1/2/1000 economic identities, overflow,
refunds, revocation/renewal, replay and onboarding-origin/conservation tests.
Arithmetic unit tests do not establish durable evidence or live acceptance.
