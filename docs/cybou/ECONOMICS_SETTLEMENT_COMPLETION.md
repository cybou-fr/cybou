# Economics-first settlement completion

Status: CURRENT
Scope: Operator-authorized implementation target, 2026-10-09. Current DEVNET
wire, genesis and canonical execution remain unchanged until a separate,
reviewed protocol transition. Operator approved canonical-code development in
isolated fixtures on 2026-10-09, without deployment or changing running DEVNET.
This is not evidence of completed payouts. Intermediate core builds are not
deployable while funding and cumulative settlement rules remain inconsistent.

## Delivery order

### Integration control checkpoint (2026-10-09)

Source audit baseline `05ca1fee`, followed by complete-term quote package
`e190cbe6`. The next deliverable is the existing StorageService/PoA/canonical
execution integration, not another standalone assignment helper. The complete
term quote closes all-slot preparation only; it does not change production.
Do not activate it behind the current settlement codec: current execution cannot
validate the target inputs, and would still apply the obsolete daily cap.

Current production path is
`StorageService::SettlementEntries` -> desktop adapter/controller ->
`CybouNodeRuntime::SubmitStorageSettlement` ->
`PoaFinalizer::SignStorageSettlement` -> candidate/block execution ->
`ApplyStorageSettlement`. It reads live payout bindings, ranks locally verified
placement chunks, retains at most lease.replica_count recipients and distributes
the old period cap. Current state has one extendable lease per publication and
no independently funded term or per-provider cumulative paid ledger. On closure
it refunds and erases the lease. These are protocol gaps, not missing GUI work.

| Existing module | Production caller today | Owned/duplicated data | Integration disposition |
|---|---|---|---|
| storage_assignment | None outside module/test composition | Per-chunk context, full eligible and selected pairs; repeats eligible set | Keep deterministic selection; share eligible transcript per assignment batch after approved contract |
| storage_assignment_attestation | None outside module/test composition | Per-chunk signature and all raw bindings; duplicates proofs across chunks | Reuse verification and existing PoA signer; batch commitment rather than thousands of signatures |
| storage_assignment_observer | Observation-store helper only, no production dispatch | Transient receipt, audit challenge/answer or GET bytes | Run from existing storage I/O scheduling; use existing authenticated transport |
| storage_assignment_observation_store | Tests only | Immutable observations/index; receipt repeated per observation | Integrate into existing EvidenceLedger ownership; retain proof references without another DB |
| storage_assignment_evidence | Payout verifier and tests | Local intervals plus shared funded-slot claims repeat interval fields | Shared claims are the cross-epoch exclusion authority; local view can be derived after recovery/regression coverage |
| storage_assignment_payout | Tests only | No records; caller supplies scopes/manifest/rate/paid/snapshots | Call within existing settlement preparation using canonical inputs, never live peers as paid history |
| storage_evidence_ledger | StorageService placement/audit/verification/settlement | Receipts, latest replica timestamps and provider shadow totals | Keep diagnostics separate from payable service; reuse receipt storage and ownership, remove diagnostic payout input only at approved activation |

This table identifies a consolidation direction, not permission to delete existing
journals. Preserve raw evidence and exact operation jobs until verified replay is
available. Neither latest-success timestamps nor shadow CYBOU totals are canonical
entitlement. No new runtime, payment service, scheduler or provider registry is needed.

#### Measured-layout scalability calculation

Current serialized plan has 185 context bytes, two u32 list counts, 64 bytes per
eligible pair and 64 per selected pair. With two replicas this is `321 + 64E`
bytes; its separate PoA signature is 3373 bytes. Thus 1000 chunks with E=100
repeat at least 10,094,000 bytes, or 69,230,000 with E=1024, before raw payout
bindings, DB keys/encryption and evidence. They also require 1000 hybrid PoA
signatures and repeated binding verification. These are layout calculations,
not throughput measurements. Local/shared interval duplication costs 144 bytes
per claim before headers; 4096 claims x 1000 chunks x two slots is 1,179,648,000
bytes. The present fixed limits must not become a production accrual halt.

