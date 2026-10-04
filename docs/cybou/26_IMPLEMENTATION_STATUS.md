# Implementation status

Status: code/evidence reviewed on 2026-10-04; deployment statements retain their stated scope.
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
| Bootstrap | Ordinary CYBOU full peer | Bootstrap binding/protocol/store, `BOOTSTRAP_REQUEST/RESPONSE` and the `cybou-bootstrap` executable are removed. Nodes and the desktop dial the compiled locator first and check its SPKI pin; the locator node serves `--tls-certificate/--tls-key`. Every process uses the same Full Node network lifecycle; signer activation does not reconnect peers. Pinned rendezvous endpoints are protected from discovered-peer crowd-out. The DEV VPS runs the ordinary `cybou node run` command on current current DEVNET. |
| Geo updater | France-only admission with a valid local dataset; fail closed otherwise | HTTPS and published archive SHA-1 verification, bounded gzip decode, SHA-256 CSV cache and validated atomic installation. Each failed attempt re-fetches metadata and archive; five attempts use interruptible 0/2/5/15/60s delays. Exhaustion retains valid cache and retries in 1h, or retries in 5m without a valid dataset. Successful/current checks use 14 days. Test-only fetch/wait hooks cover publication mismatch, rejected candidates, cache retention, strict temporary cleanup and cancellation without network access. |
| Consensus bootstrap state | No grants, roster, or network-role announcements | Removed. |
| Consensus state | Unified current state format | State with canonical AUTH, Treasury monetary base, onboarding-origin System Balance, settlement cursor and storage leases (M5); no OnboardingPool; older decoders removed. The compiled DEVNET no longer decodes and fails closed until M7. |
| AUTH state | Canonical non-transferable AUTH in AccountState and GenesisAllocation; state root commits it | Implemented (`393657f`): `AccountState.authority`, `GenesisAllocation.authority`, state root commits it. Legacy AuthorityIndex/AuthorityPolicy removed. Desktop reads AUTH from AccountState (`52c5406`). |
| AUTH transitions | Current utility-bound earning; PoA-signed `PoaAuthAdjustment` GRANT / BURN | Implemented: utility operations RootPublication and SystemLock earn flat +1, capped at +1 per account per finalized block (`59ada0b`); AccountCreate, maintenance, payments and adjustments earn nothing. `PoaAuthAdjustment` (`4a393fd`) is signed by the genesis PoA key, bound to one block height, once per block, verified by every node; never relayed. No CLI/GUI action yet issues adjustments. |
| Relay proof-of-work | Every user operation carries tier PoW checked by every Full Node and the PoA (DEC-273) | Implemented: `operation_work.h` (`CYBOU/OP-WORK`), `OperationPool::Admit` and revalidation check it against finalized state; `OP_META` carries `size u32 + nonce u64`; `CybouNodeRuntime::PrepareOperationWork` solves outside the runtime lock and caches; CLI `operation submit` solves before sending. Never stored in blocks. Component tests set `NodeRuntimeConfig.operation_work_bits = 0`. Wire change: every DEVNET node, the VPS included, must run this build. |
| Candidate execution | Every full node executes candidates before relay; one pool per node, used by PoA for blocks | Implemented (`7f447d4`): `CybouNodeRuntime` owns the single `OperationPool`; local, peer and relayed operations pass `OperationPool::Admit` before staging or forwarding; every node revalidates after each finalized block. CybouNodeRuntime produces from that pool; PoaFinalizer only signs with its durable journal. |
| Validation | `ValidationAttestation` after local execution by an Identity with finalized AUTH > 10,000,000 | Implemented (`b01f2dc`): threshold raised to 10M AUTH (`VALIDATION_AUTHORITY_THRESHOLD{10'000'000}`), codec, eligibility and vault-backed signer (`c9422a8`), bounded RAM `ValidationPool` and `OperationStatus::IsValidated()` (`f7f894f`), CYBOU P2P frame `VALIDATION_ATTESTATION_POLL` / `VALIDATION_ATTESTATION` (`037e6b0`), desktop Validated state and signer (`885986f`). |
| Notarial Storage & Resource Ladder | Consensus-enforced AUTH tier limits (DEC-272), 5 GiB onboarding credit, 1:3 reciprocal storage ratio | Implemented: `protocol_limits.h` holds the finite tier table (`ComputeAuthorityTierLimits`: operations per block/epoch, quota and largest publication in 512 KiB chunks, relay-PoW bits). `ExecuteBlockOperations` meters every Identity-authorized operation against the parent finalized AUTH and refuses `OPERATION_LIMIT_EXCEEDED`, `PUBLICATION_TOO_LARGE` and `STORAGE_QUOTA_EXCEEDED`. `CybouState.usage` and `CybouState.publications` are committed by the state root in a section omitted while empty (genesis root unchanged). Wallet shows the live window, quota, largest file and PoW. Current DEVNET had no finalized block, so the rules apply from height 1 without an activation height. |
| Mutual Proof of Storage | Randomized challenge-response byte-offset and nonce auditing | Cryptographic primitive implemented (`b01f2dc`): `src/cybou/storage_audit.h` and `.cpp` provide `StorageAuditChallenge`, `CreateStorageAuditProof`, and `VerifyStorageAuditProof` using BLAKE3 chunk sampling (test coverage in `cybou_finalized_chunk_store_tests`). Network mutual-audit protocol, PoA notarization, and reliability coefficient in state: NOT IMPLEMENTED. |
| Object Pruning | Author `RevokePublication` retires the record and providers purge chunks (DEC-271) | Implemented: `RevokePublication` (ProtocolOperationKind 9, IdentityOperationKind 6) is owner-only, costs the payment fee, earns no AUTH and frees quota; a revoked publication fails `FindFinalizedRootPublication`, so no new admission; `FinalizedChunkStore::PurgePublication` runs on each finalized revocation and deletes chunks no other publication authorizes. Desktop Mail/Files revoke automatically: once the application index is complete and every own job is finalized, `PublicationService::RevokeUnreferenced` revokes one own publication at a time (never recovery bridges, keeping 5 operations of the window for the user) when no catalog record came from it, no non-deleted message is it and no live file or attachment tree lives in its leaves; indexing skips revoked publications. "Delete forever" and "Empty Trash" can trigger revocation and managed purge once their prerequisites are satisfied; they do not erase historical capsules, retained keys, recipient copies or prove physical deletion. The fix committed in `a3f05aa` journals pending purge, retains quota on removal failure and retries on reopen and maintenance; startup reconciliation covers revocations whose local purge event was missed. |
| Executables | One production `cybou` | `cybou` is the desktop and the headless CLI (`src/cybou/cli/cybou_cli.cpp`, one parser, no internal positional commands); `BUILD_GUI=OFF` builds a headless-only `cybou`. `cybou-node` and the `network bootstrap` command are removed; `network info` lists the locators. `cybou-loadgen` builds only with `BUILD_TESTS`. |
| Operation routing | Uniform CYBOU P2P Full Node mesh | HELLO has no capability field. No PoA transport proof or special route. Sync completion is advisory. Storage is intrinsic; StorageId is challenged only for storage interaction. |
| PoA | Sole independent canonical finalizer | Single-operator PoA re-executes every candidate through the node pool and finalizes with zero attestations; a multi-node CYBOU P2P test covers validator, ordinary node and PoA. |

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
Application DB, `STORAGE_AUDIT_CHALLENGE`/`RESPONSE` (27/28) carry random-offset
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
(operation kind 10) and PoA-signed `StorageSettlement` (kind 11, contiguous
86,400 s periods, per-lease period cap, no self-payout, origin-preserving
payouts and refunds). AUTH storage quotas are removed; `MAX_PUBLICATION_CHUNKS`
remains a safety bound. Providers admit chunks only under an active lease;
PublicationService leases at publication and renews inactive leases. Not yet
implemented: aggregation of off-chain evidence into settlements, payout
bindings, lease renewal UX. Because the state and genesis formats changed, the
previous compiled DEVNET was retired. M7 steps 1–3 are done: main compiles the
new storage-economy DEVNET (NetworkBinding `6d202ccf…2d97`), `verify-devnet`
passes and the compiled-DEVNET tests are restored. The DEV VPS and desktops
still run the retired network until their cutover.

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
  is bounded and held in memory only. PoA settlement is not implemented.
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
`846e8f223054d5d1944c4e01a576ecbbbc5ef497654ae2fded57368ac6a4b311`.
Signed genesis anchor:
`5bd33c6c65462345bd5b40c297ecdfcf718029bc94cf743175bd3e73f9cea9f2`.
Genesis allocations: `cybou` 100,000,000 CYBOU and 10,000,001 AUTH (same
phrase and PoA key as the retired DEVNET; AccountID is created on initial
onboarding into the current network); `bootstrap` 0 CYBOU and
10,000,001 AUTH. Retired material: `private/devnet-retired-auth1m-20261003/`.
The pre-generated material is under gitignored `private/devnet/`. Production
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
