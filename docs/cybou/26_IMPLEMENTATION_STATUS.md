# Implementation status

Status: updated through the main simplification pass (2026-10-03).
This page describes
implementation and deployment reality; `AGENTS.md` and the frozen architecture
define the target. The prepared changes described here are not a released baseline.

## Target architecture vs. current code reality

| Component | Target constitution | Current code reality |
|---|---|---|
| Official networks | Compiled DEVNET public constants; MAINNET unprovisioned and GUI-disabled | Implemented (`d5d90cb`): one `OfficialNetwork` per kind via `RequireOfficialNetwork`. Current compiled DEVNET contains the immutable v12 signed genesis under its new NetworkID. Retired DEVNET is not accepted. MAINNET throws as not provisioned; the GUI has no network selector and always runs DEVNET. |
| Network identity | `NetworkID = Network Public Key` | Implemented: verified genesis carries the exact key; every 32-byte field is named `network_binding` and equals `ComputeNetworkBinding(key)`; `GetNetworkId()` exists only for the exact key bytes of a verified genesis. The shared `MakeNodeRuntimeConfig` derives the verified genesis and rendezvous locators from `OfficialNetwork`; the genesis digest marker is removed from the StateStore. |
| Signed genesis | One immutable offline-signed `NetworkGenesis` object and initial state compiled per official network | Implemented: the compiled genesis must carry exactly the compiled Network Public Key and its initial state root. CYG1/CYN1, `CybouNetworkFile` and every file loader are removed. |
| Official startup | Select compiled public constants; verify signature and initial state root; no external official file or separate digest pin | Implemented: every `cybou` command, loadgen, storage smoke/soak and the desktop start only from a compiled network (`--network devnet`, or `lab` in LAB test builds). No profile digest pin; the verified genesis digest anchors height zero. |
| Network Private Key | Strictly offline, signs genesis once; private material stays under gitignored `/private/` | `cybou-provision` is an explicitly built offline tool (`BUILD_PROVISION_TOOL=ON`), separate from production `cybou`; `verify-devnet` checks existing secrets without signing genesis. Existing constants and secrets cannot be overwritten. The retired DEVNET material is kept under `/private/devnet-retired-20261003`. |
| Bootstrap | Ordinary CYBOU full peer | Bootstrap binding/protocol/store, `BOOTSTRAP_REQUEST/RESPONSE` and the `cybou-bootstrap` executable are removed. Nodes and the desktop dial the compiled locator first and check its SPKI pin; the locator node serves `--tls-certificate/--tls-key`. Every process uses the same Full Node network lifecycle; signer activation does not reconnect peers. Pinned rendezvous endpoints are protected from discovered-peer crowd-out. The DEV VPS runs the ordinary `cybou node run` command on current v12 DEVNET. |
| Geo updater | France-only admission with a valid local dataset; fail closed otherwise | HTTPS and published archive SHA-1 verification, bounded gzip decode, SHA-256 CSV cache and validated atomic installation. Each failed attempt re-fetches metadata and archive; five attempts use interruptible 0/2/5/15/60s delays. Exhaustion retains valid cache and retries in 1h, or retries in 5m without a valid dataset. Successful/current checks use 14 days. Test-only fetch/wait hooks cover publication mismatch, rejected candidates, cache retention, strict temporary cleanup and cancellation without network access. |
| Consensus bootstrap state | No grants, roster, or network-role announcements | Removed. |
| Consensus state | Unified current state format | State v12 with canonical AUTH and only OnboardingPool; older decoders removed. |
| AUTH state | Canonical non-transferable AUTH in AccountState and GenesisAllocation; state root commits it | Implemented (`393657f`): `AccountState.authority`, `GenesisAllocation.authority`, state v12 (AUTH first introduced in v11). Legacy AuthorityIndex/AuthorityPolicy removed. Desktop reads AUTH from AccountState (`52c5406`). |
| AUTH transitions | Current utility-bound earning; PoA-signed `PoaAuthAdjustment` GRANT / BURN | Implemented: utility operations RootPublication and SystemLock earn flat +1, capped at +1 per account per finalized block (`59ada0b`); AccountCreate, maintenance, payments and adjustments earn nothing. `PoaAuthAdjustment` (`4a393fd`) is signed by the genesis PoA key, bound to one block height, once per block, verified by every node; never relayed. No CLI/GUI action yet issues adjustments. |
| Candidate execution | Every full node executes candidates before relay; one pool per node, used by PoA for blocks | Implemented (`7f447d4`): `CybouNodeRuntime` owns the single `OperationPool`; local, peer and relayed operations pass `OperationPool::Admit` before staging or forwarding; every node revalidates after each finalized block. CybouNodeRuntime produces from that pool; PoaFinalizer only signs with its durable journal. |
| Validation | `ValidationAttestation` after local execution by an Identity with finalized AUTH > 1,000,000 | Implemented: codec, eligibility and vault-backed signer (`c9422a8`), bounded RAM `ValidationPool` and `OperationStatus::IsValidated()` (`f7f894f`), CYP2 frame v5 `VALIDATION_ATTESTATION_POLL` / `VALIDATION_ATTESTATION` (`037e6b0`), desktop Validated state and signer (`885986f`). An attestation served before the receiver holds the candidate is not stored; it is re-offered on the next finalized base or session. Mail/Files publication jobs do not yet show Validated. No penalty rule. |
| Executables | One production `cybou` | `cybou` is the desktop and the headless CLI (`src/cybou/cli/cybou_cli.cpp`, one parser, no internal positional commands); `BUILD_GUI=OFF` builds a headless-only `cybou`. `cybou-node` and the `network bootstrap` command are removed; `network info` lists the locators. `cybou-loadgen` builds only with `BUILD_TESTS`. |
| LAB/CI network | Test-only, never an official key | `--network lab` exists only with `-DCYBOU_ENABLE_LAB_NETWORK=ON`; CI, storage smoke/soak, operator CLI and the stress controller use it. |
| Operation routing | Uniform CYP2 v5 Full Node mesh | HELLO has no capability field. No PoA transport proof or special route. Sync completion is advisory. Storage is intrinsic; StorageId is challenged only for storage interaction. |
| PoA | Sole independent canonical finalizer | Single-operator PoA re-executes every candidate through the node pool and finalizes with zero attestations; a multi-node CYP2 test covers validator, ordinary node and PoA. |