Selected design for protocol review: one assignment batch per publication,
funded term and epoch; one retained eligible transcript/binding set and one PoA
signature over its scope, selected-assignment Merkle root and eligible/seed
commitments. A leaf contains ChunkID, epoch and ordered storage/payout pairs
(168 bytes for two slots, with common context outside the leaf). A 1000-leaf
tree needs at most ten 32-byte siblings for an individual proof. This shares
signatures and eligibility proofs while retaining chunk-specific membership.
Verifier caches are local only and never substitute signature checks. Exact
hash domains, leaf ordering, padding and byte codec require approval and vectors;
no new wire entity or cryptographic domain is implemented by this checkpoint.

#### Minimal protocol change proposal — not an adopted wire/state format

The following proposal is inside this CURRENT implementation plan for review;
it is not implementation authorization for consensus changes.

1. Funding: key each immutable funded term by its finalized funding OperationID
   (initial RootPublication or renewal StorageLease). Retain publication, payer,
   units, replicas, start/end periods, UTC anchor, frozen rate, B/T and both escrow
   origins separately. Renewal appends a term instead of rescaling prior rights.
   State serialization and state-root vectors must cover these records.
2. Assignment: bind the batch to the finalized publication authorization root
   and a complete unique manifest with Merkle authorization. A locally frozen
   eligible set alone cannot prove that it predates the seed. A reviewed canonical
   commitment made before a later finalized seed is required; its placement in
   the existing operation format and deterministic effective epoch boundaries
   remain approval gates. Do not call the current caller-supplied seed verified.
3. Accounting: retain canonical cumulative unit-seconds and finalized paid per
   term/replica slot/StorageId/payout pair, plus slot aggregate usage and the accepted
   assignment/evidence commitments. Aggregate service cannot exceed T; provider
   cumulative floor uses the same B across all chunks and replacements. Raw audits
   remain off-chain; PoA attests their bounded service totals. Full Nodes validate
   the signed totals and arithmetic, not continuous physical custody.
4. Settlement: extend the existing signed operation with explicit end UTC,
   evidence commitment and term/slot/storage/payout/cumulative-service/amount
   entries, ordered by the complete key. Repeated provider claims across slots
   need distinct entries. Do not retain the old per-lease replica-count recipient
   restriction: replacement can create more legitimate historical recipients.
   Check bindings against the assignment's finalized historical keys, monotonic
   totals, entitlement minus canonical paid, per-slot and term budgets, account
   existence, no self payout, origin-preserving debit/credit and TotalCybou equality.
   Validate the entire candidate before publishing any state mutation.
5. Recovery: existing encrypted app.db exact-operation journaling retains the
   prepared input snapshot, unsigned commitment and signed exact bytes/OperationID
   before submission. Reconcile canonical finality before retry; never recompute
   a different operation on lost ACK. A stale parent requires reconciliation, not
   a second payout. Only finalized execution changes paid or the period cursor.
6. Bounds: maintain the 1024-entry atomic failure rule until an approved bounded
   period-batching contract exists. No successful empty settlement may bypass an
   oversized obligation set. That contract must fix ordering, total batch root,
   completion and refund timing before cursor advancement; it cannot be inferred
   from the current wire. Quantify serialized/block limits before activation.

UTC accrual policy, evidence retention and transition disposition are mandatory
parts of approval. A candidate policy is interval credit only between two durable
successful checks of the same active chunk/slot/provider, within the funded term,
with explicit maximum gap/full-GET cadence and failure/replacement boundaries.
No first-check credit, extrapolation or credit over a failed check. Split at period
boundaries; replay derives the same disjoint claims from retained observations.
Cadence/max-gap values are deliberately undecided: the diagnostic one-day bound
does not authorize a payment policy. Retention must preserve all unsettled proof
material, plus finalized term replay/dispute material for an agreed horizon.
Compaction needs canonical paid checkpoints and atomic recoverable replacement;
capacity pressure must produce an explicit retryable error before losing evidence.

