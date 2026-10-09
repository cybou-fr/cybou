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

### Deterministic off-chain assignment preparation

`storage_assignment.h/.cpp` now prepares and freezes an immutable per-chunk plan
in the existing encrypted Application DB. This is not a canonical provider
registry or an attestation. Context is NetworkBinding, PublicationID, ChunkID,
finalized seed, payer AccountID, AssignmentEpoch, funded-term start/end and
replica count (1 or 2). Candidates contain only StorageId and payout AccountID;
endpoint/address changes do not affect the transcript. At most 1024 input pairs
are accepted, never silently truncated. Empty IDs, conflicting payouts for one
StorageId, self payout and too few distinct payout identities fail closed.

Sort/deduplicate identical pairs, group by payout AccountID, then deterministic
Fisher–Yates with rejection sampling: shuffle the canonical account list; for
each selected account shuffle its sorted StorageIds and select one. An account
has one group regardless of its number of keys. This is reproducibility and
economic-identity deduplication, not physical independence or a Sybil solution.
The complete eligible input set must be frozen before the seed is chosen; its
eligibility/proof collection and finalized-seed validation remain integration work.

Local transcript (all integers little-endian): context contains five raw 32-byte
fields in the order above, then epoch/start/end u64 and replica count u8. Each
provider list is count u32 followed by count pairs of 32+32 bytes. INPUT hashes
context plus canonical eligible list. Each DRAW hashes input digest plus a u64
counter starting at zero; read its first eight bytes as u64 and reject values
at or above `UINT64_MAX - UINT64_MAX % bound`. One counter spans account and
StorageId shuffles. COMMITMENT hashes context, eligible list and selected list
(selected list retains replica-slot order). SHA-256 domains are exact ASCII
`CYBOU/STORAGE-ASSIGNMENT-INPUT`, `CYBOU/STORAGE-ASSIGNMENT-DRAW` and
`CYBOU/STORAGE-ASSIGNMENT-COMMITMENT`, concatenated without separator/terminator.
These new local transcripts do not rename any existing protocol/crypto domain.

The assignment test fixes a byte-for-byte commitment vector independently
reproduced with Python hashlib/struct: repeated-byte IDs 1/2/3/4/5, epoch 7,
term 10–40, two replicas, candidate StorageIds 10–17 and payout byte
`20 + StorageIdByte % 3` select StorageId bytes 15 then 10. Commitment is
`7bb3ccdf9dc723d8294ab4ede6c7849cac989f4a44cc9d7098df33db66828a72`.

The immutable store key uses SHA-256 with domain `CYBOU/STORAGE-ASSIGNMENT-KEY`
over binding/publication/chunk/epoch/start/end. It excludes seed/candidates/payer
so changing those cannot silently create an alternative assignment for the same
context. Exact retry is idempotent; incompatible replacement fails. Loading
reconstructs selection/commitment and requires exact canonical bytes, bounded
counts and no trailing bytes. A newer epoch creates a distinct retained plan;
it does not itself authorize replacement or overlapping service/payment.

Production placement still uses its current selector. This target preparation
is not activated there until PoA validates eligibility and payout bindings,
attests assignments and binds receipts/audits to their periods/slots. A caller
supplied finalized seed or payout pair alone carries no authority. Proof
retention, independent-host policy, evidence integration and payout execution
remain explicit gates; deterministic hashes alone do not close DOC-005/006.

Retain verified evidence with assignment/period binding and duplicate/overlap
exclusion. Receipts establish admission, not continuous service. Audits and full
GET verification justify bounded service intervals under an explicit policy.
Offline providers retain already established entitlement; failure never creates
successful evidence. The current latest-timestamp placement helper is not this
ledger and must not be advertised as completed economic aggregation.

### Assignment-bound interval accounting

`storage_assignment_evidence.h/.cpp` now persists previously verified service
intervals under the frozen assignment commitment and replica slot in existing
app.db. Term start/end are settlement-period indices, with a separately supplied
verified UTC term anchor and immutable period duration. A selected slot binds
the record to the plan's StorageId/payout AccountID, publication, chunk, network
and epoch; no current endpoint/provider list is needed to read past service.

