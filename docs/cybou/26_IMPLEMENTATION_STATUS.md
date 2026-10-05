# Implementation status

Status: code/evidence reviewed on 2026-10-04 with dated 2026-10-05 runtime and
desktop updates below; deployment statements retain their stated scope.

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
