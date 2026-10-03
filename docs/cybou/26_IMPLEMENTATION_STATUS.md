# Implementation status

Status: all rows verified against code at HEAD `d5d90cb`.
This page describes
implementation and deployment reality; `AGENTS.md` and the frozen architecture
define the target. Local uncommitted work is not counted as a released baseline.

## Target architecture vs. current code reality

| Component | Target constitution | Current code reality |
|---|---|---|
| Official networks | Compiled DEVNET public constants; MAINNET unprovisioned and GUI-disabled | Implemented (`d5d90cb`): one `OfficialNetwork` per kind via `RequireOfficialNetwork`. DEVNET re-provisioned with a new Network key (`ec76ef3`). MAINNET throws as not provisioned; the GUI has no network selector and always runs DEVNET. |
| Network identity | `NetworkID = Network Public Key` | Implemented: definitions carry the exact key; every 32-byte field is named `network_binding` and equals `ComputeNetworkBinding(key)`; `GetNetworkId()` exists only for the exact key bytes of a verified genesis. The runtime takes only the definition; the genesis digest marker is removed from the StateStore. |
| Signed genesis | One immutable offline-signed `NetworkGenesis` object and initial state compiled per official network | Implemented: the compiled genesis must carry exactly the compiled Network Public Key and its initial state root. CYG1/CYN1, `CybouNetworkFile` and every file loader are removed. |
| Official startup | Select compiled public constants; verify signature and initial state root; no external official file or separate digest pin | Implemented: every `cybou` command, loadgen, storage smoke/soak and the desktop start only from a compiled network (`--network devnet`, or `lab` in LAB test builds). No profile digest pin and no genesis digest in the runtime API. |
| Network Private Key | Strictly offline, signs genesis once; private material stays under gitignored `/private/` | `network provision-devnet` is the only provisioning flow; raw secret-file keygen and genesis-create commands are removed. The retired DEVNET material is kept under `/private/devnet-retired-20261003`. |
| Bootstrap | Ordinary CYBOU full peer | Bootstrap binding/protocol/store, `BOOTSTRAP_REQUEST/RESPONSE` and the `cybou-bootstrap` executable are removed. Nodes and the desktop dial the compiled locator first and check its SPKI pin; the locator node serves `--tls-certificate/--tls-key`. The DEV VPS still runs the retired prototype service. |
| Consensus bootstrap state | No grants, roster, or `CAP_BOOTSTRAP` | Removed. |
| Consensus state | Unified current state format | State v11 with canonical AUTH; older decoders removed. |
| AUTH state | Canonical non-transferable AUTH in AccountState and GenesisAllocation; state root commits it | Implemented (`393657f`): `AccountState.authority`, `GenesisAllocation.authority`, state v11. Legacy AuthorityIndex/AuthorityPolicy removed. Desktop reads AUTH from AccountState (`52c5406`). |
| AUTH transitions | +1 per finalized Identity-authorized operation; PoA-signed `PoaAuthAdjustment` GRANT / BURN | Implemented: flat +1 in `ExecuteBlockOperations` (`99af6db`; AccountCreate and `PoaAuthAdjustment` earn nothing, saturating). `PoaAuthAdjustment` (`4a393fd`) is signed by the genesis PoA key, bound to one block height, once per block, verified by every node; never relayed. No CLI/GUI action yet issues adjustments. |
| Candidate execution | Every full node executes candidates before relay; one pool per node, used by PoA for blocks | Implemented (`7f447d4`): `CybouNodeRuntime` owns the single `OperationPool`; local, peer and relayed operations pass `OperationPool::Admit` before staging or forwarding; every node revalidates after each finalized block. `CybouFinalizerNode` seals that pool. |
| Validation | `ValidationAttestation` after local execution by an Identity with finalized AUTH > 1,000,000 | Implemented: codec, eligibility and vault-backed signer (`c9422a8`), bounded RAM `ValidationPool` and `OperationStatus::IsValidated()` (`f7f894f`), CYP2 frame v4 `VALIDATION_ATTESTATION_POLL` / `VALIDATION_ATTESTATION` (`037e6b0`), desktop Validated state and signer (`885986f`). An attestation served before the receiver holds the candidate is not stored; it is re-offered on the next finalized base or session. Mail/Files publication jobs do not yet show Validated. No penalty rule. |
| Executables | One production `cybou` | `cybou` is the desktop and the headless CLI (`src/cybou/cli/cybou_cli.cpp`, one parser, no internal positional commands); `BUILD_GUI=OFF` builds a headless-only `cybou`. `cybou-node` and the `network bootstrap` command are removed; `network info` lists the locators. `cybou-loadgen` builds only with `BUILD_TESTS`. |
| LAB/CI network | Test-only, never an official key | `--network lab` exists only with `-DCYBOU_ENABLE_LAB_NETWORK=ON`; CI, storage smoke/soak, operator CLI and the stress controller use it. |
| Operation routing | Ordinary mesh relay | No preferred finalizer route: `CAP_ACCEPT_OPERATIONS`, finalizer relay sessions and `SubmitPeerOperation` are removed. `CAP_FINALIZER_PROOF` only announces the in-session PoA key proof used to confirm finalized tips. |
| PoA | Sole independent canonical finalizer | Single-operator PoA re-executes every candidate through the node pool and finalizes with zero attestations; a multi-node CYP2 test covers validator, ordinary node and PoA. |

## Current DEV VPS deployment

- The DEV VPS (`debian@vps-d0669a91.vps.ovh.net`) still runs the retired prototype
  `cybou-bootstrap.service` built from an older commit, with state under
  `/var/lib/cybou/bootstrap/state`. This repository no longer builds it.
- The DEVNET locator and its TLS SPKI pin are compiled in `src/cybou/official_networks.cpp`.
- No production network exists. The coordinated ordinary full-peer cutover has not occurred.

## Open integration gates

1. Migrate the DEV VPS to an ordinary headless `cybou ... --network devnet` node with a TLS certificate matching the compiled SPKI pin, after a coordinated state reset.
2. Complete DEVNET end-to-end acceptance.
