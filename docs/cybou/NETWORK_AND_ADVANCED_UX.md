# Network and Advanced product contract

Status: Level 5 product target, reviewed against desktop/core source on
2026-10-08. New surfaces described here are not implemented merely because
this contract exists. Delivery order and source evidence are in
[`DESKTOP_BETA_ACCEPTANCE_PLAN.md`](DESKTOP_BETA_ACCEPTANCE_PLAN.md) and
[`NETWORK_OBSERVABILITY_PLAN.md`](NETWORK_OBSERVABILITY_PLAN.md).

## Product intent and trust boundaries

One Identity connects familiar Mail, Files and Wallet workflows. Network is
an optional confidence and observation surface. Advanced explains evidence
without requiring ordinary users to understand chunks or consensus.

Every participant is the same Full Node. Bootstrap is a known locator;
optional local signing authority comes solely from the genesis-authorized
PoA private key. A peer session, IP, name, StorageId or map icon conveys no
consensus authority. Only verified PoA-finalized blocks change canonical state.
Use existing ApplicationService, PublicationService and StorageService
boundaries. No new canonical task queue, network role or format version is
introduced by these product features.

## Network overview

Advanced Storage shows encrypted stored lengths / V, utilization and policy
headroom separately from OS available disk space. Provider obligations are shown
against their budget. Console `capacity` supplies exact-byte detail and sample
UTC while locked or unlocked. Missing disk readings are Unknown; measured zero
remains zero. Headroom saturates at zero when use exceeds the configured limit.
Filesystem overhead is excluded from stored lengths. Shared disk availability
precedes admission reserve and is not promised admission or network capacity.

The local CPU tile shows normalized process CPU over the latest sampled
interval, with OS online logical processors, actual duration and last completed
mean's duration/count/age. Console `metrics` shows the same data. Initial or
failed samples remain Unknown. Means cover at least 60 seconds and weight by
elapsed time; they are not exact rolling-minute windows. GUI work is included;
host load, affinity/quota utilization and global capacity are not measured.

The local process-memory tile is an instantaneous OS working set / RSS gauge
for the entire CYBOU executable, including GUI and shared resident pages.
It uses the diagnostics sample time, displays Unknown when absent, and has
matching exact-byte output in Console `metrics`. It is not host memory, average
load or a network total. Resource history and remote resource reports remain planned.

The page is named Network / Réseau in navigation and headers. Built-in passive
monitoring is standard CYBOU functionality: actual data served, finalized op/min,
transfer speed, resource load, storage capacity and scoped potential headroom,
with time-series charts and matching Console detail. The observability plan
defines sources, consolidation and remaining implementation. Historical load
benchmarks are separate ceiling evidence and do not replace current telemetry.
No external monitoring product or synthetic load is required for collection.

Ordinary users see connectivity, local verified height, content protection
summary, local storage capacity/usage and finalized service budget. Distinguish
logical owned file usage, physical local ChunkStore usage and provider budget
`floor(2V/3)`; physical storage contains encrypted own and foreign content.
Do not enumerate foreign objects to calculate a user-facing file browser.

State every metric's source, scope and update time. Connected peers means this
node's observed sessions, not total network nodes. Advertised peer height is
untrusted peer information. Sync completion is a liveness hint, not proof of
global freshness. Under DEC-286 an old latest block can reflect an idle network;
block age alone does not prove an outage. Unknown measurements are shown as
unknown rather than zero, failed or healthy.

Refresh obtains a bounded diagnostics snapshot. Auto-refresh coalesces
updates, pauses expensive presentation while hidden and preserves interaction.
Manual Refresh displays progress and Last updated; it does not launch a full
audit, repair, history rescan or benchmark without an explicit separate action.

## Unlock preparation

