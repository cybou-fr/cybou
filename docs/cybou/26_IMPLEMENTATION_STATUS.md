# Implementation status

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Recorded status: code/evidence reviewed on 2026-10-04 with dated 2026-10-05 runtime and
desktop updates below; deployment statements retain their stated scope.

## Shared economics quotes (2026-10-09)

### Canonical assignment replica observations (2026-10-11)

ObserveCanonicalStorageReplica now resolves the exact finalized ACTIVATE body
from this Full Node's accepted block and the matching funded term/latest accepted
epoch from its state snapshot. It reconstructs the per-chunk deterministic plan
from retained PREPARE eligible bindings, accepted seed and immutable term context.
Only an active funded term with a present publication is eligible for new checks;
unknown actions, non-manifest chunks, wrong funding/slots/providers and receipts
fail before I/O. Historical Authorization keys are not requested or reverified
against today's rotated registry; their acceptance is retained canonical state.

The canonical path executes the existing receipt-bound exact GET/random offset
audit through StorageTransport, sharing the byte verifier with the preceding
attestation component. No fabricated per-chunk signature, new hash domain,
registry, wire or service is introduced. An explicit negative audit remains a
failure without GET masking; full GET is the default for this raw call.

Verification: core target rebuilt; all 53 selected assignment-attestation and
StorageService cases passed, 2071/2071 assertions. The other 278 core cases were
not selected in this focused run. The two-node fixture checks canonical GET/audit
after payout-key rotation, rejected IDs/slots/receipts before I/O and negative audit
without GET masking. Transport is an exact-byte in-memory fixture, not VPS evidence.
Ignored logs: artifacts/economics-canonical-observer-build.txt and
artifacts/economics-canonical-observer-tests.txt.

This is the canonical raw-observation boundary, not durable interval integration:
checks use the captured finalized snapshot, do not persist an attempt/success,
schedule 12-hour checks, enforce eighth-success GET or credit/pay elapsed service.
The durable collector must revalidate epoch/time when committing observations.
Its old per-chunk attestation prerequisite and quote consumer remain to be replaced;
that preceding component path is temporary integration work, not a second permanent
production authority path. No deployment or physical custody claim is made.

### PAY interval-reference retention (2026-10-11)

Complete PAY submission now freezes the exact sorted unique raw32 reference list
as LE u32 count plus identifiers in the same existing app.db batch as prepared
bytes, before signing. Recovery verifies exact size/order/nonzero identifiers and
SHA-256 equality to the signed evidence_root. Fresh nonempty service PAY cannot
be prepared without references. Retry can reuse the durably retained list; missing,
corrupt or mismatched lists fail without replacing prepared/signed bytes. PREPARE
and ACTIVATE cannot carry this PAY-only metadata. Empty PAY retains the exact
four-byte zero count. No wire, consensus field, hash domain or DB is added.

The list obeys the existing 4-MiB app.db value bound; oversize input is rejected
before preparation, never truncated. This is a local record bound, not an approved
retention horizon or scalable evidence batching design. Existing intermediate PAY
journals without this list fail explicitly; no live migration is authorized.

Verification: rebuilt core target; complete local Windows regression passed
331/331 cases and 154385/154385 assertions. New cases cover invalid/missing lists,
exact empty encoding, corruption without replacing signed bytes, signature reuse
and nonempty-reference recovery in the two-node PAY fixture. Evidence is retained
in ignored artifacts/economics-pay-references-build.txt and
artifacts/economics-pay-references-full-core.txt. No test is disabled.

This closes reference-list/body retention, not source-proof resolution. The
submission boundary still trusts the caller's collector references and service
claims; it does not load the two observations or prove their canonical assignment
provenance. The two-node fixture uses explicitly synthetic references. Replacing
the old caller-snapshot/per-chunk attestation collector prerequisite with accepted
canonical PREPARE/ACTIVATE material remains required before vertical acceptance.

### Exact action submission journal (2026-10-11)

StorageService now accepts complete PREPARE/ACTIVATE/PAY operations, retaining
exact prepared and signed bytes in the existing encrypted app.db. Journal scope
includes NetworkBinding, action and period; PREPARE adds funded term/epoch,
ACTIVATE adds preparation OperationID. PAY spans the whole period, so no term
suffix is appropriate. The existing entry-only caller delegates to this path.
Changed bytes in an occupied scope fail explicitly; retry reuses the retained
signature and checks exact finalized OperationID before candidate submission.
No DB transaction spans signing or relay. Stored unsigned input is not finality.

An existing period-only prepared/signed record blocks preparation for that period
with an explicit reconciliation error. Its bytes are neither interpreted,
migrated, overwritten nor deleted. This is isolated source development, not an
approved live-journal cutover or migration mechanism.

Verification: the rebuilt core target passed all 52 targeted StorageService and
assignment-attestation cases / 1915 assertions, including signature-save failure,
changed-body conflicts, prior-record preservation, app.db reopen, two-node PAY
finality and ALREADY_FINALIZED recovery of all three actions. This does not
exercise physical custody or the production collector scheduler. Evidence is
retained in ignored artifacts/economics-action-journal-targeted.txt. The complete
Windows core regression also passed 330/330 cases and 154274/154274 assertions;
see artifacts/economics-action-journal-full-core.txt. No tests were disabled.

This closes action separation and complete-body submission, not the complete
evidence journal: freezing/resolving the exact collector interval-reference list
with evidence_root, canonical collector provenance and production dispatch remain
open. The two-node scenario still uses PoA-attested fixture service totals.

### Canonical cumulative PAY development (2026-10-11)

The approved isolated source layout now replaces the aggregate daily PAY body
with period_end_utc, evidence_root, 113-byte entries keyed by funding OperationID,
slot, StorageId and payout, followed by sorted unique activation witnesses.
Execution resolves retained canonical assignments, enforces increasing service,
provider capacity and aggregate slot T, and requires exactly
`floor(B * cumulative_service / T) - finalized_paid`. Zero-amount entries can
record strictly increasing service; unchanged counters are rejected. There is
no recipient ranking, daily ceiling or replica-count limit on historical payees.
All effects are checked on a candidate state before publishing the result.

Each term retains sorted service/paid/closed-epoch-capacity records (89 bytes),
refunded onboarding/locked counters and accepted assignment history. The empty
term codec is 148 bytes; no historical decoder/migration is provided. Refunds
retain original B/T and origin provenance; term expiry and revocation no longer
erase financial checkpoints. Renewal can start after an expired retained term
without reusing its escrow. State validation checks paid against ledger totals,
entitlement, slot service bounds, capacity and exact residual origins.

This does not complete the whole atomic acceptance contract. The existing
off-chain observer/quote still requires its caller-snapshot/per-chunk attestation
inputs; a canonical collector consumer and exact action/evidence journal remain
open. The former live-provider daily-ranking producer was removed and now fails
explicitly instead of inventing cumulative counters or silently advancing an
empty period. The entry-only GUI submission cannot supply a real cumulative PAY.
The two-node PAY test attests service totals with the fixture PoA; it is not a
raw-observation-to-payment proof. No deployment or live record interpretation
is authorized by this source checkpoint.

Windows Qt diagnostic work adds explicit QtTest text and JUnit output files,
verbose CTest output, reported exit code and always-retained CI artifacts including
CTest's LastTest/LastTestsFailed logs. No Geo rewrite, timeout increase or disabled
test is involved; recording diagnostics is not a claim that Windows CI is fixed.

Verification completed 2026-10-11: core and native Qt test targets built; the full
local Windows core suite passed 329 cases/154174 assertions. Native Windows
QtTest through CTest passed all 95 results (including setup/cleanup), with no
failures or skips. The existing GitHub Windows failure was not reproduced here;
future CI retains the newly configured diagnostic files. The standalone offline
model also built and confirmed two CYBOU escrow/zero first-period entitlement for
the tiny two-replica term. The core target now carries its required Windows
Winsock library dependency, including for standalone consumers.

The finalized-node scenario includes an actual funded RootPublication, two real
binding proofs, PREPARE, later seed/key rotation, ACTIVATE and 30 signed PAY
periods on two independent in-memory executors. The first 29 periods retain
zero-payment service growth; the last pays one onboarding CYBOU per slot. Signing
and submission leave balances unchanged; finalized replay cannot pay again;
every imported block has equal roots and every paid snapshot reopens exactly.
Separate component scenarios verify provider replacement's shared B/T and
fractional refund, mixed-origin payments/refunds, independent renewal and
atomic rejection. These are not physical custody/12-hour scheduler acceptance.

Current fixed state-layout vectors (148-byte empty terms plus variable data):
PREPARE `0762dfdeed75bda796b79112da2c3076d0913729e0435222220de1592066b557`;
ACTIVATE `3742f90006ab43ac62b1336f93a83260696e9341b75afdafd894eebb14c7bb60`.
PAY limit vectors exercise 1024 entries/372 witnesses at 131055 tagged bytes;
373 witnesses exceed the 128 KiB bound. Counts are checked before allocation.

Ignored evidence: `artifacts/economics-cumulative-pay-full-core.txt`,
`artifacts/economics-cumulative-pay-qt.txt`, corresponding XML/CTest logs,
build-checked/build-regression logs and the offline model JSON. Preserve the
initial build FAIL (test Hash256 initializer-list narrowing), model-link FAIL
(missing standalone Winsock dependency) and core-initial FAIL (stale snapshot
reference, old codec vectors and obsolete randomized payout inputs). Corrections
refresh the fixture snapshot, assert new vectors and exercise both unattested
payout rejection and valid zero-service periods; no test is disabled.

### PAY capacity prerequisite (2026-10-10)

`ComputeAcceptedStorageCapacity` derives a per-term/slot/StorageId/payout
unit-second upper bound from retained canonical assignment epochs. It excludes
periods before first assignment and after replacement, permits later reassignment
without resetting the funded B/T, and clips at the exclusive requested boundary,
term expiry and lease closure. Invalid ordering, duplicate matching allocations
and arithmetic overflow fail closed. It neither mutates state nor treats capacity
as service evidence. Cumulative PAY and closed-term refund integration remain
open; the existing assignment-aware payment rejection stays in force.

Core build passed; four focused suites passed 45 cases and 128826 assertions
(283 unrelated cases skipped). Build and regression evidence are recorded in
`artifacts/economics-pay-capacity-build.txt` and
`artifacts/economics-pay-capacity-core.txt`. The initial FAIL is retained as
`artifacts/economics-pay-capacity-initial-fail.txt`: the earlier renewal codec
test still used the pre-assignment 112-byte term size and damaged an unrelated
byte instead of the count. The corrected test uses the current 128-byte empty
term layout and first asserts the count is two. This isolated helper checkpoint
changes no wire/state layout or live DEVNET process.

### Canonical eligibility and activation checkpoint (2026-10-10)

Isolated source development now includes PREPARE and ACTIVATE in the existing
PoA-signed StorageSettlement kind and signing domain. One action byte follows
period/start. PREPARE includes the funded term, epoch and exact sorted binding
proofs (at most 20); both STORAGE and current payout Authorization signatures are
verified before accepting term-scoped pairs/key epochs/heights. ACTIVATE requires
the latest declaration, exact height h+2 and chain-supplied nonzero parent seed.
It checks the ordered full manifest (at most 3988), publication count/root and
independently derives every chunk/slot assignment with the existing algorithm.
Accepted allocation summaries enter state serialization/root and survive decoding.
Neither action advances the payment cursor or moves CYBOU.

This is a partial isolated implementation, not the completed atomic contract.
At checkpoint `1c93de3f`, the PAY body/checker used the earlier publication/account/amount inputs;
it explicitly rejected any state containing accepted declarations/assignments.
It cannot debit or discard these checkpoints under the old daily rules.
Cumulative PAY/paid ledger and closure retention were not yet implemented at that
checkpoint; the later isolated PAY entry above records their implementation.
Action-scoped exact journal and canonical collector provenance remain required. Do not deploy this
intermediate wire/state layout or reinterpret existing live records.

State byte layout appended to each immutable funded term after its ten original
u64 counters: next_assignment_epoch u64, declaration_count u32 and declarations,
then assignment_count u32 and assignments. Each declaration is operationID32,
epoch u64, height u64, eligible_count u32, then sorted StorageId32/payout32/
key_epoch u64/accepted_height u64 (80 bytes each). Each assignment is activation32,
preparation32, seed32, epoch u64, effective_period u64, allocation_count u32, then
sorted slot u8/StorageId32/payout32/units u32 (69 bytes each). Decoder counts are
checked against remaining bytes before growing collections; semantic validation
checks ordering, references, foreign payout accounts and exactly U units per slot.
No provider registry, extra DB, signature/hash domain or P2P message was added.
The unchanged assignment algorithm and STORAGE proof verification now link from
the shared core rather than requiring node-local persistence/transport objects.
This relocation preserves all hash/signature domain bytes and existing wire
encodings; freeze/load of off-chain plans remains in the node/application layer.


Core and native Qt test targets built. The final regression passed 162 core
cases (165 unrelated cases skipped); native Qt authority review passed three
results including setup/cleanup. New scenarios cover real funded publication,
two provider bindings, independent in-memory Full Node finality/replay and equal
roots, unchanged state after signing/submission, h+1/h+3 rejection, actual payout
key rotation in h+1, retained accepted key epoch, ordered-root verification,
wire limits, and exact state reopen/corruption rejection. These are isolated
nodes, not independent physical host or paid-service acceptance.

Fixed canonical state-layout vectors are asserted in resource-limit tests:
PREPARE root `1ea29a211823069e662449359a6cdc7961d91fe15b91f30d156925cbb3d943ca`;
ACTIVATE root `ee36e5d86903cc6945f584861b6d9a4f98ee07a076f69853e9bbfdc9dda37843`.
These use the fixed key/account component fixture; the real finalized-node
scenario separately verifies execution provenance and equal roots.

Ignored evidence: `artifacts/economics-canonical-assignment-core.txt`,
`artifacts/economics-canonical-assignment-qt.txt`, checked/regression build logs
and the vector derivation log. Initial build FAIL (Hash256/array conversion)
and initial regression FAIL (negative test assigned an already possible unit
count) remain retained separately; the corrected test increments the actual
allocation instead. No runtime deployment or complete PAY acceptance is claimed.

### Approved atomic slice: parent execution context (2026-10-10)

The operator approved the concrete atomic PREPARE/ACTIVATE/PAY contract and its
effective-epoch rule for isolated implementation. This closes the placement
approval gate only; canonical action codecs, accepted declarations/allocation
state, cumulative PAY, collector provenance and action-scoped journal integration
remain unfinished. The dated proposal review below predates that approval.

BlockExecutor now receives the chain-supplied parent BlockID in candidate pool
execution, candidate root calculation, finalized execution and deterministic
conflict re-execution. Its seed check permits only preparation_height + 2,
rejecting missing parent IDs, earlier/later heights and arithmetic overflow.
This supplies execution context for future ACTIVATE enforcement; no existing
operation yet creates an accepted assignment or calls that check. No wire/state
layout or running DEVNET change is claimed by this prerequisite.


Core target rebuilt successfully. State, runtime, PoA and resource-limit
regressions passed 56 cases (267 unrelated cases skipped), including the new
exact-height/missing-parent/overflow fixture. Ignored evidence:
`artifacts/economics-assignment-parent-build.txt` and
`artifacts/economics-assignment-parent-core.txt`. Documentation manifest/link
checks also passed. This is execution-context evidence, not atomic settlement
acceptance.

### Assignment/witness protocol gate review (2026-10-10)

The current registry retains current keys, not a verified registry at every
historical seed. Existing encoded binding size is 6344 bytes; repeating 1024
complete binding proofs exceeds one 128-KiB operation. The economics plan now
contains a concrete PROPOSAL for existing StorageSettlement PREPARE/ACTIVATE/PAY
actions and canonical accepted binding/allocation summaries, not runtime changes.
Bindings would be validated before the later finalized seed; payment witnesses
would resolve retained canonical activation records rather than caller snapshots
or unavailable off-chain hashes. Placement/effective-epoch approval remains open.

An arithmetic-only dummy-byte check confirmed eight sizes and three exact
128-KiB boundaries. Evidence: ignored
`artifacts/economics-atomic-contract-layout-review.txt`. It is not a parser,
signature, state-root or runtime test. Atomic proposal capacity is only 20
complete eligible bindings / 3988 manifest chunks; bounded batches and scalable
Beta remain unresolved. No runtime, wire, crypto, state, genesis or live process
was changed by this review; DEC-292/DOC-020 acceptance is not claimed.

### Settlement preflight before durable preparation (2026-10-10)

Fresh StorageService submission now checks economic inputs against latest
finalized state before writing the period's exact journal. The runtime repeats
validation before signing. The shared read-only checker is also used by
ApplyStorageSettlement; canonical execution still requires the genesis PoA
signature. Wrong periods, missing/inactive leases, missing payout accounts,
self payouts, excess recipients and active-term/escrow excess are rejected
without a fresh prepared record or signature. Corrected input can then use
the period. Retained records still follow exact replay/reconciliation and are
never silently replaced if state advances after preparation.

No wire/state layout, current cap, balance semantics, DB/service/worker or live
DEVNET changes. Cumulative service/paid and exact assignment/evidence witness
contracts remain open; this is not cumulative payout acceptance.

Windows core and native Qt test targets built. StorageService, economics quotes,
resource limits, runtime and PoA regression passed 79 cases / 8,540 assertions
(243 unrelated cases skipped). The new fixture rejects self payout, missing
account/lease, escrow excess and wrong period before signing or preparation,
preserves the state root and then accepts corrected input. It also checks that
read-only preflight does not authorize unsigned canonical execution. Qt authority
review passed (3 results including setup/cleanup). Ignored evidence:
`artifacts/economics-settlement-preflight-{core,qt}.txt`, final build log and
the separately retained initial build FAIL from a corrected test-only Hash256
narrowing initializer. No runtime failure or live network acceptance is claimed.

### Exact settlement operation retention (2026-10-10)

The isolated source tree routes desktop settlement review/submission through the
existing Identity session worker and StorageService. Existing encrypted app.db
retains prepared input and exact signed operation bytes under NetworkBinding and
period before candidate submission. Review reuses original entries/UTC start;
changed input for that period fails closed. Signed retries retain OperationID;
verified finalized replay returns without submitting or paying again. Failed
signed persistence and corrupt records retain preparation and reject submission.
The separate desktop settlement worker and runtime combined sign/submit API
were removed. No extra database, wire fields or live deployment.