Each interval records period u64, UTC start/end u64 and nonzero proof commitment
(32 bytes). Half-open intervals must lie within their indexed period. Exact retry
is idempotent; reused proof with changed fields, overlapping intervals, future
end beyond caller's verified UTC boundary, wrong scope and overflow are rejected.
Non-overlapping delayed intervals may be inserted in chronological order. A query
through period K includes only intervals with period <= K: later service does not
pay an earlier period. Each authorized chunk/slot contributes its own seconds;
no implicit replica multiplier or credit for gaps/downtime is applied.

The encrypted record contains assignment commitment (32), slot u8, UTC anchor
u64, duration u64, count u32, then count exact 56-byte intervals, all integers LE.
It is bounded to 4096 intervals per assignment/slot. Append rereads/validates the
whole record inside the store batch; duplicate concurrent appends cannot double
credit. Scope metadata cannot be silently replaced. Corrupt or inaccessible data
returns failure, never fabricated zero service; a readable empty journal returns
zero. A full journal rejects new intervals without truncation; future compaction
must preserve unsettled evidence and finalized paid history under a separate plan.
Restart/reopening retains credited intervals without an in-memory cursor.

This API is an internal accounting boundary, not a raw-audit verifier or a PoA
attestation. It accepts references to proofs verified by its caller. Signed
assignment verification, retained raw evidence, transport-to-verifier ingestion,
independent-host validation and global term/slot exclusion across replacement
epochs remain integration gates. A commitment supplied by a peer is not verified
service. Callers must validate UTC policy and completed periods before preparing
settlement; this component has no clock or canonical period authority. Current
production auditing, placement and settlement execution are not activated through
this new journal by this package.

Evidence foundation follow-up: existing provider diagnostics now commit record,
index and bounded eviction atomically before changing memory. Failed persistence
cannot produce a successful replica verification. Concurrent observations are
serialized; duplicates/older timestamps never rewind the interval cursor, and
gaps beyond the one-day policy bound credit no service. Restart preserves prior
credited totals but starts a fresh observation interval, excluding downtime.
This hardens the existing shadow ledger; it does not create assignment-bound
payable evidence, PoA aggregation or canonical entitlement.

## P0-03: protocol and recoverable execution gate

### Off-chain attestation and signed admission verification

`storage_assignment_attestation` now verifies every eligible StoragePayoutBinding
with both the embedded STORAGE proof and the Authorization key from the supplied
finalized registry. The snapshot ID must equal the plan's seed, and the verified
canonical set must exactly match all eligible pairs, not merely selected slots.
PoA signing checks the plan's NetworkBinding against VerifiedNetworkGenesis and
the signer public key against its genesis-authorized PoA key.

The unsigned plan is durably frozen before Sign. The returned hybrid signature
is self-verified, then saved atomically with the original signed binding records.
Calls from inside an enclosing app.db batch fail before signing: committing a
savepoint would not make the frozen plan durable before the external signature.
Retry loads the stored signature and revalidates the retained bindings instead
of signing again. Corrupt/missing retained evidence fails closed without replacing
the journal. A failed signing attempt may leave the immutable unsigned plan, but
returns no attestation. No block signing history or official key material is reset.

Attestation digest is SHA-256 of exact ASCII domain
`CYBOU/STORAGE-ASSIGNMENT-ATTESTATION` followed by the 32-byte plan commitment,
without separator/terminator. Local signature record is Ed25519 64 bytes followed
by ML-DSA-65 3309 bytes, with exact consumption. Binding records use the existing
StoragePayoutBinding codec unchanged. Reload verifies genesis authority and the
plan; historical binding replay requires the registry at the original seed,
not the provider's current rotated Authorization key.

`VerifyAssignedStorageReceipt` verifies this attestation, then the existing
provider-signed receipt for the exact network, publication, ChunkID and stored
size, and requires its StorageId to match the selected slot. Valid receipts from
other storage keys, altered bytes and wrong scope are rejected. Admission is
not interval evidence: this verifier never infers a duration or appends paid service.

These are callable verification/signing primitives, not a production dispatcher.
The caller still must independently establish finalized seed/registry provenance,
active publication/funded term, eligibility/available budget and host independence
before issuance. Full audit/GET ingestion, cross-epoch exclusion, automatic PoA
collection, settlement/state schema and live activation remain open. Synthetic
fixture publications in cryptographic tests are not live leased publications.

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