After unlocking, the shell shows a modal CYBOU preparation view while the private
Application DB opens and the initial semantic Mail/Files projection is prepared.
Availability of signing/decryption keys alone is not UI readiness. Known local
history scan counts drive the progress bar; DB opening uses indeterminate progress.
A usable content projection or a completed genuinely empty scan dismisses the view,
including offline. Missing roots and preparation failures remain explicit; users
can choose the local view while retrieval retries, or return to unlock. Late
session replies cannot reopen the view after locking. Public peer synchronization
is not a freshness proof or a condition for Identity creation.

## France map

The map occupies the complete Network page below the global header. Peer details
and the scrollable Advanced drawer overlay it; opening Advanced does not shrink
the map or grow the page. Network presentation coalesces status signals and pauses
while hidden. Diagnostic rows are retained and only their values are updated.

A compact historical benchmark reference occupies the lower-right map margin.
It shows finalized op/min, the UTC run date, workload/count and same-host
simulation scope where applicable. Details opens the accepted evidence in
Advanced Overview. Missing, rejected or wrong-network evidence shows Unknown.
The fitted France/Corsica silhouette uses spare horizontal space so the reference
does not cover Corsica, retaining its size/aspect and placement when Advanced
opens. This is a historical workload result, never a live throughput counter.

Advanced has four bounded scrollable sections: Overview (local finality/mesh
summary and reference details), Peers (observed sessions and the selected-peer
card), Storage (local capacity and own-content protection), and Technical
(existing diagnostics, monitor and console entry points). Selecting a peer opens
Peers while Advanced is active; closing returns the same selected card to the
map. Reference and Technical entry points select their corresponding sections.
Unknown capacity is not rendered as a measured zero. Mesh sessions and storage
relationship proofs remain separate observations; no provider census or storage
reliability is inferred from the peer table.

The initial map is a schematic France silhouette with visually balanced,
stable placement of locally observed peers. Label it clearly: positions are
illustrative, not measured locations. Current Geo admission supplies country
classification, not city coordinates. Public France admission does not locate
LAN/loopback peers; show local/unknown observations separately, without
inventing a French city or geographic guarantee about stored data.

The compiled silhouette uses Natural Earth mainland France and Corsica, with
aspect ratio preserved and no map service or regional boundaries. Its source hash
and offline generation tool accompany the asset. Public markers use P labels;
local markers use L labels in a compact inset. These labels identify only this
view's observations, never protocol node identities or storage-provider counts.

A selected peer has one detail card: floating on the map while Advanced is
closed, inside the drawer while Advanced is open. Selection survives switching.
Normal details show connection, admission, explicitly illustrative positioning
and the unverified advertised height. Endpoint, StorageId, tip delta and transport
detail appear in Advanced. Disconnected observations never present stale height
or storage proof as current evidence.

Use bounded session pseudonyms rather than full public endpoints on the normal
map. Do not create or announce a new protocol NodeID, role or Identity mapping.
Proven StorageIds are relationship-specific evidence, not a general map key;
do not initiate storage proofs merely to populate a map. Protect endpoints,
observation history and exports according to the processing inventory.

Optional display limits 100/200/300 apply only to an observed sample when enough
observations actually exist. Current outbound connection limits are small;
never invent missing nodes to fill the map. A future wider observability feed
requires a separate source/privacy/retention design and cannot be inferred
from the local diagnostics snapshot.

Locally measured availability means reachable observations divided by planned
observations over a declared window, with sample count, gaps and collection
scope. It is neither global uptime nor canonical storage reliability. If no
measurements exist, omit the ranking. Define endpoint churn, restarts and
missing samples before adding a score; do not merge identities by guesswork.
Ranking/top limits affect presentation only. Paid placement stays randomized
over eligible providers under the frozen storage economy; users cannot select
providers and high uptime never becomes a placement weight through this UI.

## Central Authority and explorer

Extend the existing Authority page rather than replacing it. Its current
local finalizer controls and one-period settlement action retain their real
permissions and safety checks. Ordinary users may view public finalized
information without a signing key. Signer/settlement commands require the
actual locally authorized PoA key, not a displayed account name or endpoint.