This addresses retention for the current wire format. Target cumulative
service/paid execution, evidence references, batch reconciliation and retention
policy remain open; DOC-020 and production crash recovery are not accepted.

Native Windows core/Qt test targets built. StorageService regression passed
28 cases / 656 assertions. Persistent app.db reopen plus a fresh verifier pool
retains exact signed bytes through uncertain delivery and finalized replay;
the original fixture signer retains its signing journal. Signed-save failure,
conflicting preparation and corruption fail closed. Qt authority review passed
(3 QtTest results including setup/cleanup). Evidence: ignored
`artifacts/economics-settlement-journal-{core,qt}.txt` and build logs.
The initial fixture FAIL is retained separately: it incorrectly tried to sign
after importing blocks into a fresh signing journal; safety correctly halted.
The corrected fixture independently verifies the original signer's block,
without recreating signing history or weakening checks. This is isolated
component/integration evidence, not forced-process termination acceptance.
Runtime/PoA regression additionally passed 30 cases / 429 assertions
(`artifacts/economics-settlement-journal-runtime.txt`), including candidate
validation, durable signing conflicts and canonical-history mismatch rejection.

### Verified observations to service intervals (2026-10-10)

The operator adopted isolated Beta checks every 12 hours, maximum credited gap
24 hours, and first/replacement plus every-eighth-success full GET. The existing
observation collector now accepts a funded evidence scope and derives period-split
service claims in the existing assignment-evidence journal. First checks earn no
time; failures/long gaps/interrupted requests break continuity. Failed checks
preserve the successful ordinal and GET deadline, including across store reopen.
Planned-check cadence is a constant, not an implemented production scheduler.

A 41-byte checkpoint intent in existing encrypted app.db is durably committed
before I/O, with successful predecessor cleared. Successful raw observations,
index, interval claims and completed checkpoint commit atomically after I/O;
shared-claim overlap or completion failure rolls them back together, leaving
the conservative pending intent. Missing/corrupt checkpoints and bad scope fail
closed. Network calls hold no DB transaction. No database/service/worker, wire,
canonical state or live process changed. Caller-funded-term/seed/historical-key
provenance, canonical service/paid, cumulative settlement and exact signed
settlement-operation recovery remain open. DOC-020 is not accepted by this change.

Native Windows core target built. The final assignment-attestation, assignment
and StorageService regression passed 53 cases / 1,453 assertions; 266 unrelated
cases skipped. Three new tests exercise real GET/audit ingestion into the existing
cumulative quote (day one zero; complete 30-day replica term one CYBOU), failure
and gap boundaries, GET deadline/store reopen, new-epoch first GET, interrupted
intent and atomic rollback on overlapping claims. These are controlled transport
fixtures with signed assignment/receipt; not finalized payout, verified active
publication, independent-host or live DEVNET acceptance. Evidence:
`artifacts/economics-service-policy-build-final-check.txt` and
`artifacts/economics-service-policy-regression.txt`. The initial fixture build
failed due to std::array CTAD producing a ChunkId instead of a one-element
manifest; fixed explicit type, with diagnostic excerpt retained in
`artifacts/economics-service-policy-build-initial-fail.txt` (no tests ran then).

### Portable assignment-attestation encoding (2026-10-10)

The audit at `5d10a355` still correctly identifies the missing cumulative
settlement vertical slice. The existing attestation module now has exact bounded
structural encoding/decoding (3562 + 64 bytes per eligible pair), with canonical
order, exact byte consumption and count checks before allocation. Full eligible
sets of 1024 encode to 69,098 bytes. Selected pairs/commitment use the existing
assignment algorithm and domains. Binding verification now rejects duplicated
proofs. This component has no new database or production settlement caller;
decode is not cryptographic or finalized-seed verification. The complete
settlement witness envelope, canonical service/paid and removal of the daily cap
remain unfinished; DOC-020 remains open. No deployment or live state changed.

Native Windows core target build passed. The full attestation suite passed
16 cases / 323 assertions (300 other cases skipped), including two new codec
cases and existing binding, journal/reopen, observation and payout-quote coverage.
The assignment suite separately passed 8 cases / 405 assertions (308 skipped).
Logs: `artifacts/economics-attestation-codec.txt` and
`artifacts/economics-assignment-codec-regression.txt`. This is focused component
regression, not a full core run, interoperability test or settlement acceptance.

### Repeat-audit CI lifetime follow-up (2026-10-10)

The repeat audit reviewed `43220316`; active-term debit protection at `2969c1f7`
already supersedes its aggregate-only spending observation. Per-slot/provider
cumulative service/paid and verified settlement execution remain open; this
follow-up does not implement or accept them.

The connect-back P2P test destroyed node B before its Stop guard joined the
listeners/workers borrowing B. Removed that early reset; normal scope destruction
now joins listeners/workers before destroying servers and B. No production
runtime, protocol, consensus, genesis or live process changed. Ten independent
Windows runs of the unchanged test assertions passed (four assertions each),
evidence `artifacts/p2p-listener-lifetime-repeat.txt`.

