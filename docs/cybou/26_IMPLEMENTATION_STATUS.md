# Implementation status

Status: updated through DEVNET-only development (2026-10-03).
This page describes
implementation and deployment reality; `AGENTS.md` and the frozen architecture
define the target. The prepared changes described here are not a released baseline.

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
| Consensus state | Unified current state format | State with canonical AUTH and only OnboardingPool; older decoders removed. |
| AUTH state | Canonical non-transferable AUTH in AccountState and GenesisAllocation; state root commits it | Implemented (`393657f`): `AccountState.authority`, `GenesisAllocation.authority`, state (AUTH first introduced in v11). Legacy AuthorityIndex/AuthorityPolicy removed. Desktop reads AUTH from AccountState (`52c5406`). |
| AUTH transitions | Current utility-bound earning; PoA-signed `PoaAuthAdjustment` GRANT / BURN | Implemented: utility operations RootPublication and SystemLock earn flat +1, capped at +1 per account per finalized block (`59ada0b`); AccountCreate, maintenance, payments and adjustments earn nothing. `PoaAuthAdjustment` (`4a393fd`) is signed by the genesis PoA key, bound to one block height, once per block, verified by every node; never relayed. No CLI/GUI action yet issues adjustments. |
| Candidate execution | Every full node executes candidates before relay; one pool per node, used by PoA for blocks | Implemented (`7f447d4`): `CybouNodeRuntime` owns the single `OperationPool`; local, peer and relayed operations pass `OperationPool::Admit` before staging or forwarding; every node revalidates after each finalized block. CybouNodeRuntime produces from that pool; PoaFinalizer only signs with its durable journal. |
| Validation | `ValidationAttestation` after local execution by an Identity with finalized AUTH > 1,000,000 | Implemented: codec, eligibility and vault-backed signer (`c9422a8`), bounded RAM `ValidationPool` and `OperationStatus::IsValidated()` (`f7f894f`), CYBOU P2P frame `VALIDATION_ATTESTATION_POLL` / `VALIDATION_ATTESTATION` (`037e6b0`), desktop Validated state and signer (`885986f`). An attestation served before the receiver holds the candidate is not stored; it is re-offered on the next finalized base or session. Mail/Files publication jobs do not yet show Validated. No penalty rule. |
| Executables | One production `cybou` | `cybou` is the desktop and the headless CLI (`src/cybou/cli/cybou_cli.cpp`, one parser, no internal positional commands); `BUILD_GUI=OFF` builds a headless-only `cybou`. `cybou-node` and the `network bootstrap` command are removed; `network info` lists the locators. `cybou-loadgen` builds only with `BUILD_TESTS`. |
| Operation routing | Uniform CYBOU P2P Full Node mesh | HELLO has no capability field. No PoA transport proof or special route. Sync completion is advisory. Storage is intrinsic; StorageId is challenged only for storage interaction. |
| PoA | Sole independent canonical finalizer | Single-operator PoA re-executes every candidate through the node pool and finalizes with zero attestations; a multi-node CYBOU P2P test covers validator, ordinary node and PoA. |

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
`6efa3107b7c40e6c49e6644569810c7ba1a88384705da3b3601d570eb892fcd7`.
Signed genesis anchor:
`64e9bc4b0533f2753185428cfbd2f788dca023ed2899b22caee54a85971e9eea`.
The pre-generated material is under gitignored `private/devnet/`. Production
Network Root derivation/signing are disabled. The existing official PoA is
locked; finality/content integration requires the operator to unlock it.

The VPS runs the ordinary Full Node at `51.255.46.58:29461`. Public peer admission
uses verified DB-IP Geo data and the compiled TLS pin. The earlier desktop/VPS
network domains remain retired; this pass does not reset or import them.

See [DEVNET development](DEVNET_DEVELOPMENT.md) for commands and operator rules.

Verification: all 211 core tests (8,622 assertions) and all 49 Qt tests pass.
The DEVNET CLI acceptance reaches the existing pinned locator, restarts an
ordinary node and checks read-only doctor without activating a signer or
submitting operations. Production Windows GUI and Linux headless builds pass;
both linked Network Root guards reject private derivation/signing. The VPS
service is active and desktop/VPS doctors report READY on the unchanged genesis.
Its binary update preserved state. Finality/storage fault scenarios are not run
against the locked official signer during this removal.