Activation also requires deterministic handling of already funded live leases
and their absent historical service/paid data. Do not invent that history. Choose
and review a boundary/closure policy and pre/post-boundary vectors, then authorize
deployment separately. No genesis, NetworkID, keys, signing history or live nodes
are changed here. No dual permanent codec/version path is prescribed.

#### Settlement layout and recovery review (2026-10-09)

Source inspection confirms `MAX_OPERATION_PAYLOAD_BYTES = 131072`, including
the operation-kind byte. The finalized block limit is 33554432 bytes; it does
not override the per-operation limit. Current settlement uses a 20-byte body
header, 72-byte entries and a 3373-byte signature: 1024 entries produce 77122
tagged bytes. A replacement codec must check both entry count and exact byte size.

Concrete proposed base layout for review (all integers LE, identifiers raw32,
no discriminator, exact consumption):

| Field | Bytes | Validation |
|---|---:|---|
| period | 8 | Current canonical period, or approved batch cursor |
| period_start_utc | 8 | Nonzero, equal to canonical expected start |
| period_end_utc | 8 | Checked start + immutable period duration |
| evidence_root | 32 | PoA commitment to retained assignment/service inputs |
| entry_count | 4 | At most 1024; count checked before allocation |
| entries | 113 each | Strict full-key order, no duplicate key |
| witness_count | 4 | Bounded by count and remaining byte budget |
| witnesses | Variable | Exact length-delimited typed historical binding/assignment proofs; codec still requires review |
| PoA signature | 3373 | Existing Ed25519 + ML-DSA-65 role |

An entry is funded-term OperationID32, replica_slot u8, StorageId32,
payout AccountID32, cumulative_verified_unit_seconds u64, payout_amount u64.
Publication/payer/budget derive from the canonical funded term, not repeated
untrusted fields. Sort by (term, slot, StorageId, payout). Zero-amount entries
are necessary to finalize verified service before a whole CYBOU accrues; require
a strict service increase or an explicitly permitted initial assignment, not
unlimited unchanged no-op entries. Amount must exactly equal the new cumulative
floor minus canonical paid; reject a lower service total or overpayment.

Without witnesses the tagged base is `3438 + 113 * count`: at 1024 it is
119150 bytes, leaving only 11922 for all witnesses. Adding a per-entry assignment
commitment32 instead would exceed 128 KiB (151918 bytes before witnesses).
Therefore historical bindings/assignment proofs must be deduplicated and byte
bounded. A hash reference alone is insufficient unless canonical execution can
resolve its retained verified contents deterministically. An off-chain RPC fetch
or current connected peer cannot be a block execution dependency. Do not omit
binding verification to fit the payload; approved bounded batching or already
canonical proof material is required. The proposal is not yet a complete codec
and must not be activated until witnesses and batching have exact layouts/vectors.

Keep the existing `CYBOU/STORAGE-SETTLEMENT` signing domain and NetworkBinding;
the reviewed body changes, not the domain spelling. Compute operation identity
from existing canonical operation bytes. Do not introduce a provider registry
as a shortcut for proof lookup. Batch commitment hash domains remain separately
specified before implementation, not mechanically renamed existing domains.

Current `PoaFinalizer::SignStorageSettlement` checks safety halt and verifies its
signature, but does not persist the signed operation; its block-signing journal
does not constitute an exact settlement-operation journal. Current runtime
submission then admits the operation to the volatile candidate pool. The desktop
controller holds pending generation only in memory. No inspected path connects
this settlement to IdentityOperationCoordinator's exact-byte journal. A lost ACK
or process restart cannot be claimed recovered merely because the block signer
has a safe history.

Required durable lifecycle in existing app.db, scoped by NetworkBinding and
canonical period/batch identity:

- Prepare: reconcile finalized cursor and term paid state; durably retain the
  exact unsigned body and its canonical input/evidence references before signing.
