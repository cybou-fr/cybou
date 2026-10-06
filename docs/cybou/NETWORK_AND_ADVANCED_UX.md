# Network and Advanced product contract

Status: Level 5 product target, reviewed against desktop/core source on
2026-10-05. New surfaces described here are not implemented merely because
this contract exists. Delivery order and source evidence are in
[`DESKTOP_UX_DELIVERY_PLAN.md`](DESKTOP_UX_DELIVERY_PLAN.md).

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

The initial map is a schematic France silhouette with visually balanced,
stable placement of locally observed peers. Label it clearly: positions are
illustrative, not measured locations. Current Geo admission supplies country
classification, not city coordinates. Public France admission does not locate
LAN/loopback peers; show local/unknown observations separately, without
inventing a French city or geographic guarantee about stored data.

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

## Own-content inspector and console

The redundant standalone diagnostics text window is removed. Network Advanced
remains the technical summary; the Identity menu and Advanced open the same
read-only console implementation. Command names are stable ASCII tokens;
descriptions and results follow the application language. Help is generated from
the same command registry used for dispatch and authorization.

| Access | Implemented commands | Source and scope |
| --- | --- | --- |
| Any local session | `help`, `status`, `network`, `storage`, `peers`, `operations`, `clear` | This node's semantic status and bounded diagnostics snapshot; peer heights are unverified announcements |
| Unlocked Identity | `identity`, `wallet`, `files [filter]`, `file <id|name>`, `chunks <id|name>`, `jobs` | Own semantic catalog, account values and active application tasks |
| Unlocked genesis-key-proven Authority | `authority [status|candidates|totals]` | Local signer loop, volatile locally executed candidates and locally verified finalized totals |

Authority commands are hidden from ordinary help and denied by dispatch. A name,
peer endpoint or displayed role does not authorize them. Every invocation checks
current model proof; loss of proof, account change or locking clears console
output, input and history. Commands do not activate signing, networking proofs,
repair or retrieval. Shell execution, SQL, scripts and mutations remain absent.

Output retains at most 500 text blocks, lists at most 100 rows, command history
at most 100 entries and input at most 1024 characters. Nothing is persisted.
Unknown capacity/replica/finality observations stay unknown; logical file bytes
are not physical storage use. File size divided by 512 KiB is a billing-unit
estimate, never a measured chunk count. `chunks` reports the owned content root
and explicitly states that actual leaf lists and verification results are not
available through the current model. No fabricated digest, verified leaf or
self-capsule/finality claim is displayed.

### Further UI-to-console work

The implemented commands cover technical values already available in Network,
Files Advanced, Wallet and Authority. Keep normal Mail/Files/Wallet workflows in
their pages. Candidates for a subsequent advanced console slice are:

- Real own-content leaf manifests and integrity/retrieval evidence, after a
  bounded cancellable ApplicationService API is available. Never enumerate the
  common ChunkStore or foreign provider objects.
- Paginated verified block/operation lookup, with history access on a worker and
  explicit unavailable results. Current `operations` is a local tracked sample,
  not a full blockchain explorer.
- Authority queue ages, signer safety evidence summaries and settlement previews
  when real APIs expose them. Off-chain storage observations cannot be presented
  as canonical audit reliability or provider failure-domain independence.

Pause/resume/finalize and settlement mutations currently stay in the Authority
page with its established permission and review flow. Adding console mutations
requires a separate product contract and the same authority checks and durable
results; a typed command must not bypass that flow. These future features are
not advertised by help until implemented.

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
