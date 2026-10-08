# Desktop Beta Acceptance & Performance Evidence

Status: active Level 4 delivery plan, reviewed 2026-10-08 against
`1f5417fb6d6c4d418b474b856a23bdb21486c2d6`. This supersedes the remaining-work
order of `DESKTOP_UX_REMAINING_WORK_2026-10-06.md` and the initial W0–W8 order
in `DESKTOP_UX_DELIVERY_PLAN.md`; their dated findings and delivery records remain
historical evidence. AGENTS, frozen architecture and normative/product contracts
retain their authority. This plan does not certify Beta readiness.

## Assessment and corrections to the supplied review

Desktop feature delivery is substantially implemented; release confidence now
depends on acceptance evidence. No percentage of readiness is assigned.

| Surface | Current implementation/evidence | Remaining decision |
|---|---|---|
| Console / Network Monitor | Bounded read-only commands, completion/search/history/lock cleanup; retained bounded Monitor tabs and view pause | Freeze feature set; physical/accessibility and live acceptance |
| Authority Administration | Local genesis-key proof, pause review, exact settlement preview and stale-review cancellation | Live operator acceptance; no redesign |
| Mail / Files actions | Correlated durable local acknowledgments, draft safety, shared tasks, ordered Undo and reusable Protected references | Outage/restart and physical entry-point acceptance |
| Files observations | Persisted replica counts distinguished from volatile local observation time/scope/reasons | Live audit/repair and independent-domain evidence |
| Mail performance | 10k synthetic native Windows delegate profile improved construction 12,171→144 ms and completed render 17,304→219 ms | Component optimization delivered; live catch-up/physical interaction remain open |
| Files performance | 10k synthetic construction 527 ms; single-item updates median 43 / max 49 ms | No new virtualization without measurements; snapshot/scroll/completed-frame acceptance remains open |
| Keyboard | FR/EN, both themes, scoped shortcuts, menus/dialogs, focus and recovery/Console component routes | Physical keys, mixed DPI and assistive technology |
| Benchmark reference | Historical 7-operation same-host simulation, displayed as 4.7 finalized op/min | Not capacity or independent durability; multi-host capacity is P1 |

Evidence is detailed in [implementation status](26_IMPLEMENTATION_STATUS.md).
Files already uses a viewport status delegate; the next step is not an assumed
model/view rewrite. Test counts are per suite/revision and are not added into a
single cumulative acceptance total. Two remote obligations alone do not prove
successful full replicas. Distinct addresses do not prove independent hosts,
operators or sites. Independence must be established before the distributed
acceptance runs, not after them.

## P0 — Reproducible revision and CI integrity

Verified GitHub Actions snapshot for the reviewed source revision, observed
`2026-10-08T12:22:57.6841594Z`:

- [CYBOU core run 37774708866](https://github.com/cybou-fr/cybou/actions/runs/37774708866):
  completed/failure at `Build CYBOU core test, node and smoke binaries`.
  Configuration succeeded; protocol tests and official DEVNET CLI checks were
  skipped. This is a build failure, not a demonstrated protocol-test failure.
- [CYBOU desktop run 37774708869](https://github.com/cybou-fr/cybou/actions/runs/37774708869):
  in progress at review; no green conclusion recorded.
- Public job annotations show exit code 1 but not the compiler cause. Obtain
  the failing build log and reproduce the actual configuration before fixing.
  Do not attribute the failure to the latest Qt package or a refactor without
  diagnostic evidence.
- Local evidence at this source: 92 full offscreen Qt results and 6 focused
  native Windows results passed. Earlier core passes are scoped to earlier
  packages; they do not override the current remote build failure.

Next: diagnose/fix the core build, finish/check desktop CI, and record green
results against the exact repaired revision. Include required core/CLI checks,
full Qt and benchmark/report Python checks. Record configuration, toolchain,
binary hashes and logs. A new revision needs its own relevant evidence; a rerun
must retain the original failure and explain resolution. Defer large new UI
refactors until this gate passes; targeted build/acceptance defect repairs remain
allowed. No CI repair is claimed by this documentation update.

### Core build diagnosis follow-up (2026-10-08)

The authenticated GitHub job log is now available. Job `113302547455` fails in
`secret_file.cpp::ReadSecretFile`: POSIX signed `st_size` compared with unsigned
`size_t max_bytes` produces `-Werror=sign-compare` on GCC 13.3. Local Linux strict
syntax checking reproduces the same error. The correction uses C++20
`std::cmp_greater`, preserving the positive-size check and avoiding narrowing.
Size-limit regression coverage retains exact-limit reads and rejects too-small,
zero and over-policy limits. Private owner/mode and link rejection are unchanged.

The previous unknown-cause statement above is the original review snapshot,
not the current diagnosis. Local validation and toolchain differences are in
`26_IMPLEMENTATION_STATUS.md`. The P0 gate remains open until core and desktop
GitHub Actions validate the published corrected revision; local passes alone do
not close it. The older desktop run remains in vcpkg setup at this follow-up.

## P0 — Acceptance environment and independent remote domains

Prepare clients and providers before Mail/Files durability tests. Record actual
physical hosts, storage identities/payout accounts, network addresses and known
failure-domain boundaries in a private operator inventory. Publish only a
redacted topology/evidence summary. A pair of colocated VPS processes or
Windows/WSL is simulation evidence, regardless of successful placement.

Use ordinary Full Nodes with explicit capacity, existing compiled DEVNET,
France public admission and randomized service-owned placement. Identify the
required two remote full replicas by actual verified storage evidence; clients
do not choose paid providers. Record independence dimensions separately rather
than asserting independent operators/datacenters from two IPs. Document which
failure the topology can withstand and what remains untested.

Keep exactly one active PoA signer and its durable signing history. Do not
export secrets, change genesis/network keys, add a signer, bypass Geo admission
or reset history to make a test pass. Plan disruptive signer/provider outages
with the operator; preparing this plan does not authorize executing them.

Exit: clean client environment and recovery material are available through the
existing private handling process, topology meets the tested durability claim,
and failure/restart procedures preserve accepted material and signing safety.

## P0 — Live Identity, Wallet and operator workflows

Identity: fresh install without fixture variables; create, finalized name,
lock/unlock and close/restart. Restore on a separate clean installation with the
source unavailable. Verify stable AccountID, name/balances and useful accessible
published Mail/Files, not only vault opening. Distinguish local drafts/preferences
from published content and test historical-key recovery where applicable.

Rotation: verify candidate-vault retention, cancellation, lock/restart during
pending finality, exact journal resume and post-finality continuity. The old
phrase must not authorize the current epoch. Record failure paths without
recording words, passwords, private keys or plaintext content.

Wallet: review recipient/amount/fee, concurrent name/publication coordination,
disconnect after submission before acknowledgment, restart while uncertain,
and reconciliation with the same OperationID/exact bytes. Balances and ledger
change only on verified finality; no duplicate operation on restart.

Administration: prove the local authorized key, verify lock/background-finalizer
explanation, pause consequences and Cancel default, resume/safety behavior, and
real prepared settlement review/submission/finality. Never infer authority from
an endpoint/name or absent settlement evidence from an empty display.

## P0 — Live Mail and Files outage/recovery

Mail: two independent client installations; recipient offline during send and
later reconstructing/decrypting verified history. Repeat with attachment,
Save to Files, original Mail deletion and restart; retained Files bytes remain
available. Exercise disconnect during preparation, after exact operation bytes
leave but before acknowledgment, and restart while delivery is uncertain.
Confirm recoverable draft/outgoing state and no duplicate publication.

Files: upload and finalize, confirm required full remote replicas with scoped
verification evidence, then make the source client unavailable. On a clean
restored client rebuild the catalog, fetch encrypted chunks, verify ChunkIDs
(BLAKE3 of exact stored encrypted bytes), decrypt and compare the exact original
plaintext bytes using a local digest. Do not publish plaintext or sensitive hashes
without a reviewed evidence policy.

Remove one provider under the planned failure procedure; observe actual repair,
restore the required confirmed copies and verify retrieval. A deficit is not an
active-repair claim. Record observation scope/age and failures honestly. Verify
Trash/revoke/lease closure, compliant purge and shared-reference/unlink-failure
retry within existing assurance boundaries; no claim of hidden-copy deletion.

Exit: each scenario has a pass/fail record, exact source/binaries, topology and
recovery evidence. Failures become concrete defect work; do not redesign a
working surface solely because a release checkbox remains open.

## P0 — Physical interaction and accessibility acceptance

Windows 11 physical keyboard/mouse and Explorer drag/drop: 100%, 125%, 150%,
200% scaling, one monitor and mixed-DPI monitor transitions. Cover FR/EN and
both themes, minimum usable window size, real Files→Compose drag, Menu/Shift+F10,
Open/Save dialogs, focus return and disabled/error actions. Qt scale factors,
programmatic key delivery and fixture screenshots are separate evidence.

Use an actual Narrator or NVDA session for sidebar/header, Mail list/reader/
Compose, Files list/actions, Wallet, Identity/recovery, map/list alternative,
Console, task panel and Authority reviews. Check role/name/state/action, reading
order, status changes and confirmation focus; labels alone do not establish
screen-reader usability. Record tested technology/version and uncovered scope.

## P1 — Sustained capacity measurement and evidence-led polish

After correctness and topology gates, use BUILD_TESTS DEVNET tools for stepped
Payments, Publications, Files, Mail and Mixed load. Predeclare cohort, offered
load, duration/drain limits, latency sample scope and p95 budget. Track unique
submitted/finalized IDs, failures, queue drain, resource limits and signer safety.
Saturation/failure points remain results; only stages satisfying declared gates
are accepted references. Distinguish finalization from storage durability.

Record controller/client windows, sample counts, build/binding/topology and
PASS/FAIL boundaries; do not extrapolate a short low-rate simulation into capacity.
Preserve the accepted desktop op/min unit and scope labels. Internally op/s can
remain a measurement unit; a headline unit change needs a separate product
contract decision. No benchmark runs on opening Network, no production loadgen.

Only after a valid dataset consider bounded Advanced charts. Final screenshot/
copy/quiet-state polish follows reproduced clipping or usability findings. Do not
add lifecycle diagrams or redo the design system without evidence.

## Evidence ledger and release exit

Use [Beta acceptance](85_BETA_UI_ACCEPTANCE.md) as the checklist. For each executed
scenario append a durable record with these fields:

| Field | Required evidence |
|---|---|
| ID / scope | Checklist scenario, expected outcome, component/native physical/live |
| Source / binary | Exact revision, clean/dirty status, app/test hashes, configuration |
| Environment | OS/toolchain, language/theme/DPI, assistive technology if used |
| Topology | Redacted physical hosts/failure domains, NetworkBinding, signer continuity |
| Procedure / outcome | Actual sequence, timings, observed result and pass/fail/not run |
| Logs / limitations | Redacted artifact paths/hashes, failure cause or unresolved blocker |

No live/physical acceptance is newly marked passed in this review. Acceptance
requires matching release-revision evidence for applicable gates; implementation,
synthetic coverage, native Qt automation and live acceptance stay distinct.