- Sign: reuse an existing exact signed record; otherwise sign that durable body,
  self-verify and durably save canonical bytes/OperationID before pool admission.
  A crash between signing and saving may retry the same body; never substitute a
  different body under the same unresolved journal key.
- Submit: retry those same bytes, including after lost ACK. Pending means no
  balance/paid/cursor changes. Rejection or missing evidence retains the journal
  and obligations with an explicit error, not successful empty completion.
- Reconcile: lookup exact OperationID in verified finalized history. Only its
  accepted canonical result permits clearing submission work; advance paid only
  through canonical execution. If another settlement consumed the period,
  reconcile its complete canonical effects before rebasing any outstanding work.
- Retain: keep evidence/checkpoint references needed for replay and the approved
  dispute horizon. Journal deletion is not evidence compaction; neither is allowed
  to remove the only reconstructible unresolved operation.

This uses existing encrypted store and finalizer, without a second database,
worker or signing authority. app.db locking must not span network I/O. Journal
corruption/unlock failure is fail-closed. Exact restart fixtures must cover every
boundary above, including failure to save signed bytes; no such production
recovery acceptance is claimed by these specification checks.

#### Required vectors and integration order

First approve the above contract, byte layouts/domains, audit/time policy,
bounded batches and existing-lease disposition. Then change existing
storage_lease/state/block_executor and their tests together, wire StorageService
and the existing finalizer/journal, and finally test isolated Full Node runtimes.
Do not enable production assignment before the compatible payout validator.

Vectors must include: U=1, P=30, S=86400, R=5, N=2 -> B=1, escrow=2,
day-one entitlement=0 and full-term entitlement=1 per honest provider;
the same slot split equally between two providers -> separate floors both 0,
refund=1; one honest full-term slot and one without evidence -> payout=1,
refund=1; replayed finalized operation -> no second transfer; renewal -> a new
independent term. Extend these with multiple chunks, mixed origins, rotation,
replacement, future/corrupt/overlapping evidence, exact restart before/after
signing, 1024/1025 recipients and conservation. Signed helper fixtures already
passing are not this runtime integration evidence. Live independent-host
acceptance follows approved cutover and separate deployment authorization.

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
lease funding or settlement execution in the running DEVNET. The approved
isolated development implementation now calls the budget calculation from
ComputeStorageLeaseEscrow, shared by initial publication funding, renewal and
application cost quotes. The old period cap/executor remains pending replacement;
this partial funding change must not be deployed. Current canonical entries cannot
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
independent-host validation and production integration remain gates. Shared
funded-slot exclusion below handles local cross-epoch accounting. A commitment supplied by a peer is not verified
service. Callers must validate UTC policy and completed periods before preparing
settlement; this component has no clock or canonical period authority. Current
production auditing, placement and settlement execution are not activated through
this new journal by this package.

### Shared funded-slot exclusion across replacement epochs

Interval accounting now additionally journals claims under
`storage/funded-slot-evidence/<NetworkBinding>/<PublicationID>/<ChunkID>/<payer>/<term_start>/<term_end>/<slot>`.
The key deliberately excludes epoch, finalized seed and selected provider: a
replacement assignment cannot create another budget for the same funded slot.
Separate chunks, replica slots and independently funded terms retain separate
accounting. Canonical lease/payer/term provenance remains caller validation;
supplying a different term does not establish new funding.

Each claim retains assignment commitment plus the original interval. Shared
claims are UTC-ordered, non-overlapping, proof-unique and bounded to 4096 per funded
slot across all epochs. The same UTC anchor/duration is required across epochs.
Local assignment intervals and shared claims commit in one app.db batch.
The same batch retains an `/initialized` marker (exact byte 1); a missing shared
record with a surviving marker fails for every epoch, including a fresh one.
Losing or deleting all copies of trusted local accounting cannot be detected by
this journal alone; production recovery must reconcile durable canonical history.
Exact retry remains idempotent only for the original assignment; overlap or reused proof
under another epoch fails. Query requires an exact match between local intervals
and the shared claims owned by that assignment. Missing/corrupt shared records
fail rather than recreating credit or reporting zero.