## Uniform Full Node pass (2026-10-03)

CYP2 v5 uses an 80-byte HELLO with no network-role field. Every Full Node
serves blocks, discovery, operation relay, Validation transport and encrypted
storage with a positive automatic or explicitly configured local quota. On-demand StorageId proof is
restricted to storage interactions. `node run` and its optional
`--poa-key-file` option use `StartNetwork`; CybouNodeRuntime produces blocks
and `PoaFinalizer` owns durable signing safety. Signer toggles preserve
peer sessions. Qt uses feature availability and reports Full Node properties.
The economic reset uses the currently provisioned immutable v12 DEVNET.

## Prepared economics reset (2026-10-03)

Direct atomic Payment and RootPublication fees credit the unique `cybou`
genesis allocation before claim, then its claimant's ordinary Balance.
OnboardingPool begins at 100M in the genesis builder; provisioning shares that
builder. Removed fee pools, end-of-block distribution, routing errors and GUI
pool metrics. State v12 rejects v11. Canonical supply conservation remains.

Current compiled DEVNET has its new NetworkID and immutable state v12 genesis.
The former DEVNET is retired; no genesis is replaced under its NetworkID.
MAINNET remains unprovisioned. LAB uses separate fixed test keys.

Verification: headless LAB and Qt desktop builds succeeded; all 216 core tests
(5,787 assertions) passed. The multi-process storage smoke passed publication,
finality, remote protection, provider loss, repair and exact retrieval of
716,800 bytes. Translation XML and the documentation manifest were checked.

## DEV deployment and acceptance

The DEV VPS runs `cybou-node.service` as an ordinary Full Node at the compiled
TLS-pinned locator. The old prototype service is inactive. Its TLS certificate
continues to provide transport identity only; no PoA or Network Root secret is
placed on the VPS. France-only admission remains fail closed.

The hardening pass verifies existing DEVNET private material offline, builds and
tests the ordinary `node run` command, then deploys it with a clean v12 domain.
Deployment and acceptance evidence is recorded below after verification.

## Completed simplification pass (2026-10-03)

Uniform CYP2 v5 uses compact IDs 1–26, consecutive GET_BLOCKS batches and a
shared operation transfer/acknowledgment path. One ordered configured-peer
vector holds endpoints and optional pins. Runtime and StateStore receive only
VerifiedNetworkGenesis; its signed specification digest anchors the chain and
PoA signing journal. `node run --poa-key-file` is the sole headless signer path.