Core CI now uses pipefail plus tee for the full suite/report and uploads the log
with an always-running, pinned artifact step, keyed by commit SHA. Failure status
is preserved; missing logs warn instead of masking the original build/test error.
YAML and retention/pipefail structural checks passed. The historical failed
[Core run at 43220316](https://github.com/cybou-fr/cybou/actions/runs/37992906342)
remains FAIL evidence; local checks do not establish green Core/Desktop CI.

Linux WSL build with GCC 15.2.0, `-O1 -g1 -fsanitize=address,undefined
-fno-omit-frame-pointer` and sanitizer linker flags passed. ASan leak detection
and halt-on-error plus UBSan halt-on-error/stack traces were enabled. All 38
P2P suite cases / 1,185 assertions passed; 283 other Linux core cases were
skipped. Evidence: `artifacts/p2p-lifetime-asan-build.txt` and
`artifacts/p2p-lifetime-asan-suite.txt`. Initial configuration failed because the
BLAKE3 prefix was absent; the existing local dependency path fixed it, with the
failure retained in `artifacts/p2p-lifetime-asan-configure-initial-fail.txt`.
No old failing revision was rerun under sanitizers; these results validate the
corrected teardown and do not reproduce/prove the historical CI crash trace.
Five additional independent Linux ASan/UBSan runs of the connect-back test
passed all four assertions each, with leak detection enabled. Evidence:
`artifacts/p2p-lifetime-asan-repeat.txt`. Together with ten native Windows repeats
and the full sanitizer P2P suite, this is bounded local regression evidence;
new GitHub Core/Desktop runs on the committed HEAD remain necessary.

### Active-term finalized debit accounting (2026-10-09)

Approved isolated development now records paid onboarding/locked totals in each
funded term. ApplyStorageSettlement debits only the term covering its period,
uses its frozen rate/duration for the existing daily cap, and validates all
entries before changing balances, paid totals or cursor. Future renewal money
cannot cover an exhausted earlier term; a later term's onboarding origin cannot
replace the active term's locked origin. StorageService settlement preparation
uses the same active-term residuals and frozen parameters.

State codec/hash includes both counters (112 bytes per term). Validation requires
each counter within its original origin, onboarding-first consumption within the
term, and aggregate escrow exactly equal to the sum of term residuals. Original
funding and paid counters are not extra monetary supply. Renewal appends a term
without resetting earlier paid totals. Closure still refunds all remaining
origins and erases the lease and its history.

This is monetary debit accounting, not provider-specific cumulative service/paid
enforcement. The old daily ceiling still permits premature payouts. Canonical
assignment/evidence witnesses, entitlement arithmetic, bounded period batches,
retention/closure and exact-operation recovery remain open. No DEVNET deployment;
nonempty deployed snapshots remain incompatible with this source-tree format.

Validation: core build passed. The two new signed settlement regressions passed
88 assertions (312 other cases skipped), including the existing production
StorageService -> PoA/runtime -> finalized-block execution path in memory-only
fixtures. Evidence: `artifacts/economics-active-term-debits-targeted.txt`.
The first targeted invocation selected no cases due to a test-filter syntax
error, retained in `artifacts/economics-active-term-debits-filter-error.txt`;
the corrected selector ran both cases. This is not live provider/evidence or
desktop acceptance.

Full core regression at baseline `43220316` plus this package passed all 314
test cases and 141,112 assertions, with no skipped cases. Evidence:
`artifacts/economics-active-term-debits-core.txt`. Documentation manifest/link
checks passed. No official network, genesis, key, signing history or live
process was changed.

### Canonical immutable funded-term history (2026-10-09)

This subsection records the earlier `e6ade856` layout and evidence. The active-term
follow-up above supersedes its 96-byte layout and aggregate-only debit accounting.

Full core regression follow-up at `e6ade856`: all 312 test cases and 140,641
assertions passed, with no skipped cases. Evidence:
`artifacts/economics-canonical-terms-full-core.txt`. This broadens the focused
regression evidence below; it does not establish assignment/evidence payment
validation, desktop acceptance or live DEVNET compatibility. No runtime code
or live processes changed in this follow-up.

The approved isolated source-tree transition now retains StorageFundedTerm
records within each existing lease. Initial publication and authorized renewal
execution supply their exact OperationIDs and canonical parameters. Renewal
appends a separate budget/denominator/origin record, preserving earlier terms.
Current lease escrow remains the sole live balance; original funding is not
another obligation ledger or added monetary supply. Existing state serialization,
hashing, decoding and validation include the 96-byte term records.

Missing terms, duplicate funding IDs, discontinuous periods, inconsistent budget
or provenance, truncated/count-overrun bytes and live-origin escrow above original
funding fail. Genesis with no leases has unchanged bytes. Nonempty deployed lease
snapshots are incompatible; no automatic migration, legacy decoder or live
restart/deployment is authorized. Term-specific paid/service, evidence/proof/batch
execution and correct term-aware settlement/refund/closure remain open. Old
settlement still consumes aggregate escrow and deletes a closed lease; this is
not complete economics or an activatable protocol transition.

Validation: core build passed; focused economy/quotes, state, RootPublication,
runtime/PoA, operation codec, network genesis, KV store, resource limits and
storage/publication/application suites passed 132 cases / 137,951 assertions;
180 other cases were skipped. Evidence: `artifacts/economics-canonical-terms-core.txt`,
baseline `fc134c7e` plus this package. Initial run failed one of 132 cases because
the overflow fixture manually corrupted lease.end_period without its funding
history (`artifacts/economics-canonical-terms-initial-fail.txt`). The repaired
fixture creates a valid near-limit term through canonical publication execution;
renewal overflow still rejects before debit. Signed publication/renewal tests
cover immutable term copies, exact funding IDs, codec/hash round-trip, bad budget,
duplicate/gapped terms, truncated/count-overrun/missing history and duplicate
funding without mutation. Isolated fixtures, no live deployment or E2E payouts.

### Approved isolated canonical funding change (2026-10-09)

Operator approved consensus-code development in isolated fixtures, without
deployment or changing running DEVNET. ComputeStorageLeaseEscrow now uses
ComputeAssignedStorageBudget's separately rounded replica shares in the source
tree. Existing ApplyRootPublication and ApplyStorageLease call this same function,
as do application publication/renewal quotes. One chunk, two replicas and 30
periods now reserve two CYBOU rather than one. Fees/rate parameters, operation
bytes, official genesis and live processes are unchanged.

This is a partial protocol-development checkpoint and must not be deployed:
current settlement still uses its old daily cap, leases still merge renewals,
and separate canonical term/assignment/service/paid records plus proof/batch
validation remain pending. The early-payment regression remains explicitly
visible, rather than being presented as solved by larger escrow. This package
does not close DOC-005/006/020 or establish completed economics acceptance.

Validation: core build passed. Focused economy/quotes, state, RootPublication,
runtime/PoA, operation serialization, storage/publication/application suites
passed 113 cases / 129,556 assertions; 199 other core cases were skipped.
Signed initial-publication and renewal regressions check both one and 2048
chunks, quote/debit/escrow agreement, Treasury fees and conservation. Arithmetic
vectors cover 1–2048 units over 1/2/30/365 periods. Evidence:
`artifacts/economics-canonical-funding-core.txt`, baseline `326addb2` plus this
package. Isolated test genesis/state fixtures, not live independent-host payout
or CI/Desktop acceptance. No desktop executable or node was deployed/restarted.

### Complete funded-term quote (2026-10-09)

storage_assignment_payout now prepares all funded replica slots under one outer
app.db snapshot with the same assignment/binding/evidence checks. Missing/mixed
slots fail; finalized-paid/due totals cannot exceed total target escrow; the 1024
entry/input limit applies globally. Shared claim streams reject overlapping use
of the same StorageId or payout account across replica slots and epochs. Distinct
identities do not establish independent hosts. The result is read-only target
arithmetic, not an emitted settlement or current canonical state transition.
Provenance, active epoch/time/audit policy and separately reviewed activation
remain open.

Validation: core build passed; focused assignment attestation/payout and private
store suites passed 19 cases / 368 assertions; 293 other core cases were skipped.
Regressions cover missing slots, complete two-slot totals, finalized-paid inputs,
read-only retries, mixed scope, nested transactions, global input limits and
overlapping economic/storage identity reuse across replacement epochs.
Evidence: `artifacts/economics-term-payout-core.txt`, baseline `05ca1fee` plus this
package. Synthetic signed component fixtures are not live funded acceptance.

### Read-only cumulative slot payout preparation (2026-10-09)

New storage_assignment_payout joins the complete authorized chunk/epoch manifest,
stored genesis-attested plans and shared/local interval accounting. Unresolved
old epochs, duplicate chunks/plans, mixed funded scopes and corrupt state fail.
Service is grouped by StorageId/payout account across epochs and chunks, while
all providers share one funded slot budget. Provider-specific cumulative floors
minus caller-supplied finalized paid produce sorted entries; paid input conflicts,
overpayment and aggregate budget overflow fail, without partial truncation.
It is read-only target preparation, with no mutation of paid, period advancement
or wire submission. Preparation now reloads raw bindings and rechecks both
signatures against supplied historical registry snapshots for every seed. Missing,
duplicate/null snapshots, wrong keys or corrupt bindings fail. Canonical
manifest/rate/paid and historical snapshot provenance, interval policy and
production/state activation remain open.

Validation: core build passed; focused assignment/attestation, private store,
storage service/economy, quotes and resource limits passed 76 cases / 128,716
assertions; 234 other core cases were skipped. Quote regressions cover day-one
zero versus full 30-period entitlement, future-period exclusion, read-only retries,
reopen/finalized paid, overpayment/duplicate paid, omitted epoch/chunk, duplicate
manifest, mixed policy and separate provider floors under one replacement budget.
Evidence: `artifacts/economics-slot-payout-core.txt`, baseline `c7540f61` plus this
package. Fixture publications/intervals are synthetic, not live funded acceptance.

Historical-binding follow-up validation: focused core suites passed 76 cases /
122,626 assertions; 234 other cases were skipped. Missing/wrong snapshot, wrong
registry keys and corrupt raw binding reject preparation; restoring the binding
restores the valid quote. Evidence: `artifacts/economics-historical-binding-core.txt`,
baseline `77db7db8` plus this package. This does not prove the external snapshot's
chain provenance or live funded settlement acceptance.

Durable quote follow-up: enclosing app.db transactions now reject preparation,
preventing uncommitted interval data from producing a successful payout quote.
Core build passed; focused assignment attestation/payout and private-store suites
passed 18 cases / 329 assertions, with 293 other core cases skipped. Regression
checks staged append/rollback, then committed service across two chunks with one
provider and one slot budget (one CYBOU due, not one rounded share per chunk).
Evidence: `artifacts/economics-durable-quote-core.txt`, baseline `fcc7abe7` plus this
package; synthetic component evidence, no live settlement activation.

### Cross-epoch funded-slot exclusion (2026-10-09)

Assignment interval accounting now atomically maintains a shared funded-slot
journal across replacement epochs. Its scope includes network, publication,
chunk, payer, separately funded term and replica slot; epoch/seed/provider cannot
create a second budget. Overlap and reused proof are rejected across epochs.
Counters require exact correspondence between local intervals and shared claims;
missing/corrupt records and inconsistent UTC anchors/durations fail closed.
Shared claims are bounded without eviction. Active epoch boundaries, authenticated
observation-to-interval policy and canonical funding/payout activation remain open.

Validation: core build passed; focused assignment/attestation, private store,
storage service/economy, quotes and resource limits passed 73 cases / 122,583
assertions; 234 other core cases were skipped. Replacement-epoch regression checks
overlap/reused proof rejection, adjacent service, independent replica slots,
concurrent conflicting writes with exactly one winner, reopen, anchor mismatch
and missing shared journal rejection even for a fresh epoch. The full 4096-entry
fixture validates matching local/shared records without thousands of disk commits.
Evidence: `artifacts/economics-funded-slot-core.txt`, baseline `bebf740b` plus this
package. No live funded lease or protocol activation acceptance is claimed.

### Durable assignment observation collector (2026-10-09)

The new storage_assignment_observation_store calls assigned audit/GET and atomically
stores successful observations and sorted UTC index in existing encrypted app.db.
It requires the stored PoA attestation, releases the DB lock during network I/O,
rechecks state before commit and rejects nested transactions. Records are immutable
per assignment/slot/UTC second, with 4096 records maximum and no eviction. Exact
retries reuse a record; conflicts and malformed indexes/targets fail closed.
Reload checks receipt, scope and exact local chunk bytes; audit response is
recomputed from retained raw challenge/answer. GET ciphertext is not duplicated.
Its stored success/time remains a trusted local collector assertion, not signed
proof of past remote custody. Canonical UTC/funded term validation, production
dispatch/aggregation and interval/payout wiring remain open.

Validation: core build passed; focused attestation/assignment, private store,
storage service/economy, quotes and resource limits passed 72 cases / 122,800
assertions; 234 other core cases were skipped. Regressions cover DB reopening,
raw audit replay/tamper rejection, concurrent identical GET collection, UTC and
nested-transaction rejection, missing indexed records, oversized index, locked
Identity and failed transport without a successful journal entry. Evidence:
`artifacts/economics-observation-store-core.txt`, baseline `1af0e171` plus this
package. Controlled transport fixtures do not establish live funded acceptance.

### Assignment-scoped audit and full GET (2026-10-09)

The new storage_assignment_observer uses existing StorageTransport after checking
the PoA-attested slot, StorageId and signed receipt. Audit uses a fresh random
nonce/offset and exact local ChunkID/size reference. Unavailable audit falls back
to exact full GET; negative or incorrect audit responses fail. Returned data is a
local instantaneous observation, including receipt and raw challenge/answer or
verified GET bytes. No interval credit, persistent observation log, automatic
production dispatch or payment is introduced. Existing runtime transport proves
the requested StorageId; controlled transports in tests implement the interface.
Live funded leases, raw evidence retention and PoA aggregation remain open.

Validation: core build passed; focused attestation/assignment, private store,
storage service/economy, quotes and resource limits passed 69 cases / 122,800
assertions; 234 other core cases were skipped. Controlled transport tests cover
fresh challenges, explicit negative/incorrect answers, absent-audit GET fallback,
forced GET, corrupt/truncated/missing bytes and rejection before any network call.
Evidence: `artifacts/economics-assigned-observer-core.txt`, baseline `fd6d45de`
plus this package. No live network acceptance or deployment is claimed.

### Assignment attestation and binding/receipt verification (2026-10-09)

New storage_assignment_attestation helpers check both payout-binding signatures,
complete eligible-set membership and matching snapshot ID. Signing checks the
verified genesis NetworkBinding/PoA key, freezes the plan before signing, verifies
the result, and atomically saves signature plus original signed bindings in
existing app.db. Cached retry reuses the signature and validates retained proofs;
reopening can replay proofs against the original finalized registry. Wrong signer,
tampering, wrong account/storage key or corrupt records fail closed. Exact existing
receipt verification now also checks the assigned StorageId and genesis-attested
plan; it grants no interval credit. Calls inside an enclosing store transaction
are rejected before signing, because a savepoint cannot guarantee durability.

Tests use actual hybrid signatures, real StoragePayoutBinding/receipt generation
and replayed finalized Identity state on synthetic fixtures. They cover failed
signing/retry, reopen, exact cached signature, forged inputs, store lock, corrupt
journal, wrong receipt provider/publication/size/slot. Publication/lease provenance,
health/budget/independence validation, raw audit/GET ingestion, production PoA
dispatch and current settlement/state activation remain separate open gates.

Validation: core build passed; focused assignment attestation/assignment, private
application store, storage service/economy, quote and resource-limit suites passed
67 cases / 126,847 assertions. The other 234 core cases were skipped. Evidence:
`artifacts/economics-attestation-core.txt`, baseline `f5cbf610` plus this package.
No live funded assignment, desktop deployment or network activation is claimed.

### Assignment-bound evidence accounting (2026-10-09)

The new storage_assignment_evidence module persists interval references under
frozen assignment commitment/replica slot in existing encrypted app.db. It validates
period/UTC scope, rejects overlap or altered proof reuse, serializes concurrent
appends, bounds records without truncation, and returns cumulative seconds through
a specified period. Future intervals are excluded from earlier queries; reopen
retains past service without requiring the provider to be online. Empty readable
records return zero; corruption/lock/scope mismatch fail closed.
Tests exercise idempotency, late arrival, partial/gapped intervals, future/period
rejection, replica isolation, reopening, arithmetic integration, unavailable
store access, corrupt bytes, time overflow, concurrency and 4096-entry bounds.
The tests use synthetic proof references and do not verify actual storage audits
or PoA signatures. Cross-epoch slot exclusion, raw-evidence verification/retention,
signed assignments and live settlement integration remain open. See
[the accounting contract](ECONOMICS_SETTLEMENT_COMPLETION.md).

Win/MinGW scoped assignment/evidence, storage service/economy, quote and resource
limit suites passed 58 cases / 123,189 assertions; 239 other core cases were
skipped. Log: artifacts/economics-assignment-evidence-core.txt (ignored). Store
reopening and concurrent calls are covered; process termination, power loss,
actual provider proofs and production deployment are not claimed by this run.

### Deterministic assignment preparation (2026-10-09)

New off-chain storage_assignment helpers accept a bounded canonical candidate
snapshot and fixed context/seed, select distinct payout identities and StorageIds
with deterministic rejection-sampled shuffles, and commit the complete input and
result. Immutable plans are saved in existing encrypted app.db, survive reopening
and reject conflicting replacements for the same chunk/epoch/term. Endpoints are
excluded. This is target preparation, not PoA-signed assignment, verified
eligibility, physical independence, activated placement or finalized payout.
DEC-280's current CSPRNG placement and current settlement wire remain unchanged.
Tests cover replay, input permutation/aliases, distinct payout selection, invalid
or oversized snapshots, immutable retry/conflict, reopen and network/epoch/term
isolation. See the exact local transcript in
[the completion contract](ECONOMICS_SETTLEMENT_COMPLETION.md).

Win/MinGW assignment/service/economy/quote/resource-limit suites passed 54 cases
(artifacts/economics-assignment-core.txt, ignored). After adding the independently
reproduced fixed commitment vector, the final assignment suite passed 3 cases /
330 assertions (artifacts/economics-assignment-vector.txt). These results are not
added together. Production executable deployment and live PoA acceptance were
not performed; this package activates no new settlement or placement rule.

### Evidence persistence and interval safety (2026-10-09)

Existing EvidenceLedger now writes provider record/index/eviction in one store
batch before mutating memory. Failed writes and wrapped counters fail closed.
Replica verification and receipt admission require successful evidence saves;
failed saves close the volatile credited interval rather than report a verified
replica. Serialized interval accounting ignores duplicates/late results without
rewinding its cursor. A gap longer than 24 hours credits zero and establishes a
new baseline; restart preserves already credited totals and excludes downtime.
Tests exercise ordered/late/duplicate observations, failure boundaries, reopened
ledger totals, unavailable encrypted-store access and counter overflow. They
do not simulate disk-full, power loss, signed assignment or live PoA payouts.
The bounded provider shadow counters remain diagnostics: no new payable evidence
or settled balance is inferred, and assignment-bound durable intervals remain open.
Scoped Win/MinGW core suites passed 51 cases / 121,802 assertions (storage
service/economy, economics quotes and resource limits); 239 other cases were
skipped. Log: artifacts/economics-evidence-safety-core.txt (ignored). No production
desktop/VPS deployment or new live-provider acceptance was performed.

### ECONOMICS-P0-01 initial accounting package (2026-10-09)

DEC-292 sets economics before Beta hardening. Pure target helpers compute funded
whole-CYBOU replica shares and cumulative verified-unit-second entitlement minus
finalized paid; they are not connected to current canonical funding/execution.
Invalid counters/remainders and overflows fail closed. Two-replica funding adds
at most one CYBOU over current combined ceil in the checked 1–2048 unit,
1/2/30/365-period matrix. Tests also cover 1/2/1000 shares, partial service,
missing intervals and repeatable calculation from supplied counters. These are
arithmetic tests, not durable-ledger or process-restart evidence.

StorageService settlement preparation no longer truncates above 1024 entries:
it throws an explicit preparation error. The Qt adapter reports failure and the
controller does not review/submit an empty success or advance the period. The
service regression checks rejection under a reduced preparation limit and exact
boundary success without changing the subsequent result. Qt static library built;
no new GUI runtime acceptance or production executable deployment is claimed.

Scoped Win/MinGW core suites passed 49 cases / 122,020 assertions:
storage economy, economics quotes, storage service and resource limits.
Ignored log: artifacts/economics-p0-01-core.txt. Other 239 core cases were not run.
DOC-020, canonical term/paid serialization, assignment/evidence integration,
recoverable settlement execution and coordinated transition remain open under
[the completion contract](ECONOMICS_SETTLEMENT_COMPLETION.md).

Follow-up offline simulation emits JSON for five workloads at 30/90/365 periods,
provider sensitivity and onboarding Treasury exposure, and a renderer produces
the dated EVIDENCE report. Source sizing templates contain placeholders and are
never dispatched; no node or signer runs in the simulation. Chunk bounds are
checked against actual encrypted tree construction, including an INDEX-bearing
41 MiB source. The small-lease test executes a signed canonical settlement and
confirms current acceptance of first-period exhaustion of 30-period escrow,
preserving monetary mass and onboarding payout origin. DOC-020 records this gap;
the deployed settlement arithmetic is unchanged. Renewal quotes now reject
periods beyond the existing consensus maximum.

Updated scoped suites passed 15 cases / 86,568 assertions and the standalone
simulation CTest passed: artifacts/economics-simulation-core.txt (ignored).
The report distinguishes modeled accrual/refunds from unknown actual payouts,
shows per-object rounding and separate renewal fees, and does not establish
provider independence, attack cost, a new tariff decision or live economics.

The pure economics_quote module returns publication fee, initial escrow and
total immediate System Balance debit using current consensus helpers. Its exact
overload serializes the authorized operation; renewal quotes include the
StorageLease protocol fee and additional escrow. No tariff, settlement rounding,
wire or state field changed. Wallet/Compose/Files previews are not connected yet.

Scoped quote/storage-economy/root-publication suites passed 13 cases / 86,516
assertions on Windows/MinGW: artifacts/economics-quote-core.txt (ignored).
The four new cases exercise valid/invalid/overflow inputs, 512 KiB/1 MiB/1 GiB
billing boundaries, parameter-derived renewal cost and comparison with signed
RootPublication plus StorageLease consensus execution, Treasury/escrow changes
and monetary conservation. Existing accumulator tests contribute most assertion
counts; this is not a live publication, tariff study or Linux acceptance result.

## Forced process termination and disk recovery (2026-10-09)

The process suite now also covers three Identity rotation checkpoints: both
application data keys wrapped for future access before submission; canonical
rotation finalized while the original vault remains active; and candidate vault
promoted before process termination. Fresh-process recovery checks access to
acknowledged draft/Mail/Outbox and an app.db sentinel, reconciles the rotation via
IdentityService and verifies the promoted recovery key. Prepared wrappers preserve
old-vault access until promotion, and the promoted vault opens the same data.
All six scenarios passed: artifacts/local-rotation-crash-first.txt (ignored).
This is serial disk-backed core/service acceptance, not concurrent desktop key
rotation, a crash inside vault replacement, or loss of both wrapping keys.

A BUILD_TESTS-only child executable and Python supervisor now exercise actual
process termination (Windows TerminateProcess / POSIX SIGKILL), without destructor
or normal shutdown assistance. Each isolated synthetic fixture uses disk-backed
runtime, signing/coordinator journals, vault, local.db, app.db and encrypted blobs.
The recovery phase opens these in a fresh process without wiping runtime data.
No official DEVNET state, transport connection or operator signer is involved.

Three scenarios pass: after staging before local acceptance (no sent Mail and
orphan retention released); after acknowledged local commit (draft, Mail, Outbox
and encrypted content survive); after submission before local status persistence
(same OperationID restored, finalized once, retries do not advance the account
nonce a second time). The third scenario additionally removes the network job
record after reopening to exercise the independent local recovery copy.

The CTest entry cybou_local_crash_recovery passed all three checkpoints on
Windows/MinGW in 2.16 seconds. Evidence: artifacts/local-process-crash-first.txt
and artifacts/local-process-crash-ctest.txt (ignored). These checkpoints do not
establish power-loss durability, arbitrary instruction-boundary crash safety,
arbitrary crashes inside key rotation, concurrent shutdown or large-file desktop acceptance.
The production executable and running desktop are unchanged by this test package.

## Active Outbox indexing (2026-10-09)

Follow-up exact-job recovery: PublicationService mirrors Outbox-owned prepared
jobs and intents into local.db before submission; the Qt session supplies its
separate local store. NetworkSync restores missing app.db jobs from this copy,
validates bundle/Identity/network/OperationID and canonical finality, and preserves
known OperationID on failure. A different newly signed ID cannot replace an
existing publication ID. Publication records and their app.db index now commit
atomically. This does not migrate arbitrary headless jobs or eliminate the
coordinator's separate signed-operation journal.

Deterministic local/publication/coordinator/application suites passed 36 cases /
626 assertions with the recovered-finality check. The Qt adapter static library
also rebuilt successfully; the running desktop executable was not replaced.
Scenarios simulate loss of a job with stale local
status, mismatched/corrupt backup rollback and service reconstruction after
finalization. They are not process-kill or complete loss of both databases and
the coordinator journal. Evidence: artifacts/outbox-recovery-core.txt (ignored).

The local store retains an encrypted active-job index alongside immutable
completed history. Acceptance appends membership in the local transaction;
status transitions update membership atomically. Protected jobs can be
reactivated in original acceptance order. Existing stores initialize the index
once from validated history. PendingOutbox(limit) decodes only the selected
active payloads; NetworkSync no longer decodes completed history each pass.
The network pass still processes all active jobs to avoid starving later intents
behind jobs securing replicas. A bounded scheduling cursor remains open.

Scoped local/application core suites passed 22 cases / 439 assertions:
artifacts/active-outbox-core.txt (ignored). Tests cover selection limits, service
restart with retained store, index reconstruction, reactivation order and an
invalid completed payload excluded from active reads. They do not establish
process-kill recovery, 10,000-job real-disk performance or the cross-database
exact-operation crash guarantee. Desktop deployment was not part of this check.

## Independent local content preparation (2026-10-09)

Upload and new local Mail attachment preparation run on the LocalContentStager executor.
Short local Mail/catalog commands retain their separate executor. The staging
journal mutex no longer covers an entire encrypted stream; independent metadata
preparation can progress while a file source waits. Shutdown requests cancellation
between source reads/chunk writes and joins staging before stopping local commits.
An OS read already in flight is not cancellable through this helper.

Acceptance of a send preserves draft edits made during preparation. The same
fingerprint check protects draft retirement during retry. Content commits advance
the local snapshot revision so an older network snapshot cannot replace them.
The draft snapshot is saved in short-command order before waiting for preparation;
plain Mail and reused content references do not wait behind file encryption.

Scoped tests hold a content source while 50 Mail moves, draft save and folder
acceptance complete; verify preservation of newer draft edits; and stop an
unbounded source before Outbox acceptance with staging ownership cleaned up.
Updated local/application suites passed 21 cases / 421 assertions. Eight scoped
Qt scenarios plus setup/cleanup passed (10 total, no failures/skips), including
end-to-end Mail/Files, blocked transport, rotation, commit acknowledgement, Undo
and offline retrieval. Evidence: artifacts/local-content-executor-core.txt and
artifacts/local-content-executor-qt.txt (ignored). Desktop rebuilt; documentation
and manifest structural checks pass. Older full-suite evidence retains its own
revision scope; this update does not claim another full-suite run.
This is not yet evidence for every physical-disk failure or process-kill boundary.

## Local/Network separation in progress (2026-10-09)

DEC-291 is an accepted architecture target. The working implementation separates
the local executor and encrypted local.db from the network executor and app.db.
Mail folders/flags/drafts and Files desired mutations commit locally; publication
runs from immutable Outbox with stable jobs. SessionScheduler is removed.
Destination migration is atomic and leaves app.db untouched. Key mismatch no
longer deletes app.db. Wrapped key access is prepared before IdentityRotate.
Staging records ownership before pinning; reopening releases interrupted staging
without a durable intent and preserves pins owned by accepted Outbox entries.
Publication takes its ordinary retention reference before local Outbox pins are
released; local content remains pinned until normal cancellation/revocation.
Retry reuses the same exact OperationID, including after finalization.

Scoped Windows Qt evidence: with the real adapter and deliberately blocked
StorageTransport, 50 Mail moves, draft save, folder creation and local send
acceptance completed before releasing transport; reopening preserved local data.
Move logs: queue 0 ms, local save 1–2 ms, GUI acknowledgement 7–20 ms on this
fixture, not a claim about the operator's full live mailbox. Existing end-to-end
Mail/Files integration passed. Core tests cover atomic migration rollback/retry,
source preservation, rotation access, Outbox restart, source-file removal and
older confirmation preserving a newer desired name.

Validation: full core run passed 274 cases / 98,441 assertions before the final
retention handoff change; the updated local/publication suites passed 14 cases /
143 assertions including immutable bundle identity and exact-operation reuse.
Desktop and native Qt targets rebuilt. Initial full Qt run failed with SIGSEGV
in rotation (debug evidence: artifacts/local-network-rotation-gdb.txt, ignored).
The old draft replay called a not-yet-created local executor during session reopen;
that replay is removed because durable local.db retains drafts. The rotation
regression then passed. This failed run remains evidence, not Beta acceptance.
The subsequent complete native Windows Qt run passed 95 cases, zero failures or
skips (artifacts/local-network-full-qt.txt, ignored), including blocked transport,
rotation and end-to-end Mail/Files. Documentation structural checks pass.

The reported P2P listener lifetime issue was not reproduced in this core run.
Current NetworkListener owns io_context before its server, so reverse destruction
keeps the context alive through server destruction. No speculative listener or
protocol change was introduced.

Remaining acceptance: actual process-kill/fault-injection boundaries, durable
exact-job recovery after deliberate network-index rebuild, explicit cross-device
Files conflict policy, cancellation/retry of local queued intents, live large-file and concurrent
rotation/staging acceptance, and live unlocked
mailbox/shutdown verification. Current app.db publication journals must be
preserved; it is not yet safe to delete/rebuild the whole database. Restart tests
alone do not prove every process-crash or disk-failure scenario. No peer deploy,
network reset, key replacement or second signer is part of this change.

## Network layout correction and shutdown investigation (2026-10-09)

Operator-confirmed base 5 op/min is a labelled GUI reference; current measured
rate and actual observed maximum stay separate. Numeric text is 18 px; fitted
map uses larger bounds. The summary remains visible with Advanced, whose drawer
starts below it. Wide/narrow/Advanced captures and Network regressions pass.
Desktop rebuilt; full native Windows Qt: 92 passed, zero failed/skipped.
Evidence: artifacts/network-overlay-20261009/qt-compact-full.txt (ignored).

The old process was force-ended only after explicit operator authorization.
Ordinary close was tested with the existing local state, first locked and then
operator-unlocked Identity; both completed within the 15-second check. The prior
hang was not reproduced and its root cause is not established. Keep-running-in-
background is a separate intended close behavior, not the demonstrated cause.
Static shutdown phase logs now distinguish operator/settlement/network/application
waits and controller completion; window-to-tray logs identify the actual setting.
No private keys, content or identifiers are added to those diagnostics.

## Network indicative totals and observed maxima (2026-10-09)

The primary Network page preserves the centered, full-size map. A translucent
horizontal header overlay presents exactly observed finalized op/min, approximate
network provider capacity and admitted encrypted bytes. Benchmark cards, compiled
reference resource and GUI parser are removed. Each figure has its greatest real
observation since runtime start; no generated load, benchmark result or historical
block import supplies an operation maximum. Maxima are volatile, not all-time
network records or theoretical ceilings.

DEC-290 adds only a small direct counter query over ordinary TLS: empty request
(27), exactly two little-endian u64 counters in response (28). Existing storage
identity proof deduplicates storage relationships; no new usage signature, audit
or accounting platform. Source is existing FinalizedChunkStore CapacityBytes /
UsedBytes. The independent storage probe samples known/configured endpoints at
most every 30 seconds per endpoint, with two-second reply deadline; samples older
than 90 seconds do not contribute. Sum direct samples once per StorageId, plus
this node once; display approximate values, response coverage and time. Copies
count, local reserve excluded; not a census, free disk or unique logical size.
GUI reads snapshots without network I/O. No new worker, DB, network role, signer
route, genesis/key/history reset or canonical operation.

Validation: desktop/core/Qt targets rebuilt; runtime, P2P and node-service
suites passed (74 cases), full native Windows Qt passed (92, zero failed/skipped).
After the responsive header adjustment, both Network regressions passed again;
1040x720 and 760x640 fixture captures preserve the map and show uncut labels.
These fixture values are layout evidence, not live network measurements.
Operator-authorized deployment followed: VPS checkout fast-forwarded from
7f043f95 to 9353c740, headless cybou rebuilt (GUI/tests OFF), and all three ordinary
services restarted with their unchanged state/configuration. All returned active,
height 11401, NRestarts=0. Direct TLS request/response checks on public ports
29461/29462/29463 returned respectively capacity/stored byte counters:
28633115306/287738688, 10737418240/255143424, 10737418240/336516352.
Sum: 50107951786 capacity bytes and 879398464 admitted physical bytes
(approximately 46.7 GiB / 839 MiB, copies included). This is the three VPS processes,
not a global census or independent failure-domain claim. Bootstrap SPKI was
checked against the compiled pin; the other ordinary endpoints have no compiled
pin. This one-shot read-only check validates live response shape/counters, not
storage-proof signatures or the desktop's displayed aggregate. The runtime's
identity-proof path has separate TLS regression coverage.
Windows desktop rebuilt and started; existing application/state/key material and
PoA signing history retained. No genesis replacement/reset, new signer or load
run. PoA requires unlocking the existing authorized Identity after restart.
Ignored build/test/fixture evidence: `artifacts/network-overlay-20261009/`.

## Beta monitoring simplification (2026-10-09)

Remote telemetry has been removed from production source and active documents:
message codes 27/28, codecs, cache/collector worker, exchange guards, address groups,
aggregation, immutable remote snapshots, cohort history, polling and remote UI.
The ordinary P2P baseline ends at 26 and rejects all unsupported message codes.
No experimental/disabled compatibility path remains. Superseded implementation
and design are retained in Git, not in the runtime.

Existing local metrics, transfer counters, finalization windows, process CPU/RAM,
storage gauges and chain/P2P/storage behavior remain. CPU/RAM/frame traffic moved
to Technical; two local charts show operations and completed PUT/GET payload.
The diagnostics sampler is still independent of Identity unlock/page visibility.
Uncommitted storage-job/queue instrumentation was withdrawn. Further metric
additions require the concrete-question/source/scope gate in the frozen plan.
No deployment, signer restart, genesis/key/history or storage-protocol change.

Validation: desktop, core-test and native Qt targets rebuilt. Serial targeted
core suites passed: runtime 23, P2P peer manager 37, node service 11 and storage
service 23 (94 cases). The unsupported-message regression fixes the current
maximum at 26 and checks rejection of every higher code. Completed-payload
history is checked separately from frame bytes. Full native Windows Qt: 92
passed, zero failed/skipped, including exactly two charts, absent remote UI,
local values/Unknown, EN/FR Console and keyboard workflows. Active-source and
document scans find no removed API or report-contract link; manifest/diff checks
pass. Ignored evidence: `artifacts/local-monitoring-20261009/`.

## Local completed storage payload counters (2026-10-09)

Every production TLS session shares its runtime traffic meter. Separate passive
PUT/GET meters now count completed encrypted chunk bytes, independently of frame
traffic. PUT receive counts successful provider admission (including duplicates),
even if its receipt response subsequently fails; PUT send counts only after the
matching proven provider's signed receipt verifies. GET send counts a completed
local payload write, without proof of remote receipt; GET receive counts only
after the whole chunk's ChunkID verifies. Locally partial/rejected PUTs, missing
GETs and invalid received chunks add no completed payload bytes on those paths.
Their frames still count normally; the other endpoint may have completed its
local write/admission and counted it. Bytes enter windows at local completion.
Recovery/repair/full-GET checks and repeated completed transfers count again.
Endpoint totals can differ; these are neither unique logical content nor network
throughput, storage durability, service settlement or canonical evidence.

Each direction has a volatile runtime-lifetime total and a rolling window of
60 completed seconds, excluding the current partial second. Before a complete
minute, rates are Unknown; a complete quiet window is zero. Bounded in-memory
meters retain no peer/content/user identifiers and restart empty. Snapshot reads
copy bounded completed-payload history and trigger no transfers, scans, probes or audits.
Network Advanced adds local PUT/GET cards; Console `metrics` uses the same EN/FR
formatter with byte totals, rates and completion semantics. Collection remains
independent of Identity unlock and page visibility. Remote reports are unchanged.

Validation: desktop, core-test and native Qt targets rebuilt. Serial core suites
passed: storage service 23, node runtime 27, peer manager 43 and observation
cache 6 (99 cases). Real TLS storage checks cover accepted/duplicate PUT,
capacity rejection, complete GET, missing GET and subsequent PING/block sync.
Deterministic windows cover startup Unknown, partial-second exclusion, quiet
expiry and restart. The full native Qt suite exited successfully; explicit Qt
file-logger checks for Network and Console also passed, including EN/FR and
Unknown rates. Ignored evidence: `artifacts/storage-payload-20261009/`.
No deployed network, signer or service was restarted.

## Local storage headroom and available disk (2026-10-08)

Runtime snapshots now include optional OS available bytes on the blob filesystem.
The query is read-only and does not enumerate content. Before the first blob,
the immediate parent is used; memory-only stores, missing parents or OS errors
have no disk reading. Sampling uses the existing desktop diagnostic refresh,
independent of Network visibility, and the same snapshot's UTC observation time.

Network Advanced Storage shows accounted encrypted lengths / V, percent used,
saturated policy headroom, obligations / provider budget and separate available
disk. Public read-only `capacity` shows exact bytes, provider budget headroom and
time in EN/FR without Identity unlock. Missing observations remain Unknown;
measured zero disk availability is retained. No foreign-content/provider DB
enumeration, directory creation, wire fields or admission policy changes.

Stored lengths exclude filesystem allocation/metadata overhead; disk availability
is shared and precedes provider admission reserve. Local policy headroom neither
reserves disk nor guarantees admission, replicas or network capacity. Admission
estimates and storage I/O/history are outside the frozen Beta metric set.

Ordinary MinGW desktop/core/Qt binaries rebuild successfully. Storage suite:
6 cases / 61 assertions passed; runtime suite: 22 cases / 316 assertions passed.
Disk tests cover memory-only absence, a parent before first blob, an existing
chunk directory and a removed parent, while preserving policy/usage accounting.
Windows filesystem-space queries can succeed for a missing directory; explicit
directory checks prevent that from becoming a valid storage observation.
Other core suites are filtered; no Linux execution or live network capacity
claim. Final core logs: `artifacts/network-capacity-20261008/core-storage-final.txt`
and `core-runtime.txt` in the same directory.
Desktop SHA-256: `8b46e9292a8050dc75aa21c214d445d7c45b0ccae7b594cdc07b50805475d075`.
No desktop/signer or VPS service is restarted.

Full offscreen Qt with system fonts: 92 passed, 0 failed/skipped. Coverage
includes available/missing disk in Network, read-only `capacity` in EN/FR,
argument validation, locked access, measured zero available disk and saturated
headroom when use exceeds policy. Manifest/diff checks pass. This is component
evidence, not a live service/durability or network-capacity measurement.

## Local process CPU observation (2026-10-08)

Runtime diagnostics now sample cumulative whole-process CPU time under a local
sampler mutex: Windows `GetProcessTimes` kernel+user time and Linux
`CLOCK_PROCESS_CPUTIME_ID`. Interval percent uses monotonic elapsed time and
OS online logical processor count. First/unavailable readings have no interval;
failures, counter/time regression and processor-count changes reset the baseline
and accumulated mean. Sub-millisecond reads retain the baseline without adding
an interval. Idle intervals
with unchanged cumulative CPU are measured zero. GUI work is included.

Completed means use the CPU delta over actual elapsed windows of at least
60 seconds, weighted by time, with duration, interval count and age. Sparse
reads produce longer windows; no gap interpolation or rolling-minute guarantee.
Fixed memory, no thread/process enumeration or background worker is added.
The desktop's existing diagnostic sampling works independently of Network
visibility. Runtime restart resets observation.

Network Technical and read-only Console `metrics` expose interval CPU and the
completed mean's scope in EN/FR. OS online processors are not affinity or
container quotas. Host load, resource charts, storage I/O, remote reports and
network capacity remain outside this package.

Full offscreen Qt with system fonts: 92 passed, 0 failed/skipped. Coverage
includes interval display, clearing missing samples and exact duration/count/
age in EN/FR `metrics`. Manifest/diff checks pass. No live workload ceiling,
Linux acceptance or physical assistive-technology acceptance is claimed.

Ordinary MinGW desktop/core/Qt binaries rebuild successfully. Runtime suite:
22 cases / 316 assertions passed, including elapsed-time weighting, idle zero,
short reads, completed-window duration/count/age, reset on missing readings,
counter/time regression and changed processor count, plus the Windows OS
counter. Other core suites are filtered. Linux code is not compiled/executed
by these Windows checks. Logs: `artifacts/network-cpu-20261008/`.
Desktop SHA-256: `5dfafa78ffd009ac45c21f2f39a0261c61c8420c4e822fe25a5d2af3826950c3`.
No desktop/signer or VPS service is restarted.

## Local instantaneous process memory (2026-10-08)

Shared runtime diagnostics expose an optional resident-byte gauge for the entire
CYBOU executable. Windows uses `GetProcessMemoryInfo` working set; Linux reads
the fixed `/proc/self/statm` RSS record and OS page size with checked arithmetic.
Unsupported platforms and read failures return absent, never a fabricated zero.
The snapshot's UTC time applies to this instantaneous reading; the existing
background collector reads it while Network is hidden. No history, process
enumeration, external collector or new wire message is added.

Network Advanced Overview displays process memory in human-readable units;
Console `metrics` exposes exact bytes in EN/FR. Shared pages and the GUI are
included. This is neither host RAM nor unique physical RAM, mean load, peak
memory or a network total. Resource history,
storage I/O and remote resource reports remain open.

Ordinary MinGW desktop/core/Qt test binaries rebuild. Runtime suite: 21 cases /
296 assertions passed, including a positive live OS resident-memory reading
in the ordinary runtime diagnostics. Other core suites are filtered. This
package verifies the Windows implementation; the Linux branch is not compiled
or executed by this Windows check. Logs: `artifacts/network-memory-20261008/`.
Desktop SHA-256: `38985b4a2769512742f7b0030aaae5ede12bc8dab46f71b6d19717d973b9a5e7`.
No desktop/signer or VPS service is restarted.

Full offscreen Qt with system fonts: 92 passed, 0 failed/skipped. The memory
tile handles a value and an absent measurement; read-only `metrics` covers
missing samples and exact-byte EN/FR output. Manifest/diff checks pass. These
are component tests, not a live network workload or physical UI acceptance.

## Bounded local charts (current Beta)

Network retains two volatile local charts: finalized operations and completed
PUT/GET encrypted payload receive/send. Five-second completed intervals are
bounded to 180 points (15 minutes), with keyboard/mouse selection and accessible
text. Frame traffic remains Technical/Console detail. No remote/cohort chart,
reporting-group coverage model, resource history or capacity ceiling.

## Verified operation observation windows (2026-10-08)

Successful runtime block commits record operation counts in a fixed one-second
ring supporting 1/5/15-minute windows. Local production and directly accepted
announcements count in the observed series; batch/default history imports stay
separate. Duplicate/rejected commits never increment; replacing a canonical
height resets windows/totals. Zero-operation blocks are not transaction activity.
Collection uses the existing chain mutex with no history scan or GUI dependency.

Network Advanced shows observed finalized op/min over a complete minute.
Console `metrics` shows all windows, local-produced contribution, history counts
and totals since observation reset. Startup/reset windows are Unknown; complete
local idle windows may be zero. No creation timestamp exists in blocks, so old
announcements can arrive later. Observation speed is not a measurement of current
global production, global freshness or a throughput ceiling. The stale timestamp
check claim in `POA_FINALITY.md` is corrected to the actual block layout.
The charts added above cover local history; resource metrics and remote
aggregation remain open.

Ordinary MinGW desktop, core and Qt test binaries rebuild successfully.
Runtime suite: 20 cases / 280 assertions passed, including window boundaries,
source separation, duplicate commit, actual local production and canonical
replacement behavior. Full P2P suite: 37 cases / 1,178 assertions passed.
Remaining core cases are filtered; no full-core-suite or live throughput claim.
Logs: `artifacts/network-finalization-20261008/`.

Full offscreen Qt: 92 passed, 0 failed/skipped. Coverage includes the observed
op/min tile, Unknown on missing/uninitialized samples, incomplete longer windows,
separate history counts and EN/FR `metrics` output. Manifest/diff checks pass.
Desktop SHA-256: `9e7529e864f849f48bdbe31e3418b268ff7b9e6ab783110574ab392c2b2e455f`.
No desktop/signer or VPS service is restarted; these are component observations.

## Passive local traffic observation (2026-10-08)

The shared runtime meter counts actual CYBOU frame-stream bytes at successful
TLS application read/write progress. Inbound sessions, mesh outbound and pooled
storage connections use the same counter. Partial/retried/service/invalid-input
bytes count as traffic; TLS record/handshake and TCP overhead do not. It does not
measure unique delivered content, finalized operations or network-wide traffic.

One fixed 61-slot ring and a short mutex provide the preceding 60 complete
seconds, excluding the current partial second. Startup rates remain Unknown;
complete idle intervals yield measured zero. Lifetime totals survive connection
churn, reset on runtime restart, and are not reset by diagnostic reads. A delayed
writer cannot overwrite a newer ring interval. Collection has no GUI dependency,
per-peer labels, chain scan, new wire message or storage proof request.

Network Advanced Overview displays the shared received/sent B/s observation;
read-only `metrics` also exposes exact lifetime totals and UTC sample time.
FR/EN translations are included. Resource measurements, operation rates,
PUT/GET payload and local charts were delivered separately; remote consolidation is outside Beta.

Ordinary MinGW desktop/core/Qt binaries rebuild. The passive window case passes
14 assertions (startup, complete-window boundaries, partial-second exclusion,
expiry, idle, restart, concurrent writers and delayed-writer ring collision).
The full P2P peer-manager suite passes 37 cases / 1,178 assertions, including
actual TLS handshake/ping traffic observed through the runtime meter. Other
core cases are filtered; this is component transport evidence, not DEVNET load.
Logs: `artifacts/network-traffic-20261008/`. Desktop SHA-256:
`a06e7d50bdd8a4c95080dd803d363b2d144465a3c5ecb10aabf582f941648277`.

Full offscreen Qt: 92 passed, 0 failed/skipped; shared Network rates, missing
observation reset and EN/FR `metrics` totals/rates are covered. Manifest/diff
checks pass. No running desktop/signer or VPS service is restarted or deployed.

## Network naming and monitoring scope

Network / Réseau is the sole network observation page. DEVNET remains the compiled
active profile. Local diagnostics and service-owned content protection use the
existing sampler and product model. DEC-289 and NETWORK_OBSERVABILITY_PLAN.md now
freeze the Beta metric set; wider telemetry is absent from the active architecture.

## Core CI signed-size build repair (2026-10-08)

Authenticated log for GitHub core job `113302547455` (run `37774708866`,
source `1f5417fb`) identifies `-Werror=sign-compare` in POSIX
`ReadSecretFile`: signed `stat::st_size` was compared with unsigned `size_t`.
Linux `-Wall -Wextra -Werror` syntax checking reproduces the failure. C++20
`std::cmp_greater` now compares the integers without narrowing or suppressing
warnings. Positive-size, one-MiB policy, private ownership/permissions, regular
file and link rejection remain in force. The existing regression adds too-small,
zero and over-policy size limits while retaining the exact-limit successful read.

A fresh isolated WSL Ubuntu 26.04/GCC 15.2 headless build uses CI's
RelWithDebInfo `-O2 -g0`, BUILD_TESTS, test hooks and WARNINGS_AS_ERRORS settings;
all five CI targets build. It uses local OpenSSL/BLAKE3 dependencies, not the
Ubuntu 24.04/GCC 13.3 CI image: this is local strict-build evidence. The full
core suite passes 266 cases / 98,757 assertions. Official DEVNET help/info/doctor
checks pass; doctor leaves the supplied absent data directory absent.

Isolated MinGW desktop/core/Qt builds pass. Full offscreen Qt: 92 passed,
0 failed/skipped. Three native Windows Qt scenarios: 5 results including
setup/cleanup, all passed. The Windows secret-file case passes 7 assertions
(the remaining 258 cases are intentionally filtered, not a Windows full-suite
claim). Python battle/benchmark/report discovery passes 6 tests. Build cache
output is restored. Logs, original failing CI log and binary/source provenance
are under `artifacts/ci-integrity-20261008/`.

This repairs the diagnosed source failure; green remote core/desktop CI for the
published corrected revision is still required before closing P0. The earlier
desktop jobs were in vcpkg setup at the status check. No running desktop,
signer, VPS, accepted network material or signing history was changed.

## Acceptance planning review (2026-10-08)

Active remaining work is [DESKTOP_BETA_ACCEPTANCE_PLAN.md](DESKTOP_BETA_ACCEPTANCE_PLAN.md),
reviewed against `1f5417fb6d6c4d418b474b856a23bdb21486c2d6`. Public GitHub Actions
API confirms [core run 37774708866](https://github.com/cybou-fr/cybou/actions/runs/37774708866)
failed at binary build; protocol/CLI steps were skipped. Compiler cause remains
unverified. [Desktop run 37774708869](https://github.com/cybou-fr/cybou/actions/runs/37774708869)
was in progress at review. Earlier local core passes and the current 92 Qt/6
native Windows results retain their exact package scope; they do not establish
green remote core CI. No CI fix or new live acceptance is claimed here.

The implementation-first backlog is superseded by revision integrity, prepared
independent topology, live recovery/outage/operator acceptance and physical/
assistive-technology checks. Sustained capacity is P1. Files profiling does not
close completed-frame/scroll/live acceptance; Mail's component optimization
avoids an unsupported further architecture rewrite. Historical same-host
benchmark remains a scoped op/min reference, never capacity. No network,
provisioning, signer or deployment change is authorized by this planning update.

## Recovery/Console keyboard package (2026-10-08)

Recovery phrase replacement explicitly defaults to No at its first confirmation.
The recovery word display and two distinct numbered confirmation fields expose
translated accessible names; confirmation labels are associated with their
fields. The existing phrase/password gate and confirmation checks remain in use.

Console allows ordinary Tab traversal from an empty command input. Tab still
opens/accepts suggestions for nonempty commands, and Ctrl+Space remains available
on an empty input. Command and output fields expose translated names; output
reserves a contrasting focus border.

FR/EN and light/dark component coverage uses synthetic words only. It exercises
password/acknowledgment gating and cancellation without revealing a phrase,
Return on the default No before rotation proceeds, wrong/correct confirmation
words, Escape and text cleanup in the rotation phrase dialog. Console coverage
checks empty-input Tab/Shift+Tab, command execution and Up/Down history, Ctrl+F,
search Escape without closing, Ctrl+L and final Escape. This does not establish
clean-machine restore, real key rotation/finality, physical keyboard,
screen-reader or mixed-monitor acceptance.

Validation: isolated GUI/test builds succeeded; full offscreen Qt passed
92 results and four focused native Windows scenarios passed 6 results including
setup/cleanup, with no failures/skips. The initial native test sent focus before
page event processing; settling the page before activation fixed the test.
Evidence and source/binary provenance: `artifacts/ux-recovery-console-keyboard-20261008/`.
The build cache is restored; no live vault, signer or VPS was changed/restarted.

## Identity/Wallet/Network keyboard package (2026-10-08)

Wallet activity rows and folded fee groups now participate in Tab navigation,
expose translated semantic names/descriptions and activate on Enter or Space.
Keyboard expansion focuses the first individual fee; existing row retention and
command paths remain in use. Rows reserve a two-pixel focus border. The Network
map now repaints an explicit contrasting border on focus changes. Identity's
recovery password and Wallet's Network balance amount expose explicit names.

Component coverage walks complete forward/reverse Tab cycles through enabled
Identity, Wallet and Network actions in FR/EN and light/dark themes. It checks
map arrow selection, activity details cancellation and focus return, fee-group
expansion, and Escape after entering a valid Network balance amount without
submitting or changing balances. Synthetic dialog input settles QWidget
activation. Physical keyboard, screen readers, native OS file dialogs,
mixed-monitor DPI and live DEVNET acceptance remain separate gates.

Validation: isolated GUI/test builds succeeded. Full offscreen Qt: 91 passed,
0 failed, 0 skipped. Seven focused native Windows scenarios: 9 passed including
setup/cleanup, 0 failed, 0 skipped. Native focused-row captures in EN/light and
FR/dark and the FR/dark map were visually inspected; borders are visible.
Evidence and source/binary provenance: `artifacts/ux-account-network-keyboard-20261008/`.
The build cache is restored; no live desktop/signer or VPS was restarted.

## Mail/Files keyboard menus and dialogs package (2026-10-08)

Mail previously selected a context-menu row from pointer coordinates even for
a keyboard request. Mail and Files list/grid now handle keyboard context events
at the current item, scroll it into view and anchor the existing menu there.
Mail retains a multiple selection containing that row; Files replaces retained
selection when its current item is outside it. Mouse requests and the existing
command/acknowledgment paths remain unchanged. Move's folder selector and the
CYBOU Files attachment picker now expose explicit accessible names using
existing translated strings.

FR/EN regression delivers keyboard context events with coordinates away from
the current item, navigates menus with Home/Down/Return, cancels Mail's menu and
Rename with Escape, and opens Compose attachments with Space before cancelling
the CYBOU picker. Cancellation retains draft text and issues no Rename. Files
Move uses keyboard folder selection and acceptance; injected save failure
retains the original parent. Focus ownership after closure is checked.

The test uses separate menu/dialog drivers and explicitly settles QWidget
activation before nested synthetic input. It starts each language in Inbox,
because language rebuilding correctly preserves an open Compose and can hide
the list in two-pane mode. Investigation of a cancellation timeout found a
synthetic activation problem; ordinary application Escape behavior is unchanged.
These are Qt event-route checks, not physical OS Menu/Shift+F10 acceptance.

Validation: isolated GUI/test builds succeeded; the final full offscreen Qt
suite passed 90 results and seven focused native Windows scenarios passed nine
results including setup/cleanup, with no failures. Existing protected-file picker,
Mail menu/Undo, shortcut scopes, Tab/focus and compose rebuild regressions passed.
Evidence and source/binary provenance are under `artifacts/ux-menus-20261008/`.
The build cache is restored to its usual output; no live desktop/signer or VPS
was restarted/deployed. Physical keys, native OS file dialogs, screen-reader,
mixed-monitor DPI and wider dialog/live acceptance remain open.

## Mail/Files Tab and visible-focus package (2026-10-08)

Compose Send and Reader Security details used local borderless styles, suppressing
visible border treatment when focused. Both now reserve a transparent two-pixel
border and use the theme's primary text color for an explicit focused border.
The reserved space keeps focus transitions from changing geometry. Command,
publication, storage and Identity behavior are unchanged.

A new component regression walks complete Tab and Shift+Tab cycles through
Compose, Reader and Files list/grid in FR/EN and light/dark themes. It verifies
all currently visible enabled buttons are reachable both ways, icon-only
controls have accessible names, and Subject/Body are in the Compose cycle.
Compose includes a dynamic attachment-removal button; Reader includes received
attachment controls; Files includes the selection toolbar. Native page layout
and activity are settled after page replacement before walking focus. This is
reachability evidence, not a claim that every menu/dialog action was activated.

Validation: isolated GUI/test builds succeeded; the full offscreen Qt suite
passed 89 results and seven focused native Windows scenarios passed nine results
including setup/cleanup. Native control/panel captures of Send, Security details
and selection Trash were saved in all four language/theme combinations. Light
English and dark French Send/Reader panel captures were visually reviewed, with
clear focused borders; the dark Files Trash control was also inspected.
Native layout testing reported a Windows maximum-size clamp for the largest
requested geometry, so that run does not establish physical 1920x1080 acceptance.
Evidence and final source/binary provenance are under
`artifacts/ux-focus-20261008/`. The build cache is restored to its usual output;
no live desktop/signer or VPS was restarted/deployed. Other pages/dialogs,
physical keyboard, screen-reader and mixed-monitor DPI acceptance remain open.

## Mail/Files keyboard scope package (2026-10-08)

Files selection shortcuts were attached to the whole page: retained selection
could let F2/Delete act on files while navigation held keyboard focus. They now
belong to the list/grid with WidgetWithChildrenShortcut scope. Trash rejects
both shortcuts, consistent with its visible Restore/permanent-delete actions.
The acknowledged command and review paths are unchanged.

A new FR/EN regression covers retained selection with navigation focus, header
text deletion, actual list/grid rename and Trash commands, failed-save catalog
retention and the Trash guard. It also covers Mail search/compose shortcuts and
literal compose text containing shortcut letters and Delete. The test dismisses
search completion with Escape, explicitly reactivates the parent after a modal
dialog (required by offscreen Qt), and distinguishes projection/session traffic
from file mutations. Existing async unlock and language/theme compose-retention
scenarios remain covered.

Validation: isolated GUI/test builds succeeded; the full offscreen Qt suite
passed 88 results and five focused native Windows scenarios passed seven
results including setup/cleanup. Both final runs passed without failures.
Evidence and final source/binary provenance are under
`artifacts/ux-keyboard-20261008/`. The build cache is restored to its normal
output directory. No live desktop/signer or VPS was restarted/deployed.
These programmatic events do not close full tab-order/visible-focus, physical
keyboard, screen-reader, mixed-monitor DPI or wider FR/EN/live acceptance gates.

## Mail viewport performance package (2026-10-08)

Profiling confirmed that per-message widgets dominated initial presentation.
Mail now paints visible semantic rows through one QStyledItemDelegate; stable
QListWidgetItem IDs, rank, selection/current/anchor and replacement-ID handoff
remain. The delegate preserves avatar, direction, unread/star/attachment markers,
plain subject/preview, status/date, protection and support-rate explanations.
Accessible item text/description and escaped tooltips replace child-widget-only
labels. A visual review caught short metadata clipping; font source and text
padding were corrected, with light/dark status captures retained. Reader,
compose, encrypted index ownership, delivery and publication semantics are unchanged.

Native Windows component runs with the actual desktop stylesheet initialized,
1040x720 geometry and DPR 1.75 gave these observations (milliseconds):

| Synthetic mailbox | Construction before / after | First completed Qt render before / after | Scroll render median before / after | One-message update + render median before / after |
| --- | ---: | ---: | ---: | ---: |
| 2,000 messages | 972 / 27 | 1,252 / 112 | 9 / 5 | 26 / 13 |
| 10,000 messages | 12,171 / 144 | 17,304 / 219 | 32 / 5 | 117 / 44 |

Scroll/update columns each use ten samples, not long-run percentile estimates.
Separate unstyled native baseline logs are retained, including the earlier
7,440 ms first render at 10,000 messages; they are not mixed with the styled
comparison. The baseline row-widget counts after ten updates include ten
widgets awaiting Qt deferred deletion; final rows create zero mailRow widgets.
A synchronous viewport grab establishes completed Qt rendering, not physical
monitor presentation. These are synthetic local UI observations, excluding
network/storage/index discovery I/O and clean-machine/live acceptance.

Validation: isolated GUI/test builds succeeded; the final full Qt suite passed
87 results, and seven targeted native Windows scenarios passed (nine results
including setup/cleanup). Profiling runs each passed at 2,000 and 10,000 messages.
Regression covers semantic status/date/unread/star/attachment metadata, escaped
tooltips, zero message-row widgets, retained current/selection/anchor/search,
replacement IDs, lock clearing and existing acknowledged Mail/Files core-adapter
flows. Light/dark status captures and a French dark desktop Inbox fixture were
visually inspected. Evidence, baseline source patch/binary provenance, structured
measurements and final source/binary provenance are under
`artifacts/ux-mail-performance-20261008/`. The build cache is restored to its
normal output directory. No live desktop/signer or VPS was restarted/deployed.
Physical drag/mixed-monitor DPI and real screen-reader acceptance remain open.

## Shared Mail/Files task panel package (2026-10-08)

The existing header Activity panel now receives correlated local Files command
progress alongside Mail. Folder creation, rename, move, copy, Trash and Restore
report actual Queued/Running/Committed/Failed worker states; local acknowledgment
is distinct from finalized catalog/publication state and remote protection.
The single application task journal replaces the Mail-only API throughout the
model, compose/reader, Activity and Console jobs. Scope guards keep file commands
from changing Mail handoff state. Existing publication retry remains available;
failed mutations offer their item workflow without automatic intent replay.

The popup scrolls instead of silently stopping at 12 entries. Up to 100 stable
command/item rows are retained across refresh with button focus and scroll;
excess tasks receive an explicit notice. Status and tooltip output are bounded
and use plain text. Completed local saves leave the in-flight list. Terminal
history retains 32 entries without evicting active work, and pruning the final
old failure refreshes the indicator. Lock clears tasks and cached private popup
labels even when hidden; old-session callbacks cannot restore them. File events
no longer force unrelated Mail Reader refresh. FR copy covers new task actions,
shared-panel scope and Console task domains.

Validation: isolated GUI and Qt test builds passed. The final full Qt suite
passed 85 results; six targeted native Windows scenarios passed (eight results
including setup/cleanup), covering the common panel, local Mail/Files commit
and Undo, actual core-adapter Mail/Files flows and layout bounds. The shared-task
regression checks 105 queued tasks, explicit display limits, stable focus/scroll,
plain-text failure labels and item opening, stale/duplicate replies, hidden popup
lock cleanup and removal of the last retained failure. Evidence and binary/source
provenance are in `artifacts/ux-common-tasks-20261008/`. These are local component
and delivered Qt event checks, not physical input, live-network outage or
clean-machine acceptance. No live desktop/signer or VPS was restarted/deployed.
The build cache is restored to its normal output directory.

## Files local protection observations package (2026-10-08)

Files details distinguish saved remote replica counts from the last local
placement/audit attempt. Attempt time, partial/full audit scope and typed reasons
come from StorageService; raw endpoint-bearing errors never enter the semantic
projection. A bounded volatile cache retains up to 1,024 publications and drops
observation time on reopen/eviction without discarding persisted placement counts.
Reading those records does not refresh time or perform a network check.

Old observations receive a descriptive 24-hour age label, not fabricated replica
loss or a protection-state transition. Unknown counts remain unknown in Advanced.
An unmet target no longer advertises active replication. Disabled downloads
explain retrieval in progress or missing local/remote availability; verified local
copies remain downloadable regardless of remote observation age. FR translations
cover scope, reasons and the new details fields. These are local diagnostics, not
canonical reliability, continuous uptime or independent failure-domain evidence.

Validation: isolated GUI/core/Qt builds passed; all 259 core cases passed with
98,013 assertions, all 84 Qt results passed, and four targeted native Windows
scenarios passed (six results including setup/cleanup). Coverage checks read-only
freshness, reopen with unknown time, placement/full/partial scopes, safe reasons,
unknown/zero/one/two copies, age labels and local download eligibility. The first
full Qt run retained an obsolete replication-label expectation; its failing log
and the corrected full run are retained in
`artifacts/ux-files-observations-20261008/`. The build cache is restored to its
normal output directory. No live desktop/signer or VPS was restarted or deployed.

## Protected Files into Compose package (2026-10-08)

Compose now accepts internal Files drag IDs over its surface and text editors,
adding reusable references without a local source path. Duplicate sources are
deduplicated; all dropped references must currently be eligible. Pending,
missing, folders, content inside Trash and closed sessions are refused. Source
eligibility is rechecked on drop; send/close handoff blocks attachment changes.
Internal MIME takes precedence over downloaded URLs, so refusal never becomes
a local re-upload. Local file drops remain supported and dropped batches respect
the current 32-attachment schema bound. FR copy explains reuse and refusal.

Validation: isolated application/test build and the full Qt suite passed 83
tests. Native Windows targeted scenarios passed at Qt scale factors 1.00, 1.25,
1.50 and 2.00. UI regressions cover editor routing, duplicate/mixed payloads,
source changes, pending/Trash/missing items, busy/lock and draft acknowledgment.
The core adapter test persists a reference draft through session closure before
successful delivery and exact-byte download. Source removal before preparation
refuses sending while retaining the acknowledged draft. This is not a new draft
retention guarantee or physical OS/mixed-monitor drag acceptance. The first UI
test run exposed an incorrect test-double projection expectation; its original
log is retained alongside the corrected runs in
`artifacts/ux-compose-drops-20261008/`. No live desktop or signer was restarted.

## Files drop routing and Qt scale validation package (2026-10-08)

Files list/grid, breadcrumbs and navigation now share target validation with
the move command. They refuse missing items, unusable destinations, folder cycles
and closed Identity sessions, and revalidate at drop. Duplicate IDs issue one
command per item. Internal Files MIME takes precedence over downloaded-copy URLs,
so a refused move cannot become a duplicate import. Blank My files drops move
into the current folder; ordinary external local-file import remains available.
Non-local URLs alone do not count as an import.

Validation: isolated app/test build and the full Qt suite passed 82 tests.
Five targeted scenarios ran with the Windows Qt platform at measured device
pixel ratios 1.00, 1.25, 1.50 and 2.00, seven results including setup/cleanup per
profile. They cover Files list/grid targets, local import, mixed MIME refusal,
duplicate/stale IDs, lock between enter/drop, acknowledged Undo, Mail folder
event routing and layout bounds. These are delivered Qt events and configured
Qt scale factors, not physical OS drag-and-drop or mixed-monitor DPI acceptance.
Evidence and binary provenance are in `artifacts/ux-files-drops-20261008/`.
No running desktop/signer, network constants or deployment was changed.

## Acknowledged Files changes and Undo package (2026-10-08)

Folder creation, rename, move, copy, Trash and Restore report whether the
publication intent was saved locally. Failed publication staging retains the
existing item and reports failure; a failed new folder shows Needs attention.
Move/Trash batches give pending feedback, count partial results and offer Undo
only for saved changes. Undo queues an inverse move to the original folder,
including from Trash, before or after the forward projection arrives. Apparent
no-ops are checked against worker state, not a potentially older GUI snapshot.
Session replacement/lock invalidates delayed replies and retained Undo actions.
Acknowledgment does not imply network finalization or confirmed remote replicas.

Validation: isolated MinGW application/test builds and the complete Qt suite
passed 81 tests. Regression coverage exercises delayed/failed saves, partial
results, early Undo, unavailable Files and stale session callbacks. The actual
core adapter test queues a move and its inverse, and Trash and return to the
original folder, before finalization; it verifies both final catalog outcomes.
Native Windows focused tests also cover these two scenarios. FR strings cover
pending/local acknowledgment and new failure states. No live DEVNET signer or
node was restarted, and no network, genesis or deployment configuration changed.

## Acknowledged Mail deletion package (2026-10-08)

Draft discard and permanent mailbox deletion now reuse correlated Mail command
progress and session generation. The adapter removes private projected rows only
after the encrypted Application DB commits deletion. Exceptions or failed writes
retain the failed item and report failure. Permanent batch deletion reports each
committed/failed item; the reader stays open while deletion is pending. A durable
pending send cannot be cancelled by deleting its projected row or backing draft.
This concerns local mailbox state, not deletion of historical/recipient copies.

Compose keeps text while discard is queued/running and re-enables it on failure.
Discard is ordered after any queued autosave; its obsolete editor reply is ignored.
Theme/language rebuild follows an in-flight discard by draft/task identity. Lock,
session replacement and reentrant lock from a row-removal listener invalidate late
completion callbacks. Context-menu draft discard no longer announces success
before the local commit. FR strings cover the new progress/failure states.

Validation: isolated MinGW app/test builds passed and the complete Qt suite passed
80 tests. The added regression injects delayed/failed commits, partial deletion,
autosave/discard ordering, rebuilt composer and stale callbacks. The existing core
adapter integration waits for actual draft and permanent mailbox deletion and
checks absence after lock/unlock. Final native Windows checks passed both the
new regression and core adapter integration (4 results including setup/cleanup),
including the additional reentrant-lock guard. Logs and isolated binaries are under ignored
`artifacts/ux-mail-actions-20261008/`. Clean-machine Beta and physical accessibility
acceptance remain open; Files mutation acknowledgement/Undo is separate remaining
work. The running desktop binary and live PoA state are not replaced.

## Authority and desktop presentation QA package (2026-10-08)

Authority now occupies a separate Administration navigation section visible only
with local genesis-key proof. Operator controls also require an unlocked Identity
and active signer. Pause has a Cancel-default consequences review; resume is
direct. The page and unlock screen explain that locking the user Vault does not
stop an already active background PoA finalizer. That local runtime observation
does not grant permissions or announce a network role.

Storage settlement prepares actual payout entries on the application worker,
then reviews period/UTC interval, unique payout accounts, entry count, amount,
escrow and the first 100 exact entries. Missing evidence is not reported as zero
service. Lock, account/proof loss, halt, changed period or changed escrow closes
the review without submission. Confirmed entries are submitted on a worker after
rechecking unlocked key and runtime cursor. Submission is explicitly pending
PoA finality. Journal status is the available local observation, not an asserted
integrity audit. Explorer separates finalized diagnostics from volatile candidates
and keeps selection by identifier; peer deltas remain unverified announcements.

Validation: isolated MinGW GUI/test builds passed; complete Qt suite 79 passed,
0 failed. The final dialog sizing/keyboard adjustment also passed its focused
regression (3 including setup/cleanup). Native Windows synthetic fixtures were
captured in FR/EN, light/dark, at 1040×720, 1280×860 and 1920×1080, plus dark
1040×720 with Qt scale 1.25: 14 profiles. Contact sheets cover Home, Mail, Files,
Wallet, Identity/recovery, Network/Advanced, Monitor, Console and Authority;
full-size operator reviews and locked-Vault views were inspected. The pass fixed
translated details-button clipping. Updated reviews include expanded payout
entries and pause consequences. Language fixture selection does not persist real
user preferences.

Evidence and isolated binaries: ignored `artifacts/ux-authority-20261008/`.
Fixtures and component checks are presentation evidence, not live settlement,
independent remote durability, clean-machine Beta acceptance or screen-reader
certification. Physical mixed-monitor DPI and assistive-technology acceptance
remain open. This package does not replace the running PoA desktop binary.

## Monitor and Console package (2026-10-08)

Network Monitor now opens one reusable nonmodal window from Advanced, with
Peers, Operations and Own content tabs capped at 256 rows each. Refreshes
coalesce over 150 ms, retain selected object and scroll, and distinguish
unverified peer-ahead announcements. Pause freezes the view only. Own-content
rows clear synchronously on lock/account change even while paused.

Console now has a compact live scope header, icon toolbar and run control,
registry-derived grouped help and bounded command/argument completion. Own file
arguments require an unlocked Identity; Authority suggestions require current
key proof. Completion never executes. Ctrl+F searches output with wrapped
previous/next and match counts; Ctrl+L clears output/history. Private session
cleanup also clears search and completion. Both windows save geometry only;
screenshot fixtures do not read/write those preferences. Console stays read-only:
Authority signing controls and settlement submission remain on the Authority page.

The concurrent refactor at `0dd16099` required build repairs: the publication job
cache uses its actual nested `Job` type; the node builds its own PCH because core
has different offline test definitions; extracted Activity declarations forward
declare semantic types; the screenshot harness includes the actual network and
backend headers. Activity retains its original translation context.

Validation: 259 core cases / 97,796 assertions and 78 Qt tests passed. Native
Windows captures at 1040×720 were inspected in light and dark, including ordinary
and Authority console scopes. Evidence and isolated binaries are under ignored
`artifacts/ux-r6-20261008/`; no live signer or network deployment is required.

## Network reference and Advanced sections package (2026-10-07)

The main Network map now carries a compact accepted-reference card: finalized
op/min, UTC run date, workload/count and historical same-host simulation scope.
Missing/rejected/wrong-network evidence renders Unknown. Details opens the full
existing evidence in Overview, retaining its exact counts/window and provenance.
Opening Network or its reference does not run a benchmark. Spare horizontal map
space keeps Corsica clear of the lower-right card without shrinking/distorting
the silhouette or changing its geometry when Advanced opens.

Advanced now separates Overview, Peers, Storage and Technical into bounded
scrollable tabs. Overview holds local finality/mesh observations and reference;
Peers holds session observations and the one selected-peer card; Storage holds
local capacity and own-content protection, with an explicit mesh/storage evidence
boundary; Technical hosts the existing diagnostics and monitor/console launchers.
The selected peer survives drawer close and reference/technical navigation.
Selecting a peer while Advanced is open chooses Peers. The Identity/desktop
diagnostics entry point opens Technical directly. Unavailable capacity shows
Unknown instead of an apparent zero measurement.

Isolated MinGW GUI/test builds passed. Focused suite: 5/0 including setup/cleanup;
final complete Qt suite: 77 passed/0 failed. Native Windows light 1040×720 and
dark 1280×860 Network, selected-peer, reference and Storage captures were visually
inspected under `artifacts/ux-r5-20261007/`. The captures combine synthetic peer
observations with the genuine compiled historical reference; they do not form a
new live network measurement. No network reset, deployment or signer change was
performed. Professional Monitor/Console/Authority work and complete physical
DPI/accessibility/FR-EN acceptance remain subsequent packages.

## Recovery and Mail/Files interaction package (2026-10-07)

Identity preparation labels local encrypted opening and discovery/decryption as
two stages. Continue in background keeps the worker running and shows a persistent
strip with Recovery details. Progress remains a verified-history scan, capped at
99 until projection readiness, not a claim of downloaded/protected content.
Failures reopen the scoped dialog after background continuation; Ready/lock hide
the strip. Appearance rebuild preserves background mode and its details action.

The shell now exposes one search field on Mail/Files. Typing and clearing feed
their existing current-view filters; switching products restores the respective
query and folder navigation clears the Files query. Ctrl+K and Mail `/` reach the
visible control. Lock clears search and private suggestions. Standalone page
fixtures retain their local control. Appearance rebuild retains the shared query.

Files selection actions now support one/multiple items, list/grid and Trash. Star
toggles the cohort, ordinary views offer Move to Trash, and Trash offers Restore.
Single selection offers Details; Clear selection is always available. Mutation
controls follow active Identity/backend availability. Permanent deletion keeps
its existing explicit review. Shared icon styling follows a dedicated property,
so named controls retain their size, hover and keyboard focus presentation.

Isolated MinGW GUI/test builds passed. Complete Qt suite: 76 passed/0 failed after
the icon style fix; the subsequent background-strip appearance retention change
passed its focused regression (3/0 including setup/cleanup). Native light 1040×720 and dark 1280×860
fixtures are under `artifacts/ux-r4-20261007/`. These are presentation evidence;
no additional signer, deployment or network reset is involved. Broader quiet
success-state review, physical DPI/accessibility and complete FR/EN acceptance
remain. This package did not restart or replace the user's PoA desktop binary.
Final native captures of the two-stage dialog, background recovery strip, single
header Mail search and Files selection toolbar were visually inspected. An early
capture exposed named icon buttons losing the common padding style; the dedicated
icon property correction is included in the final captures and build.

## Desktop benchmark accounting package (2026-10-07)

Loadgen now reports attempted/submitted/finalized operations separately, tracks
unique measured OperationIDs, excludes historical jobs and observes wallet
finalization during drain. The battle report uses one controller measurement
window, validates per-client counters/rates and shows missing latency samples as
Unknown. Network Advanced consumes a bounded compiled reference only after PASS,
successful acceptance checks and matching NetworkBinding; it labels provenance,
workload, measurement window and Windows/WSL simulation scope. Opening the page
does not generate load. French translations accompany the reference display.

The first live run exposed a database missing CURRENT and a missing client vault;
its files and failed verdict are retained. Opening an existing LevelDB without
CURRENT now fails before create_if_missing can replace its metadata. A real-disk
regression verifies that rejection preserves the files, and a later single-client
run preserved vault, CURRENT and NetworkBinding across close/reopen. The cause of
the original file disappearance remains unresolved; automatic repair is absent.
Loadgen also stops its runtime before destroying referenced Identity services.

Core suite: 267 passed; final Qt suite: 75 passed, including the simulation scope
and accepted-reference parser regression; Python report suite: 6 passed.
Build/test logs are under `artifacts/ux-r3-20261007/`. Failed runs
`battle/20261007-195003.md` and `battle/20261007-203645.md` remain evidence: the
first produced no load metrics, and the second finalized six operations but
timed out at one of two required replicas. Their results are not accepted
benchmark references.

Accepted reference: `battle/20261007-205703.md`, current DEVNET, files profile,
one Windows client/Identity, seven 64 KiB files, two-replica target, ordinary WSL
Full Node plus the existing DEVNET peers. All seven operations finalized, load
drained with no failed operations and clean vault-only restore verified 7/7
files after the source client stopped. Controller throughput was 7/88.8 s,
approximately 0.08 finalized op/s; client load/drain throughput was approximately
0.22 op/s. These denominators differ by documented reconnect/controller overhead.
The desktop displays the unrounded reference rate as 4.7 finalized op/min;
the artifact and historical report retain the per-second measurement contract.
This short low-rate workload is not a capacity benchmark. Windows and WSL share
one physical host; two placement addresses do not prove independent remote
machines or failure domains. No proxy, Geo bypass, additional signer or network
reset was used. Its reviewed public `benchmark_reference.json` is compiled into
the isolated desktop build; the user's running PoA desktop is untouched.
Native Windows light 1040×720 and dark 1280×860 reference captures were visually
inspected: the accepted historical number, French workload/window text, binary
hash and same-host simulation caveat are readable. Peer/map snapshots in those
captures are synthetic fixtures, separate from the real reference evidence.

## Desktop Network presentation package (2026-10-07)

The schematic Network map now uses a compiled Natural Earth mainland/Corsica
outline with preserved aspect ratio (284/32 simplified vertices). The generated
header records the GeoJSON SHA-256 and `tools/ui/generate_france_outline.py`
reproduces it offline. No map SDK, runtime geography fetch, city assignment or
regional boundary is introduced. P markers are mesh observations; L markers are
in a bounded compact LAN inset, including a selected peer when the inset overflows.

The normal map shows its Public CYBOU/France scope, local connection/protection
summary and illustrative-position note. One selected-peer card moves between
the map and Advanced without losing selection. Normal rows show admission,
connection and unverified advertised height; endpoint, StorageId and tip delta
appear in Advanced. Floating row height is measured after row widgets become
visible. Closing the icon clears the selection. Disconnected observations retain
no current storage proof or advertised-height claim.

Ahead height announcements and matching heights are explicitly unverified;
neither renders as proof of synchronization. The provenance timestamp is labelled
Last sync, not diagnostics freshness. These changes concern local presentation,
not consensus, peer admission or provider placement.

Native Windows light/dark fixture captures are under `artifacts/ux-r2-20261007/`.
The final MinGW GUI/test build passed and the complete Qt suite passed 74/74
after the advertised-height correction, including selected-card height, drawer
selection retention, ahead-announcement and disconnected-evidence regressions.
Network, floating card and Advanced captures were visually inspected at 1040×720
(light) and 1280×860 (dark); fixture snapshots are not network/benchmark evidence.
Full DPI and FR/EN acceptance remain. Benchmark,
VPS deployment and signer/network changes remain outside this package.

## Desktop UX first implementation package (2026-10-07)

Implemented against `7f043f95`: Compact/Toolbar utility icon buttons with accessible
names and keyboard focus; Home activity refresh and Identity shortcut, Identity
copy/lock and Settings download-folder controls use them. Primary and sensitive
actions keep their text and existing reviews. Home, Wallet, Mail support fees,
activity and wallet errors use Network balance / Solde réseau for the canonical
System Balance. Identity no longer duplicates Wallet balances.

Header search is contextual on Home/Mail/Files; other pages show their title.
Ctrl+K temporarily opens search there without navigation; Escape or focus leaving
the field restores the title. Subsequent single-header Mail/Files integration is
recorded in the package above. Suggestions are scoped to the corresponding product, and Home/global
suggestions still span both local indexes.

Validation: MinGW GUI and Qt test targets built; the complete Qt suite passed
74/74 results. Native Windows fixture captures at 1040×720 (light) and 1280×860
(dark) are under `artifacts/ux-r1-20261007/`; Home, Identity and Wallet captures
were visually inspected. Offscreen captures had missing font glyphs and are not
visual acceptance evidence. These are synthetic UI fixtures, not network or
performance evidence; Windows DPI and the complete FR/EN acceptance pass remain.

Subsequent benchmark work is recorded in the dated package above. Remaining
packages from the accepted desktop proposal include recovery and Mail/Files
polish, Console and Authority professional workflows.
No fresh benchmark, VPS deployment or network reset was performed in this package.

## Build, peer admission and battle evidence (2026-10-06)

- The build uses CYBOU CMake modules only (`cmake/CybouBuildType`, `CybouFlags`,
  `CybouTargets`, `ThirdParty`). Every CYBOU library and executable links
  `core_interface`: stack protector, `_FORTIFY_SOURCE=3`, CET, ASLR/NX/high-entropy
  VA, relro and warnings. The tree builds without warnings with MinGW GCC 13.
- A node accepts 128 inbound sessions, at most 32 per public IP (was 8); local
  addresses are not limited per address. Deployed to the DEV VPS bootstrap and
  both storage peers.
- The desktop network page no longer leaks peer-detail labels (a stack overflow
  while painting) and refreshes only while shown. The status refresh no longer
  re-enables the PoA signer on the GUI thread.
- `tools/battle/battle_test.py` adds a clean-machine restore phase (each Identity
  restarts from its vault alone and reads every protected file and incoming mail
  back from the network) and a PASS/FAIL verdict with exit code. Provider loss,
  repair and settlement are not yet exercised live; a full run with the restore
  phase has not completed yet.

## Bounded console and redundant diagnostics removal (2026-10-06)

The duplicate diagnostics text window and its entry points are removed. Its six
values remain in Network Advanced. The menu now opens the read-only console.
Blank French multiline translations caused `help`, `status` and `storage` to
produce empty responses; the console translations are complete and help derives
from the actual command registry. Public local diagnostics, unlocked own-account
queries and genesis-key-proven Authority queries have separate dispatch/help
availability. Authority proof loss and locking clear private output and history.

Storage uses the runtime's measured physical counters rather than guessed default
capacity. Unknown replicas and finality remain unknown. The former `chunks`
implementation synthesized SHA-256 digests from root text/index and called them
verified BLAKE3 leaves; this false evidence is removed. Real chunk leaf inspection
is unavailable through the current semantic model and the command says so.
Lists, input, output and history are bounded. The UX contract records the next
API-backed inspector/explorer work without advertising unimplemented commands.

Validation: the native Windows Qt suite passed 73 results, including French
command responses, ordinary-versus-Authority dispatch, proof-loss/lock cleanup,
input/history/output bounds and truthful chunk evidence. The main desktop binary
was rebuilt and deployed locally. Native French console screenshots and the full
log are in `artifacts/console-20261006/`. This does not claim live-network audits
or delivery of the future chunk/history APIs.

## Network and unlock presentation (2026-10-06)

The local UI worktree combines Network and Diagnostics: the sidebar caption
shows the current network. The map fills the entire Network page below the
header; peer details and a scrollable Advanced drawer overlay it without resizing
it. Public markers are pulled inside the silhouette; LAN observations use a
separate bounded inset. Arrow keys select peers. Formerly connected endpoints
remain as bounded (100), volatile session observations, shown pale with explicit
disconnected text and unknown current height; switching network clears them.
These observations are not a discovery census or current reachability evidence.
Protection uses protected/total. Network refreshes coalesce; hidden diagnostics
pause presentation, and diagnostic rows update values without widget teardown.

After Identity unlock, a modal branded preparation view follows private DB
opening, history discovery and semantic content preparation. Known scan counts
provide progress; opening is indeterminate. The modal closes after applying a
usable Mail/Files projection, or confirming a genuinely empty complete scan.
Missing roots/failures remain explicit, with a local-view escape and a lock action.
Network connectivity is informational, never a prerequisite for local readiness
or Identity creation. Old session replies cannot revive the modal after locking;
failed catch-up scans retry at the normal worker interval instead of spinning.

Validation: the final native Windows Qt suite passed all 73 results, including
coalesced status bursts with the drawer hidden/visible, full-page geometry,
loading/failure/lock behavior and genuine empty projections through the core
adapter. Native screenshots cover 1040x720 and 1280x860 with synthetic fixtures.
Final logs and screenshots are under `artifacts/network-loading-20261006/`.
The main `build_cybou_qt_mingw/bin/cybou.exe` was rebuilt successfully. This does
not establish live-network performance acceptance or update the VPS deployment.

## Desktop UX source review (2026-10-05)

Source inspection at HEAD `cc6c18e` plus the current worktree confirms live
Mail/Files, encrypted local drafts, attachment references, application history
indexes, the extracted Identity worker/projections and existing Authority
finalizer/settlement controls. The Files product contract's former statement
that the encrypted catalog was not connected was obsolete.

The first implementation slice, based on HEAD `77a29d2`, is now in the working
tree: correlated Queued/Running/Committed/Failed Mail tasks, archive batch
acknowledgement and Undo after commit, debounced draft autosave with save-error
retention, and send acknowledgement after durable publication-job ownership.
An encrypted draft-to-message binding survives restart/stale compose replay;
successful handoff retains this local binding, explicit discard removes it.
One failed worker command no longer drops the remaining dequeued batch.
The binding retains a private recipient/text/attachment fingerprint: altered
draft content cannot silently resume an earlier saved publication. Preparation
can replace this fingerprint only before a durable publication job exists.

Files Advanced and detail scroll survive snapshot updates; active-job copy
counts are filled and unknown counts are explicit. Disabled Files-to-Mail and
download actions explain their protection gates. Folder destinations use IDs
and breadcrumbs. Downloads use atomic QSaveFile commit, preserving an existing
destination on retrieval/write/commit failure. Unchanged semantic Mail/Files
snapshots suppress signals and Home avoids unchanged activity reconstruction.
Mail row children pass mouse input to the existing drag/select viewport.

The next slice adds stable Identity-local activity IDs, cached Home rows with
in-place label updates, coalesced manual local-view refresh and a timestamp
scoped to that refresh. The worker reads semantic indexes without forcing
network sync, scanning history, auditing providers or creating operations.
Failed/interrupted refresh allows retry; old request/session replies are
ignored. Lock clears activity, row caches and refresh presentation. Home
scrolls vertically at small window sizes and wraps activity as plain text.

Remaining delivery work includes physical mouse drag acceptance, incremental
Mail rows and ID handoff, initial large-Files presentation cost, evidence
timestamps and measured
worker/shutdown latency. Folder-import discovery is now bounded and asynchronous
with cancellation; large-catalog responsiveness still needs measurement. Files-to-Mail remains
restricted to Protected references; pending-content compose needs the durable
reference design in the delivery plan. Map/explorer/console and the broader
assurance packages remain target work.

Current diagnostics do not supply a global node census, city locations, remote
uptime or canonical placement reliability. A schematic local-peer map and
scoped metrics are feasible targets; broader observability and ranking require
new evidence/privacy design. Existing distinct DEV storage peers share a host
and do not prove Beta failure-domain independence.

See [`DESKTOP_UX_DELIVERY_PLAN.md`](DESKTOP_UX_DELIVERY_PLAN.md) for delivery
dependencies and [`NETWORK_AND_ADVANCED_UX.md`](NETWORK_AND_ADVANCED_UX.md) for
new product boundaries. Validation for these slices on HEAD `da6d6b9` plus the
current UI worktree: all 55 Qt shell results passed (five new regression cases),
and all 12 ApplicationService cases passed. Focused native Windows checks cover
activity, refresh/session failures, Advanced, mailbox acknowledgements, drop
event routing and supported window-width constraints at the current display
scaling. Logs and the source manifest are under
`artifacts/uiux-implementation-20261005/`. A separately linked/deployed GUI
review build avoids replacing the executable/DLLs held by the running desktop.
These are local component/Qt checks, not distributed durability, native drag
or live-network acceptance. The implementation is not deployed to the running
desktop or VPS.
This page describes
implementation and deployment reality; `AGENTS.md` and the frozen architecture
define the target. The changes described here include committed local development; commit status is not release or deployment evidence.

## Target architecture vs. current code reality

| Component | Target constitution | Current code reality |
|---|---|---|
| Official networks | Compiled DEVNET public constants; MAINNET unprovisioned and GUI-disabled | Implemented (`d5d90cb`): one `OfficialNetwork` per kind via `RequireOfficialNetwork`. Current compiled DEVNET contains the immutable signed genesis under its new NetworkID. Retired DEVNET is not accepted. MAINNET throws as not provisioned; the GUI has no network selector and always runs DEVNET. |
| Network identity | `NetworkID = Network Public Key` | Implemented: verified genesis carries the exact key; every 32-byte field is named `network_binding` and equals `ComputeNetworkBinding(key)`; `GetNetworkId()` exists only for the exact key bytes of a verified genesis. The shared `MakeNodeRuntimeConfig` derives the verified genesis and rendezvous locators from `OfficialNetwork`; the genesis digest marker is removed from the StateStore. |
| Signed genesis | One immutable offline-signed `NetworkGenesis` object and initial state compiled per official network | Implemented: the compiled genesis must carry exactly the compiled Network Public Key and its initial state root. CYG1/CYN1, `CybouNetworkFile` and every file loader are removed. |
| Official startup | Select compiled public constants; verify signature and initial state root; no external official file or separate digest pin | Implemented: every `cybou` command, loadgen, storage smoke/soak and the desktop start only from a compiled network (`--network devnet`). No profile digest pin; the verified genesis digest anchors height zero. |
| Network Private Key | Strictly offline, signs genesis once; private material stays under gitignored `/private/` | `cybou-provision` is an explicitly built offline tool (`BUILD_PROVISION_TOOL=ON`), separate from production `cybou`; `verify-devnet` checks existing secrets without signing genesis. Existing constants and secrets cannot be overwritten. The retired DEVNET material is kept under `/private/devnet-retired-20261003`. |
| Peer discovery | Every node listens and shares dialed and verified inbound peers (DEC-287) | Implemented: HELLO is 82 bytes with `listen_port`; `InboundPeerServer` records listening inbound peers as candidates, `SyncFromConfiguredPeer` connects back to one per pass and adds it to the gossiped discovered set. Desktop, `node run` and loadgen listen by default (29461, else any free port). Before this, peers that only connected inbound were never shared and a desktop found no peer but the bootstrap. |
| Bootstrap | Ordinary CYBOU full peer | Bootstrap binding/protocol/store, `BOOTSTRAP_REQUEST/RESPONSE` and the `cybou-bootstrap` executable are removed. Nodes and the desktop dial the compiled locator first and check its SPKI pin; the locator node serves `--tls-certificate/--tls-key`. Every process uses the same Full Node network lifecycle; signer activation does not reconnect peers. Pinned rendezvous endpoints are protected from discovered-peer crowd-out. The DEV VPS runs the ordinary `cybou node run` command on current current DEVNET. |
| Geo updater | France-only admission with a valid local dataset; fail closed otherwise | HTTPS and published archive SHA-1 verification, bounded gzip decode, SHA-256 CSV cache and validated atomic installation. Each failed attempt re-fetches metadata and archive; five attempts use interruptible 0/2/5/15/60s delays. Exhaustion retains valid cache and retries in 1h, or retries in 5m without a valid dataset. Successful/current checks use 14 days. Test-only fetch/wait hooks cover publication mismatch, rejected candidates, cache retention, strict temporary cleanup and cancellation without network access. |
| Consensus bootstrap state | No grants, roster, or network-role announcements | Removed. |
| Consensus state | Unified current state format | State with Treasury monetary base, onboarding-origin System Balance, settlement cursor and storage leases (M5); no OnboardingPool; older decoders removed. No AUTH or usage counters (DEC-284). |
| Relay proof-of-work | Every user operation carries flat PoW checked by every Full Node and the PoA (DEC-273, DEC-284) | Implemented: `operation_work.h` (`CYBOU/OP-WORK`), `OPERATION_WORK_BITS` 22, names +4; `OperationPool::Admit` and revalidation check it; `OP_META` carries `size u32 + nonce u64`; `CybouNodeRuntime::PrepareOperationWork` solves outside the runtime lock and caches; CLI `operation submit` solves before sending. Never stored in blocks. Component tests set `NodeRuntimeConfig.operation_work_bits = 0`. |
| Candidate execution | Every full node executes candidates before relay; one pool per node, used by PoA for blocks | Implemented (`7f447d4`): `CybouNodeRuntime` owns the single `OperationPool`; local, peer and relayed operations pass `OperationPool::Admit` before staging or forwarding; every node revalidates after each finalized block. CybouNodeRuntime produces from that pool; PoaFinalizer only signs with its durable journal. |
| Anti-spam | Fees, storage rent and flat relay PoW; no AUTH, tiers or Validation (DEC-284) | Implemented: AUTH, `AccountUsage`, operation tiers, `PoaAuthAdjustment`, `ValidationAttestation`/`ValidationPool` and their P2P messages are removed. `CybouState.publications` remains the publication register. |
| Mutual Proof of Storage | Randomized challenge-response byte-offset and nonce auditing | Cryptographic primitive implemented (`b01f2dc`): `src/cybou/storage_audit.h` and `.cpp` provide `StorageAuditChallenge`, `CreateStorageAuditProof`, and `VerifyStorageAuditProof` using BLAKE3 chunk sampling (test coverage in `cybou_finalized_chunk_store_tests`). Network mutual-audit protocol, PoA notarization, and reliability coefficient in state: NOT IMPLEMENTED. |
| Object Pruning | Author `RevokePublication` retires the record and providers purge chunks (DEC-271) | Implemented: `RevokePublication` (ProtocolOperationKind 8, IdentityOperationKind 6) is owner-only, costs the payment fee and closes the lease; a revoked publication fails `FindFinalizedRootPublication`, so no new admission; `FinalizedChunkStore::PurgePublication` runs on each finalized revocation and deletes chunks no other publication authorizes. Desktop Mail/Files revoke automatically: once the application index is complete and every own job is finalized, `PublicationService::RevokeUnreferenced` revokes one own publication at a time (never recovery bridges, keeping 5 operations of the window for the user) when no catalog record came from it, no non-deleted message is it and no live file or attachment tree lives in its leaves; indexing skips revoked publications. "Delete forever" and "Empty Trash" can trigger revocation and managed purge once their prerequisites are satisfied; they do not erase historical capsules, retained keys, recipient copies or prove physical deletion. The fix committed in `a3f05aa` journals pending purge, retains quota on removal failure and retries on reopen and maintenance; startup reconciliation covers revocations whose local purge event was missed. |
| Executables | One production `cybou` | `cybou` is the desktop and the headless CLI (`src/cybou/cli/cybou_cli.cpp`, one parser, no internal positional commands); `BUILD_GUI=OFF` builds a headless-only `cybou`. `cybou-node` and the `network bootstrap` command are removed; `network info` lists the locators. `cybou-loadgen` builds only with `BUILD_TESTS`. |
| Operation routing | Uniform CYBOU P2P Full Node mesh | HELLO has no capability field. No PoA transport proof or special route. Sync completion is advisory. Storage is intrinsic; StorageId is challenged only for storage interaction. |
| PoA | Sole independent canonical finalizer | Single-operator PoA re-executes every candidate through the node pool and finalizes; a multi-node CYBOU P2P test covers ordinary nodes and PoA. |

## Finalized-state runtime snapshot (2026-10-05)

`CybouStateStore::GetStateSnapshot()` shares immutable finalized state through
an atomic `shared_ptr<const CybouState>`. First access validates the persisted
state, hash and network binding; genesis initialization and both ordinary and
min(BlockID) conflict commits publish a replacement only after the KV batch
succeeds. Allocation precedes the durable write. Empty no-op blocks retain the
same state object. Existing readers can retain the previous immutable state.
Runtime, operation admission, Identity/application services and desktop state
reads use this snapshot; `LoadState()` remains an explicit disk integrity read
and is retained at runtime startup. Direct mutation of the underlying database
while its StateStore is active is unsupported. Wire bytes, genesis, consensus
execution and durable PoA safety checks are unchanged.

Local validation: Windows MinGW Release headless and Qt GUI builds passed;
87 selected runtime/state/PoA/Identity/application/publication/storage regression
tests passed with 1,600 assertions. Snapshot-specific checks cover pointer reuse,
retained-reader immutability, commit/reopen equivalence, rejected/empty blocks,
corrupt persisted hash on reopen, and min(BlockID) conflict publication. This is
local component evidence, not deployment, battle, soak or throughput evidence.

This implements R1 of the performance refactoring proposal. Bounded storage
I/O, PoA event wakeups, application history indexes, Qt projection extraction
and battle/soak profiling remain separate work; no throughput improvement is
claimed without measurement.

## Runtime ownership domains (2026-10-05)

R2 retains `CybouNodeRuntime` as the desktop/headless facade, with three private
owners inside the same Full Node: `ChainCore` owns the database, state store,
candidate pool, PoA signing/retry state, Identity coordinators and operation
relay/status; `NetworkCore` owns sessions, peer retry/discovery, ingress limits
and routing hints; `ProviderCore` owns encrypted blobs, provider admission,
retention and the stable storage secret. Their implementations live in
`node_runtime_chain.cpp`, `node_runtime_network.cpp` and
`node_runtime_provider.cpp`. The facade keeps construction, aggregate diagnostics and cross-domain
admission/relay, finalized purge events and storage payout binding orchestration. These are internal ownership boundaries, not network roles or
independently exposed services.

Routing hints have a short dedicated NetworkCore mutex instead of the chain
mutex: peer callbacks can consult routes while session I/O owns its mutex,
and route access does not acquire chain/session locks. Existing session ->
chain callbacks and separate diagnostic lock scopes are retained. Provider
storage retains its own store locks. Destruction closes sessions before
provider and chain stores and cleanses the provider secret even if facade
construction fails after provider initialization. Storage endpoint binding
verification shares R1's immutable state rather than copying it.

Local R2 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 252 cases and 100,542 assertions, including
routing/discovery, synchronization, operation relay, provider admission,
revocation/purge, Identity recovery and PoA conflict/signing safety. The facade
implementation is 237 lines after extraction. No deployment or performance
benchmark is implied by this component evidence.


## StorageService responsibility extraction (2026-10-05)

R3 retains one Identity `StorageService` and the existing `StorageTransport`
interface. Private components have distinct ownership:

- `PlacementRepository` owns encrypted placement/rebuild records, exact-consumption
  decoding and atomic placement/index persistence;
- `EvidenceLedger` owns signed receipt persistence, bounded provider evidence,
  volatile replica verification times, shadow accrual and verified-slot counting;
- `ReplicaVerifier` owns random-offset challenges, full GET/ChunkID validation
  and evidence updates over the existing transport;
- `ProviderSelector` owns CSPRNG ordering by verified payout account (falling back
  to StorageId) and endpoint shuffling for recovery GET.

Their implementations are separate translation units; `storage_service_internal.h`
contains private declarations. Placement/audit/repair coordination lives in
`storage_service_repair.cpp`; recovery, public projections and settlement
preparation remain in the main service. No independently exposed storage service,
provider role, wire entity or canonical evidence is introduced.

Application DB keys and current binary layouts are unchanged. The ledger retains
its own evidence mutex; placement coordination retains the service mutex and
per-publication guard, releasing it during remote I/O. Existing rent caps, integer
rounding, payer exclusion, distinct economic identities, first-failure replica
downgrade and restart closure of credited intervals are preserved. Settlement
preparation obtains aggregate verified slots from the ledger without accessing
its maps or mutex directly. No evidence transport to PoA or parallel storage I/O
is implemented by this extraction.

Local R3 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 253 cases and 104,023 assertions. Added
regressions verify that persisted counters/placements do not authorize settlement
after reopen until fresh replica checks, and truncated evidence or an invalid
rent remainder are rejected without deleting valid placement metadata. Existing
coverage verifies signed receipts, restart accrual, repair without local cache,
partial rebuild persistence, payout identity deduplication, concurrent inspection
while placement I/O runs and P2P PUT/GET/audit. The main service implementation is
378 lines after extraction. This is component evidence, not deployment, soak or
measured concurrency/throughput evidence.


## Bounded concurrent storage I/O (2026-10-05)

R4 adds one node-local `StorageIoScheduler`: four persistent workers, at most
eight queued jobs, one active job per StorageId and two read/verification jobs.
The read budget conservatively includes random-offset checks that can fall back
to full GET. GET/proof recovery also passes through this scheduler. These are
local resource limits, not protocol roles or consensus parameters.

Placement plans up to four distinct economic identities for one chunk, sends
PUTs concurrently, collects every result, verifies each StorageId-bound receipt,
and checkpoints the placement once per completed batch. A batch shares one
owned ciphertext; it never queues an entire file. Audits check the current
chunk's replicas concurrently and retain exact-byte verification and evidence
rules. Per-publication guards serialize audit, rebuild and placement mutations,
while semantic inspection remains available during remote I/O. Exceptions are
returned through futures; queued jobs drain before runtime domains are destroyed.

Remote storage requests use a node-local pool of at most four cached ordinary
P2P sessions, one in flight per proven StorageId and two full GETs. Each session
is exclusively leased, separately proves the expected StorageId and uses the
same TCP deadline, Geo admission, compiled TLS pin, HELLO network and known-chain
checks as mesh sessions. Admission is rechecked on reuse. Storage I/O no longer
holds the mesh manager mutex; discovery/relay/sync remain in PeerManager. The
old PeerManager storage-transfer path is removed. No wire, genesis, canonical
state, payout formula or database format changes.

Concurrency is covered by gated tests for worker/read/provider budgets, exception
release and shutdown draining; simultaneous PUT/GET and receipt accounting;
invalid-receipt rejection and missing-replica resumption; and real TLS/HELLO/
StorageId session overlap and reuse. No performance multiplier is claimed;
battle/soak and measured throughput remain R8 work.

Local R4 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 257 cases and 97,660 assertions. This is
component and local integration evidence; no VPS deployment or soak was run.

## Event-driven PoA production (2026-10-05)

R5 replaces the 100 ms production polling tick with a runtime condition variable
and a local change revision protected by the chain mutex. New locally executed
candidates (local submission, mesh relay or signed settlement), signer changes
and finalized-head/pool revalidation wake the worker. Duplicate or rejected
admissions do not generate candidate wakeups. Reading the revision before
readiness and checking it under the same mutex in the wait predicate prevents
lost wakeups. Shutdown sets its stop flag and notifies this wait explicitly.

Idle or signer-disabled production waits without a timer. Pending work waits
until the successful-block interval or transient retry deadline, unless a state
change or stop occurs first. Existing exponential retry (100 ms to 5 seconds),
exact journaled candidate reuse and fail-closed safety halt remain intact.
DEC-286 now describes event-driven scheduling; non-empty automatic blocks,
manual finalization, height-counted windows, wire, state and durable signing
rules are unchanged. No network migration or genesis change is required.

Local R5 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 260 cases and 101,813 assertions. New tests
cover pre-wait events, idle/stop/deadline predicates, relay admission versus
duplicate suppression, block-interval enforcement, prompt shutdown and pending
work after worker restart. Existing service coverage verifies exact-candidate
retry after signer failure and resumed production after signer unlock.

## Local finalized-event coordinates (2026-10-05)

R6 adds `finalized_event_index.cpp` behind `CybouStateStore`. The current local
`cybou/events/` namespace contains operation coordinates (including publication,
rotation and revocation), publication-bearing heights, KEM coordinates by public
AccountID/epoch and a complete-head marker. Each coordinate is exactly 44 bytes
(height, operation index, BlockID); ordered height/epoch keys use fixed-width hex.
There is no recipient index or cached plaintext. New entries and removal of a
losing same-height canonical block are part of the canonical commit's atomic
batch; the index is not committed by state root and changes no wire/genesis.

Missing, stale or malformed complete-head metadata triggers a rebuild from
retained blocks. Rebuild invalidates the marker first, clears derived namespaces
in bounded batches and verifies parent continuity plus hybrid PoA certificates
before publishing a complete marker. Interruption never leaves a complete partial
index. Positive lookup coordinates are checked against the canonical source
block, operation identity/type and parent link; malformed/stale operation or KEM
rows cause one rebuild attempt. Missing referenced source history yields unavailable, and
no index is treated as independent proof of finality. Normal lookup validates
its source block, rather than re-verifying the entire preceding history each time.

Runtime operation/KEM lookup uses these coordinates. ApplicationService queries
bounded ordered publication-height ranges, skips unrelated heights and retains
atomic per-relevant-block private records/checkpoints and bridge rescanning.
Each range contains at most 256 relevant heights; semantic capsule filtering,
unavailable-content retries, own-publication recovery and monetary rules remain
in their existing services. `KVStore::ForEachStringRange` supplies an inclusive
fixed-length-key range with early termination. Rebuild requires retained source
blocks; deleting all canonical data still requires ordinary verified mesh sync.

Local R6 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 263 cases and 98,100 assertions. New
regressions cover malformed/missing operation/KEM rows, marker deletion and
store reopen, rebuild failure with absent source blocks, unchanged canonical
state root, removal of losing-branch coordinates and sparse Mail recovery after
private Application DB deletion. Existing rotation, revocation, storage admission,
bridge recovery and receipt/settlement tests also pass. This is local component
and integration evidence; long-history recovery timing remains R8 profiling work.

## Qt Identity session responsibilities (2026-10-05)

R7 splits the Qt adapter's implementation into private Identity session,
scheduler, Mail, Files and storage translation units. `IdentitySession` owns the
encrypted Application DB and the existing three core services, plus rotation
preparation. `SessionScheduler` owns the sole Identity worker, command queue,
stop-aware interval wait and shutdown drain. The session stops and joins it
before destroying any service or projection.

`MailProjection` owns the unindexed outgoing Mail overlay and semantic Mail
snapshot construction. `FilesProjection` owns pending catalog mutations,
item-to-job associations and the local availability cache. `StorageProjection`
owns shared publication job results and the existing audit, GC, durability,
reference-revocation and settlement preparation work. Mail/Files command entry
points remain the same adapter API; their implementations live with the relevant
projection. GUI draft/delete/star reconciliation stays on the GUI thread, and
queued state delivery retains the session-generation check. No extra Identity
worker or core service is introduced.

Local R7 validation: Windows MinGW Release GUI and native Qt test targets built
successfully. The full offscreen desktop suite passed all 50 cases, including
live Mail/Files publication, draft/delete/star reconciliation, close/reopen,
private-content locking and recovery-phrase rotation with the live session.
This is local regression evidence; battle/soak and profiling remain R8 work.

## Storage economy status (2026-10-04)

DEC-274–DEC-283 are frozen as target architecture (M1). M2 is implemented:
`NodeRuntimeConfig::storage_capacity_bytes` is the explicit local capacity `V`
(default and minimum 15 GiB outside memory-only tests, `cybou node run
--capacity`), `ChunkBlobStore` rejects new blobs beyond `V` with
`CAPACITY_EXCEEDED`, and `FinalizedChunkStore` receives the provider budget
`ProviderBudgetBytes(V) = floor(2V/3)`; diagnostics report local and provider
usage separately. The desktop has no capacity picker yet and uses the
15 GiB default. M3 is implemented: successful admissions return a signed
`StorageReceipt` (P2P `CHUNK_ADMISSION_RESULT`), StorageService counts a replica
only with a receipt from that StorageId and keeps it in the encrypted
Application DB, `STORAGE_AUDIT_CHALLENGE`/`RESPONSE` (25/26) carry random-offset
audits, and `StorageService::ProviderEvidence()` exposes bounded in-memory
rolling evidence. Replicas still drop on the first failed check.
M4 shadow accounting is implemented: `storage_economy.h` holds the exact
integer rent arithmetic (512 KiB units, 5 CYBOU/GiB/day/replica, floor with
carried remainder). StorageService credits verified billing-unit-seconds only
between two successful checks of a replica (receipt opens the interval, gaps
capped at 24 h, failure or restart closes it), accrues a shadow reward per
provider, persists evidence in the encrypted Application DB and reports
`EstimatedDailyRent()`. Diagnostics show a provider-side estimate. No CYBOU
moves; measured DEVNET numbers will validate the rate before M5.
M5 consensus economics is implemented in `state.cpp`, `storage_lease.h/.cpp`
and `block_executor.cpp`: no MAX_SUPPLY or OnboardingPool, `TotalCybou`
conservation, Treasury-funded 20,000 onboarding, onboarding-origin tracking,
`RootPublication.lease_periods` (initial lease paid atomically), `StorageLease`
(operation kind 9) and PoA-signed `StorageSettlement` (kind 10, contiguous
86,400 s periods, per-lease period cap, no self-payout, origin-preserving
payouts and refunds). Storage quotas are removed; `MAX_PUBLICATION_CHUNKS`
remains a safety bound. Providers admit chunks only under an active lease;
PublicationService leases at publication and renews inactive leases. The
Central Authority desktop settles one complete UTC period per click
("Settle storage period"): `StorageService::SettlementEntries` splits each
active lease's period cap over its `units × replicas` slots and pays the live
payout account of every replica verified since the period start; the
settlement also advances the cursor and refunds ended leases. Limitation: the
PoA pays only leases whose placements its own Identity holds; evidence of
other payers is not yet transported to the PoA, so their escrow is refunded
at lease end. Not yet implemented: that evidence transport, lease renewal UX. Because the state and genesis formats changed, the
previous compiled DEVNET was retired. After M7, AUTH and Validation were removed
(DEC-284), changing the state format again: main compiles the DEVNET
`eee26eca…3665` (`verify-devnet` passes); the DEV VPS runs it with
`--capacity 40GiB`; desktops cut over on demand.

M6 adversarial evidence (`cybou_resource_limits_tests`,
`cybou_storage_placement_simulation_tests`):
- monetary conservation: a 400-block deterministic random walk over SystemLock,
  publish-with-lease, StorageLease, RevokePublication and StorageSettlement keeps
  `TotalCybou` exact and every state canonical;
- payout attacks refused: unknown lease, missing payout account, self-payout,
  over-cap or over-escrow payout, duplicate/unordered/zero entries, replayed or
  gapped period, overflowing period start, foreign signer, missing PoA key,
  oversized or overflowing lease extension;
- storage failures: existing suites cover lost, corrupt and offline providers,
  bit rot healing, repair without local cache and audit-detected wrong answers;
- concentration (1000 nodes: 700x15 GiB, 200x150 GiB, 80x1 TiB, 20x20 TiB) under
  the implemented uniform selection: at 5%/30%/70% demand the top 1% of nodes
  hold 2.8%/11.9%/33.9% of replicas, the 5 largest 1.4%/6.0%/17.0% (gate < 70%),
  effective provider count 678/161/43, and every home node receives work.
  Capacity-weighted selection would give the top 1% ~39% and leave 60% of home
  nodes idle at 5% demand;
- Sybil: with selection by payout account (implemented, DEC-280), 100 nodes of
  150 GiB under one account get 0.11% of replicas, the same as one 15 TiB node;
  per-StorageId selection would have given them 10.15%. Splitting into 100
  separate Identities still reaches 10.4% at 10% demand; only AccountCreate PoW
  prices that (open question in `25_OPEN_QUESTIONS.md`).

## Evidence limits reviewed on 2026-10-04

Committed code baseline: `a3f05aa`; local Windows headless verification passed
230 core cases / 9414 assertions. This is not GitHub CI, Linux, desktop UI or
deployment evidence. Subsequent governance commits changed documentation.

- Replica placement counts distinct proven StorageIds, not independently owned
  disks, hosts or operators. Physical independence remains the Beta target.
- StorageService checks replicas with random-offset audits over CYBOU P2P and
  full GET plus BLAKE3 one time in eight (always without a local copy). Per
  provider evidence (receipts, successes, failures, full verifications, times)
  is bounded; receipts and the evidence index persist in the encrypted Application DB.
  PoA settlement exists only for leases whose placements the Central Authority
  Identity itself holds (see the storage economy paragraph above).
- Repair is attempted from valid surviving bytes to available providers. Finality
  and admission ACKs alone do not prove current availability or recoverability.
- Local allocation and finalized quotas do not measure actual 1:3 reciprocal
  contribution. Signed storage receipts prove admission only.
- Finalized revocation stops new admission and initiates compliant-provider
  purge of unshared chunks. It does not remove historical capsules or establish
  per-object crypto-erasure. Recipient and adversarial copies are outside purge.

These are implementation limits, not changes to the frozen target decisions.

## DEVNET-only development (2026-10-03)

Runtime supports official DEVNET and the disabled, unprovisioned MAINNET only.
The alternate development profile, fixed test-network key derivation, runtime
genesis creation, seed-export command and dedicated build option are removed.
Development and integration use the existing compiled DEVNET with its saved
local keys. This removal changes no keys, NetworkID, genesis or canonical state.

France-only peer admission applies to desktop, headless nodes and development
clients. The private-route bypass and its GUI/environment/CLI paths are removed.
Geo diagnostics now have Waiting and Ready states. Event logs offer minimal or
detailed public fields; neither mode changes network policy.

The automatic multi-process/fault controller and its templates, namespace helper
and controller tests are removed. CLI acceptance connects an ordinary temporary
Full Node to existing DEVNET without a signer or operation submission. Storage
clients retain ordinary DEVNET admission and finality requirements. Component
tests use in-memory fixtures and synthetic Geo input through the actual parser;
these fixtures are not available as a runtime network profile.

Current NetworkBinding:
`eee26eca805d5a16b2f550d66b3355ecf50d90c43138bc1fefc34df92f7f3665`.
Signed genesis anchor:
`1abf2355b6597e53c6affb73d4c92bacc9b970f956a1a259bac8c3699f958cfc`.
Genesis allocations: `cybou` 100,000,000,000 CYBOU (Central Treasury; same
phrase and PoA key as the retired DEVNETs; AccountID is created on initial
onboarding into the current network); `bootstrap` 0 CYBOU. The private
material is under gitignored `private/devnet-no-auth/`. Production
Network Root derivation/signing are disabled. The existing official PoA is
locked; finality/content integration requires the operator to unlock it.

The VPS runs the ordinary Full Node at `51.255.46.58:29461`. Public peer admission
uses verified DB-IP Geo data and the compiled TLS pin. The earlier desktop/VPS
network domains remain retired; this pass does not reset or import them.

On 2026-10-04 the operator explicitly authorized a coordinated destructive
DEVNET reset with the exact existing keys and genesis. The preceding active
Windows state was archived under `CYBOU-retired-20261004-live-reset`, the VPS
state under `/var/lib/cybou/node-retired-20261004-live-reset`, and local WSL live
test state under `~/cybou-live-retired-20261004`. The height-2369 Windows chain
had a height-zero PoA journal halted with `HISTORY_MISMATCH`; that evidence was
archived rather than silently repaired. This is a development reset under the
explicit `AGENTS.md` exception, not a production recovery claim. The NetworkID,
genesis and TLS pin above remain unchanged. Historical blocks must not be imported
into the restarted exercise; they remain cryptographically valid.

See [DEVNET development](DEVNET_DEVELOPMENT.md) for commands and operator rules.

The 2026-10-04 live exercise resumed PoA on Windows and verified remote Mail and
Files publication and recovery through the VPS, including clean application
index and chunk loss. See [live content acceptance](DEVNET_LIVE_ACCEPTANCE.md)
for tested scope, repairs, the Windows transport workaround and economic limits.

Verification results must be attributed to the tested revision and build.
Committed baseline checks are recorded in [Data assurance and erasure](DATA_ASSURANCE_AND_ERASURE.md); they do not establish desktop CI or deployment status.
The DEVNET CLI acceptance reaches the existing pinned locator, restarts an
ordinary node and checks read-only doctor without activating a signer or
submitting operations. Production Windows GUI and Linux headless builds pass;
both linked Network Root guards reject private derivation/signing. The VPS
service is active and desktop/VPS doctors report READY on the unchanged genesis.
Its binary update preserved state. Finality/storage fault scenarios are not run
against the locked official signer during this removal.

Files private IDs are allocated once in the model and reused by the core adapter
for uploads, folders, copies and attachment saves. Pending-to-indexed Files
identity therefore requires no replacement mapping. See the live-core Qt
regression and delivery plan for current validation.
Validation for this slice: full Qt suite 55 passed/0 failed and native Windows
focused suite 5 passed/0 failed (including setup/cleanup); isolated review build
passed, based on `1ee870f` plus the UI worktree.

Folder-import slice (2026-10-05): background bounded discovery (10,000 entries,
64 levels) precedes publication staging. Nonmodal progress/Cancel, bounded GUI
batches, safe partial cancellation, hidden/empty folder preservation and lock/
page-lifetime handling are implemented. Isolated build passed; full Qt suite
56 passed/0 failed and focused native Windows suite 5 passed/0 failed (including
setup/cleanup), against `1ee870f` plus worktree. Blocking-volume cancellation and
large-catalog latency remain evidence gaps; cancellation does not undo accepted
publication jobs. Logs are in `artifacts/uiux-implementation-20261005/`.

Files row slice (2026-10-05): retained ID-keyed table/grid items, metadata/status
updates in place, selection/current-item continuity across refresh/sort/rename,
visible-anchor preservation on table insertion, view-mode selection transfer,
removal/filter cleanup and lock clearing are implemented. Child-count aggregation
is linear in the catalog and glyph icons are shared. Native synthetic 2,000-item
construction took 796 ms; ten one-file updates had median 11 ms/max 12 ms. This
is local fixture/model/page timing, not live storage or completed-frame evidence.
Per-item widgets still make initial presentation expensive; delegate/lazy-view
work and larger-catalog/frame benchmarks remain required.
Final isolated review build passed; full Qt suite 58 passed/0 failed and native
Windows focused suite 7 passed/0 failed including setup/cleanup, against
`1ee870f` plus worktree. Logs: `qt-file-rows-final-full.txt` and
`qt-file-rows-final-windows.txt` in the UI artifact directory.

Files delegate slice (2026-10-05): one viewport status delegate replaces per-row
QLabel widgets; plain display/accessibility text and full tooltips are retained.
The Qt table accessibility API returns the updated status. Synthetic native
construction/update times are 90 ms / median 8 ms, max 9 ms for 2,000 items;
527 ms / median 43 ms, max 49 ms for 10,000. These are fixture model/page timings
without completed-frame, live runtime or physical screen-reader evidence.
Full Qt suite 58 passed/0 failed; Windows focused 7 passed/0 failed and 10,000-item
run 3 passed/0 failed including setup/cleanup. Isolated review build passed and
list/upload screenshots were inspected. Sources: `1ee870f` plus worktree; logs
`qt-file-delegate-*.txt` in the UI artifact directory. Remaining performance
acceptance concerns large snapshot updates, scrolling and frame completion.


Mail reconciliation slice (2026-10-05): visible list items are retained by ID,
unaffected row widgets are reused, and only changed rows are repainted/replaced.
Selection, current item and scroll anchor survive metadata updates/insertion;
filtered/removed selections are dropped and lock clears private row caches/search.
Folder targets/count labels are retained. Outgoing Archive/Trash rows show the
recipient. The model adopts a replacement outgoing ID before removing the old
projection; list and reader transfer their current ID/selection to it. Move
acknowledgement and durable draft/send ownership rules remain in force.

Isolated review build passed; full Qt suite 59 passed/0 failed
(`qt-mail-rows-full.txt`), focused Windows suite 6 passed/0 failed including
setup/cleanup (`qt-mail-rows-windows.txt`). Regression covers a 100-message list,
retained unrelated widgets and folder target, badge count changes, multiple
selection/current/anchor, ID replacement with an open reader, insertion,
removal/filter/lock and existing failure/commit/context-menu paths. Native Inbox
fixture screenshot was inspected. Baseline `1ee870f` plus worktree; no deployment.
Large mailbox construction still creates row widgets and needs profiling/delegate
work; physical mouse/multi-DPI and broader assurance acceptance remain open.


Mail reader context and privacy slice (2026-10-05): unrelated Mail/status updates
preserve body selection, scroll and unchanged attachment widgets. Attachment
retrieval changes update controls; switching messages resets selection/scroll.
Subjects and bodies render as plain text. Lock or a missing message clears
private labels and attachment caches; scoped Security Details closes on loss of
access, while unrelated changes keep it open. Details are an opening-time
inspection, not live evidence. Outgoing direction survives Archive/Trash.
Replaced Mail row widgets are hidden immediately before Qt deferred deletion,
preventing transient overlapping text noticed in native fixture capture.

Remaining acceptance: large mailbox construction/delegate profiling, completed
frame/scroll timings, physical mouse and multiple DPI settings. Runtime rotation
must retain an honest record of timing failures; a passing rerun does not erase
the initial failed run. No user desktop restart or network deployment occurred.

Validation: initial isolated reader build passed (`build-mail-reader.txt`).
Native Windows focused suite: 5 passed/0 failed including setup/cleanup
(`qt-mail-reader-windows.txt`). Full Qt run: 59 passed/1 failed
(`qt-mail-reader-full.txt`); the failure was the rotation completion deadline
in `rotationKeepsLiveSessionWorking`, outside reader presentation. Its isolated
rerun passed, 3/0 including setup/cleanup (`qt-mail-reader-rotation-recheck.txt`).
The initial failure remains recorded; suite-wide clean acceptance is not claimed.
Native reader fixture was inspected and revealed deferred row-widget overlap,
subsequently corrected with an immediate hide and regression assertion.

Final overlap fix validation: isolated GUI/test build passed
(`build-mail-reader-final.txt`); Windows reader/list/command focused suite
5 passed/0 failed, including setup/cleanup (`qt-mail-reader-final-windows.txt`).
The list regression asserts that a replaced row is already hidden before Qt
processes deferred deletion. Source hashes are recorded in source-manifest.json;
concurrent repository commits are represented by its current HEAD plus worktree.