Add a read-only explorer for locally verified blocks, operations, AccountState,
publication roots, leases, escrow and settlements where actual APIs expose
them. Show canonical facts separately from local candidate-pool entries and
off-chain storage observations. Paginate/index history; avoid full scans on
the GUI thread. No plaintext filenames, message contents, recipient ownership
guesses, content keys or arbitrary provider data appear in a public explorer.

Operator diagnostics may show queue age, worker timing, relay/admission failures,
storage aggregate utilization, evidence preparation and settlement readiness.
Public block data does not establish provider placements, global audit quality
or physical independence. Implement evidence aggregation before claiming a
complete payout/audit dashboard. Scope sensitive operational exports and
redact them; viewing a panel does not authorize publishing it.

### Authority operator interaction (2026-10-08)

The proven Authority entry has its own Administration section. Unproven sessions
see neither the section nor its navigation entry. Compact navigation retains the
separator and icon. Controls require an unlocked authorized Identity and an active
signer; a safety halt disables them. Locking the user Vault leaves an already
active PoA production loop running. Authority explains this before locking, and
the unlock screen displays a local background-finalizer observation independently
of the locked Identity's proof. This observation grants no control permissions or
network role. Pausing uses a Cancel-default consequences dialog; resume is direct.

Storage settlement first prepares actual entries off the GUI thread. A Cancel-default
review shows the UTC period, unique payout AccountIDs, exact entry count/amount,
observed escrow and scoped local off-chain evidence. Details list the first 100
entries and disclose truncation. No entries means no eligible locally prepared
payouts, not proof of zero service; finalization closes the period. Lock, account
change, proof loss, safety halt, changed period or changed escrow invalidates the
review. Confirmation submits the exact reviewed vector on a worker after checking
the unlocked key and runtime cursor again. Pending preparation is single-flight;
closing a session invalidates stale callbacks. Submission remains Submitted until
PoA finality. No new keys, signer histories, protocol operations or evidence claims
are introduced by this UI.

Signing safety displays the available local observation, or Unknown; it never
asserts that a journal integrity audit occurred. Settlement readiness declares its
local evidence scope. The verified explorer excludes nonfinalized diagnostic entries,
while the candidate section remains explicitly volatile. Peer deltas are unverified
announcements and never prove synchronization or authority.

## Own-content inspector and console

The redundant standalone diagnostics text window is removed. Network Advanced
remains the technical summary; the Identity menu and Advanced open the same
read-only console implementation. Command names are stable ASCII tokens;
descriptions and results follow the application language. Help is generated from
the same command registry used for dispatch and authorization.

| Access | Implemented commands | Source and scope |
| --- | --- | --- |
| Any local session | `help`, `status`, `health`, `metrics`, `network`, `storage`, `peers`, `operations`, `block <height|hash>`, `op <id>`, `history [page]`, `clear` | This node's semantic status and bounded diagnostics snapshot; peer heights are unverified announcements; locally verified blocks, operations and blockchain history |
| Unlocked Identity | `identity`, `wallet`, `files [filter]`, `file <id|name>`, `chunks <id|name>`, `jobs` | Own semantic catalog, account values, active application tasks, real own-content leaf manifests and BLAKE3-256 integrity verification |
| Unlocked genesis-key-proven Authority | `authority [status|candidates|totals|settle]` | Local signer loop, candidate queue age and waiting times, safety journal status and settlement preview |

Authority commands are hidden from ordinary help and denied by dispatch. A name,
peer endpoint or displayed role does not authorize them. Every invocation checks
current model proof; loss of proof, account change or locking clears console
output, input and history. Ordinary commands do not activate signing, networking proofs,
repair or retrieval. Signing and settlement submission remain on the Central Authority page; console `settle` is a preview only.
Shell execution, SQL, scripts and arbitrary mutations remain absent.