Shared record: UTC anchor u64, period duration u64, count u32, then assignment
commitment 32 and interval period/start/end u64 plus proof commitment 32 (88 bytes
per claim), integers LE, exact consumption. This complements the existing local
record without a version discriminator or legacy decoder. Previously standalone
local fixture records cannot supply payable totals without matching shared claims;
they fail closed. No deployed accounting migration or live activation is claimed.
Proof authentication, audit cadence/gap policy, active epoch time boundaries,
observation-to-interval derivation and canonical payout/state integration remain
open. This change prevents local duplicate funded-slot credit, not all provenance
or physical host/replica independence errors.

Evidence foundation follow-up: existing provider diagnostics now commit record,
index and bounded eviction atomically before changing memory. Failed persistence
cannot produce a successful replica verification. Concurrent observations are
serialized; duplicates/older timestamps never rewind the interval cursor, and
gaps beyond the one-day policy bound credit no service. Restart preserves prior
credited totals but starts a fresh observation interval, excluding downtime.
This hardens the existing shadow ledger; it does not create assignment-bound
payable evidence, PoA aggregation or canonical entitlement.

## P0-03: protocol and recoverable execution gate

### Read-only slot payout preparation

`PrepareStorageAssignmentSlotPayouts` connects stored assignment interval journals
to the accepted cumulative target arithmetic for one funded replica slot across
all authorized chunks and replacement epochs. Caller supplies the finalized
chunk manifest, immutable term rate/UTC and canonical paid history; this helper
does not prove those inputs or activate canonical funding/settlement rules.

All scopes must agree on network/publication/payer, funded period range, replica
count, slot and UTC policy. Each supplied plan must have its stored genesis-PoA
attestation and a consistent local/shared interval journal. Authorized chunks and
plans cannot repeat; every manifest chunk needs a representative, and every
assignment commitment referenced by shared claims must resolve to a supplied plan
for the same chunk. Omitting an older epoch fails rather than losing its service.
Future-period claims must still resolve but contribute no earlier-period credit.

The single slot share B and denominator T are computed from the complete manifest
using the accepted funded-term formula. Service sums by exact StorageId/payout
AccountID pair across chunks/epochs; each pair receives its own cumulative floor.
The same pair across epochs retains one floor; different storage-key/payout pairs
retain separate floors. No provider receives a fresh B because of replacement.
Finalized paid inputs cannot duplicate a pair, reference an unresolved provider or
exceed its entitlement. Total entitlement/paid/new payout cannot exceed B. More
than 1024 provider/payment inputs fails without truncation. Missing/corrupt/locked
state, inconsistent terms, invalid attestations and arithmetic overflow fail.

The sorted result includes per-pair service, entitlement, canonical paid and due,
plus the single-slot budget/totals. Preparation rejects enclosing app.db
transactions, so staged intervals cannot become a successful payout quote before
durable commit. It never writes paid history, advances a period,
signs a settlement or emits current wire entries. All reads run under one local DB
snapshot. Repeating preparation and reopening retain the same quote; only a new
canonical finalized-paid input reduces due. Physical independence, lease/manifest
and paid provenance, historical registry provenance, audit-to-interval policy,
active epoch boundaries, full multi-slot operation/state format and separately
authorized activation remain explicit gates.

Preparation now requires registry snapshots indexed by every assignment's seed.
For each plan it reloads the retained raw payout bindings and checks both STORAGE
and Authorization signatures against that exact historical registry. Missing,
duplicate or null snapshots, wrong keys and corrupt retained bindings fail before
a quote is returned. The helper verifies cryptography and matching block IDs;
callers still independently establish that the supplied registry really is the
finalized state at that block. It does not substitute today's rotated keys.

### Complete term payout preparation