PublicationService stages directly into the shared pinned blob store and saves
one encrypted leaf list. Transient Merkle levels generate one proof at a time.
RootPublication v4 and encrypted/private schema v3 use bounded binary layouts,
strict UTF-8 and exact input consumption. Native Hash256 preserves raw 32-byte
serialization with forward hex. Generic CBOR and inherited Bitcoin blob/util/
compat code are removed, along with unused libevent and Boost dependencies.

Existing economics, AUTH transitions, France-only fail-closed admission,
hybrid post-quantum signatures and BLAKE3 remain the implementation baseline.
That earlier simplification pass did not provision or deploy the network.
The subsequent hardening pass uses the existing new DEVNET constants.

Verification for this pass: both headless LAB and Qt builds succeeded; all 205
core tests and 49 Qt tests passed. Operator CLI acceptance and the multi-process
storage smoke passed, including finality, remote protection, provider loss,
repair and exact retrieval of 716,800 bytes. Seven stress-controller unit tests
ran successfully with one skip. Active CYBOU production sources have zero
references to the removed runtime APIs and Bitcoin/CBOR substrate. Third-party
crc32c CPU hardware capability constants are outside that runtime check.

## Final hardening and DEV deployment (2026-10-03)

Existing Network Root and cybou.cybou mnemonic/seed material match compiled
Network, PoA and Recovery keys, AccountID, allocation, genesis signature and
state v12 root. The saved public AccountID and summary state-root text were
normalized from the former reversed hex representation; seeds, keys, NetworkID
and signed genesis were unchanged. Private files remain gitignored.

Production `cybou` does not link provisioning and rejects Network Root private
key derivation/signing. `cybou-provision` is opt-in and refuses replacement
outputs. Storage defaults to a positive automatic allocation and reserves disk
space; the opt-in GUI checkbox is removed. PoA retries retain the exact intent
and certificate, use bounded backoff, preserve the worker across lock/unlock,
and keep safety halts fail closed. Storage PUT/GET/proof have local byte/request
and concurrency limits before chunk reads, with progress and absolute deadlines.
Outward storage identifiers and events use `storage_*`; deployed cryptographic
domains and storage-key bytes retain their original definitions.

The VPS production headless build has tests, LAB and provisioning disabled.
`cybou-node.service` uses `cybou node run` at `51.255.46.58:29461`, with the
compiled TLS SPKI pin, validated October Geo cache and automatic allocation
about 5.8 GiB. Old state is retired separately; the new state starts at its
immutable genesis. No Network Root or PoA secret was sent to the VPS.
Doctor reports READY, and a desktop-to-VPS France-admitted, TLS-pinned CYP2
probe succeeded. PoA production remains the Central Authority desktop's job;
this pass does not activate an official signer or manufacture official traffic.

Verification: 210 core tests (8,640 assertions), 49 Qt tests, operator CLI
acceptance, and multi-process LAB storage smoke (finality, remote protection,
replica loss, repair, exact retrieval of 716,800 bytes) passed. A separately
linked probe confirms production Network Root derivation and signing reject.
Final transport/runtime refinements receive focused regression checks.

The default Windows desktop domain was also cut over: legacy network-bound files
are retired under `C:/Users/cybou/AppData/Local/CYBOU-retired-20261003-hardening`.
`C:/Users/cybou/AppData/Local/CYBOU/cybou_state` now starts from current v12
genesis, passes doctor READY and probes the VPS successfully. Host virtualization
resources were preserved at their existing paths; `cybou-guest` is Off and the
new Full Node does not use it. The desktop production build has BUILD_TESTS,
LAB and provisioning disabled. The separate headless LAB build retains tests.

After the final idle-session/truncated-frame refinement, all 53 runtime/P2P
regression cases (1,082 assertions), CLI acceptance and storage smoke passed.
Provisioning refusal against existing output was checked without generating any
new material. Translation XML and the documentation manifest are consistent.

### Repeat code audit (2026-10-03)

The repeat audit closes a consumed-frame-header/body-timeout reuse gap, makes
storage concurrency leases independent of runtime lifetime and exception-safe,
and bounds decoded operation-count allocation by the actual block input.
PoA checks serialized candidate size including certificate overhead before
recording a signing intent. Offline provisioning exclusively creates public
constants and cleanses seeds and mnemonic words on early write failure as well
as success. The architecture document now agrees with the positive production
storage allocation rule.

All 212 core tests (8,643 assertions), operator CLI acceptance and LAB storage
smoke passed. Existing DEVNET private material again matches compiled public
constants. Production GUI and VPS headless builds succeeded. The VPS binary was
updated without resetting state or provisioning a network; its service is active
and doctor reports READY at the unchanged genesis. Official PoA remains locked.