Output retains at most 500 text blocks, lists at most 100 rows, command history
at most 100 entries and input at most 1024 characters. Only window geometry is persisted; commands, output, search and completion data remain in memory.
Unknown capacity/replica/finality observations stay unknown; logical file bytes
are not physical storage use. File size divided by 512 KiB is a billing-unit
estimate, never a measured chunk count. `chunks` inspects the real own-content chunk tree
and BLAKE3-256 integrity verification results from local storage when available, or explicitly
reports when chunk evidence is not exposed. No fabricated digest, verified leaf or
self-capsule/finality claim is displayed. Foreign provider chunks and common ChunkStore
objects are never enumerated.

### Implemented technical console capabilities

`health` reports a real local UTC sample time, monotonic runtime uptime,
initialization/safety observation and candidate-pool operations/bytes. Network
Advanced Overview uses the same sample. Recent finalized/rejected operation
history is not counted as candidate load. Missing sample or uninitialized pool
shows Unknown. This is local load, not global health or operation throughput.

`metrics` reports local frame-stream received/sent totals and rates over the
preceding 60 complete seconds, with observation time. Advanced Overview shows
the same rates. TLS/TCP overhead is excluded; service/retry/partial bytes are
included. This is not unique content or finalized transaction throughput.
Rates stay Unknown until the complete window exists. Runtime restart resets
the counters; storage-pool and ordinary mesh sessions share the collector.

`metrics` also reports verified operation observations over 1/5/15 complete
minutes: observed op/min (local production plus direct announcements), local
production contribution and separate historical imports. The overview uses the
one-minute observed value. Repeated/rejected blocks do not count; canonical
replacement resets operation observations. No block creation timestamp exists,
so announcement arrival time does not establish present network production or
global freshness. Incomplete windows show Unknown and complete local idle windows
may be zero. This does not establish a performance ceiling.

Overview also shows traffic and operation observation plots from retained runtime
history, up to 15 minutes / 180 completed five-second intervals. Rates are per
five-second interval (B/s and op/min); the one-minute headline remains separate.
History imports are excluded from the operation plot; local production is its
secondary series. Unknown startup intervals are absent. Restart/reset discards
the corresponding history. Mouse and Left/Right/Home/End select exact values;
tooltip and accessible description provide textual detail. Solid/dashed lines
and theme tokens distinguish series. Presentation pauses while hidden, while
the core collector keeps history. No physical/screen-reader acceptance is
implied by these component interaction features.

The technical console supports:
- Real own-content leaf manifests and BLAKE3-256 integrity/retrieval evidence via `chunks <id|name>` without enumerating common ChunkStore or foreign provider objects.
- Paginated verified block, operation and history lookup via `block <height|hash>`, `op <id>`, and `history [page]`.
- Central Authority diagnostics and settlement preview via `authority [status|candidates|totals|settle]`, gated by the unlocked genesis-key-proven session. Controls remain on the Authority page.

### Monitor and console interaction (2026-10-08)

Network Monitor is a separate nonmodal window, reused when opened again from
Advanced. Peers, Operations and Own content have separate tabs, each capped at
256 rows. Updates coalesce over 150 ms and retain selection by object key and
scroll position. Pause freezes only displayed observations; node activity
continues. Locking or changing Identity clears own-content rows immediately even
while paused. Advertised peer deltas remain explicitly unverified. Only window
geometry is saved, with no diagnostic snapshot persistence.

Console has a compact live scope header, help/search/copy-selection/clear toolbar,
structured registry-derived help, and bounded command and argument suggestions.
File arguments come only from the unlocked own catalog, operation IDs from the
local snapshot, and the block suggestion from the locally verified tip. Authority
suggestions require current genesis-key proof. Completion inserts text without
executing it. Tab and Ctrl+Space show suggestions; Tab accepts a visible suggestion.
Ctrl+F searches output with wrapped previous/next navigation and match counts;
Ctrl+L clears output and history; Escape dismisses search/completion. Lock, account
change or lost Authority proof also clears search and completion data. Commands
retain their existing read-only semantics. Geometry is the only persisted value.

## Tests and benchmarks