`PrepareStorageAssignmentTermPayouts` now prepares every funded replica slot in
one outer app.db snapshot through the same internal slot verifier. Missing slots,
mixed publication/network/payer/term/UTC scope, invalid paid slot or inconsistent
budgets fail rather than returning a partial term. Canonical-paid and due totals
are bounded by the term escrow; the 1024 provider-entry/payment-input limit applies
to the whole result, not separately to each slot. Slots are returned in index order.
Preparation remains read-only and cannot advance finality or paid state.
Shared claim streams are also compared across the two replica slots for each
chunk: overlapping service must use distinct StorageIds and payout accounts even
when the claims refer to different assignment epochs. This checks necessary
storage/economic identity separation, not independent machines/operators.
Caller provenance, audit interval policy and canonical format/activation gates remain.

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

### Assigned replica observation boundary

`ObserveAssignedStorageReplica` now calls the existing StorageTransport only after
verifying the genesis-attested assignment, selected StorageId and exact signed
receipt. Optional local reference bytes must match the assigned ChunkID/size;
the stored size is bounded by the existing encrypted chunk maximum. Runtime
transport authenticates the requested StorageId through its storage session.
Custom transports must enforce that same condition: endpoint labels alone do not
authenticate the provider.

With reference bytes it generates a fresh OpenSSL nonce and random byte offset,
compares held/response_hash to the exact issued challenge, and returns the raw
challenge/answer and receipt scoped to assignment commitment/slot/provider.
Missing audit support/response or RNG failure falls back to full GET. A negative
or incorrect answer fails directly. Without reference bytes or with force_full,
GET must return the exact size and BLAKE3 ChunkID; verified bytes are returned
transiently, without creating another content database. The caller schedules
periodic full checks and runs this synchronous call outside the GUI thread.

The returned audit answer is not a standalone provider-signed proof; it is a
local observation tied to an authenticated transport request. The helper does not
persist observations, append duration intervals, discover providers or dispatch
production PoA work. The durable collector below provides local retention/replay;
interval policy and aggregation remain open. A successful instantaneous check never establishes uninterrupted
service or authorizes a payout by itself. Tests exercise signed assignment/receipt
fixtures through a controlled transport, not live remote funded leases.

### Durable local observation collector

`ObserveAndStoreAssignedStorageReplica` calls that transport boundary and retains
successful observations under `storage/assignment-observations/<commitment>/<slot>`
in existing encrypted app.db. The PoA-attested plan must already be durably stored.
No database lock is held across network I/O; assignment/store state is checked
again before atomic commit of the immutable observation and its sorted UTC index.
The API rejects enclosing transactions so success means durable outermost commit.

One record is allowed per assignment/slot/UTC second. Exact serialized retries are
idempotent; different audit challenges at the same second conflict rather than
overwrite. Each slot is limited to 4096 records, without automatic eviction.
Malformed/oversized/unsorted indexes and missing indexed records fail closed.
Locked stores, malformed target records and conflicting retries do not create a
successful durable observation. New observations require nonzero caller-supplied
UTC no later than the supplied verified-through boundary; neither value proves
canonical time. PoA clock/term policy remains an independent validation gate.

Record layout: observed UTC u64, stored size u32, assignment commitment 32,
StorageId 32, slot u8, kind u8 (0 audit, 1 GET), receipt length u32 and exact receipt
(maximum 8192). Audit then retains ChunkID 32, offset u64, nonce 32, held flag u8
and response hash 32. Index is count u32 and strictly increasing nonzero u64 UTCs.
Integers are LE and decoding consumes all bytes. This is a local off-chain codec,
not a new P2P entity, block field or protocol version.

`LoadAssignedStorageObservation` validates the stored assignment/slot, receipt,
size and exact local ChunkID; audits recompute the response from retained challenge
and local ciphertext. GET bodies are not duplicated into metadata: reload requires
the chunk bytes from the existing content store and returns an empty GET body.
The recorded GET success remains the trusted collector's assertion, not a
provider-signed proof that remote data was held at the recorded time. These records
do not themselves generate service intervals or survive as usable payable evidence
when the exact local reference bytes are unavailable. Production dispatcher,
collector authenticity/provenance aggregation, retention/compaction policy and
evidence-to-interval-to-settlement wiring remain open.

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