Production diagnostics initially offer passive measurements, bounded own-file
read/verify checks and local health summaries. Never report a passive sample as
network-wide throughput or a full security audit. Active checks declare target,
bytes/cost, resource limits and cancellation/cleanup behavior first.

Loadgen and storage smoke/soak remain development tools behind BUILD_TESTS.
An optional DEVNET test-build panel can consume their results after a reviewed
resource/safety design; production cybou does not gain a load generator merely
to fill a dashboard. No automatic benchmark on page opening, no additional PoA
signer and no reset/secret export. Real live testing is a separate planned run.

The current loadgen measurement contract has no schema version. It distinguishes
`attempted_operations` (service calls, including busy refusals and staged intents),
`submitted_operations` (unique OperationIDs handed to the runtime, including
uncertain delivery), and `finalized_operations` (those same IDs observed in locally
verified PoA-finalized state). Existing historical jobs never enter the measured
cohort. Repeated observations/retries do not increase either operation count.
Wallet operations that complete during drain count once; pending wallet operations
prevent a successful drain verdict.

`measurement_window_s` covers the client's load and final drain, after Identity
preparation/funding and before recovery/teardown; `load_window_s` also records the
generation phase. Both `submitted_ops_per_s` and `finalized_ops_per_s` use that
explicit measured window. The battle aggregate uses its controller's monotonic
window from first load-client launch to last metrics receipt, including reconnect,
load and drain. It does not divide total operations by the longest individual
client window. Rates describe that cohort/workload, not the network capacity.
Missing latency samples render Unknown with n=0 in the report.

Network Advanced can display a compiled benchmark reference separately from live
peer observations. It requires PASS, successful nonempty acceptance checks,
consistent counts/rate, nonzero finalized operations, binary/revision provenance,
and the current node's matching NetworkBinding. No valid reference means Unknown.
The compiled resource is an evidence artifact, not network/genesis configuration;
opening Network never runs a test. `tools/battle/battle_test.py` writes
`benchmark.json` with counts, timing scope, profile, binding, build provenance and
acceptance checks; only a reviewed successful result may replace
`docs/cybou/battle/benchmark_reference.json`.

An operator-authorized Windows/WSL simulation records `co_located_wsl: true` in
the reference and displays that scope explicitly. WSL can exercise a second
network address through ordinary private-LAN admission, but shares the Windows
physical host. Two successful placement addresses in this exercise are not
evidence of independent remote machines, operators or failure domains. No public
proxy or admission bypass is required for this simulation.

The desktop displays finalized operations per minute (`op/min`), rounded to one
decimal from the unrounded per-second reference rate multiplied by 60. The
measurement artifact retains its per-second fields and exact measurement window.

## Data confidence, CIA and deletion

Present an understandable evidence view: confidentiality profile verified for
this content; integrity last verified during retrieval/audit; availability
observed at a stated time; remote copy count and measured diversity; recovery
tested or untested. Unknown, stale and failed evidence remain distinct.

Encryption or French admission alone does not prove RGPD conformity, storage
residency, availability SLA, certification or universal deletion. Compliance
readiness links to processing roles, retention, rights procedures and scoped
evidence in SECURITY_GOVERNANCE and its supporting registers.

Deletion explains local Trash, finalized removal/revocation, lease closure and
compliant provider purge separately. Shared references can retain chunks;
purge failures remain pending with retry and physical byte accounting. Historical
capsules, recipient copies and backups remain outside universal erasure claims.
Where there is no per-object purge acknowledgement API, show that absence;
do not fabricate a green deletion receipt. Stronger evidence requires its own
privacy/accounting design before implementation.

## Acceptance

The map has a readable list alternative, keyboard-accessible details and no
information conveyed by color alone. Task completion is usable without opening
Network or a console. Diagnostics remain responsive during transfer and quiet
network periods. No secrets or foreign content leak through logs, map, inspector
or export. Unknown evidence never becomes success. Tests cover absent/stale
measurements and authorization changes, including lock while a job is running.
