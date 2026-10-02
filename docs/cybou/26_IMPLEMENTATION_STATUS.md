# Implementation status

Status: AUTH, candidate execution, Validation and PoA rows verified against code at HEAD `885986f`;
other rows last aligned at `3d00945`.
This page describes
implementation and deployment reality; `AGENTS.md` and the frozen architecture
define the target. Local uncommitted work is not counted as a released baseline.

## Target architecture vs. current code reality

| Component | Target constitution | Current code reality |
|---|---|---|
| Official networks | Compiled DEVNET public constants; MAINNET unprovisioned and GUI-disabled | The enum contains DEVNET and MAINNET. Public Network Key fields are empty; MAINNET is not yet disabled throughout the GUI. DEV bootstrap locator is compiled. |
| Network identity | `NetworkID = Network Public Key` | Genesis code carries canonical Network Public Key bytes, but most runtime paths still use the SHA-256 hash of a `CybouNetworkDefinition`. |
| Signed genesis | One immutable offline-signed `NetworkGenesis` object and initial state compiled per official network | Canonical signing and verification exist, but code still has CYG1 reader/writer and external genesis/network-file paths. CLI loads CYG1; desktop and utility paths still use `CybouNetworkFile` and CYN1. These are technical debt, not architecture. |
| Official startup | Select compiled public constants; verify signature and initial state root; no external official file or separate digest pin | Official public constants are not provisioned. CLI requires external genesis and profile digest pins; desktop pinning remains definition-hash based. |
| Network Private Key | Strictly offline, signs genesis once; private material stays under gitignored `/private/` | `network-genesis-create` signs CYG1 from a supplied secret file. `/private/` is ignored in this working tree; one-time provisioning and generated public C++ constants remain to implement. |
| Bootstrap | Ordinary CYBOU full peer | Standalone prototype binary and bootstrap protocol still exist; the DEV VPS still runs `cybou-bootstrap.service`. |
| Consensus bootstrap state | No grants, roster, or `CAP_BOOTSTRAP` | Removed. |
| Consensus state | Unified current state format | State v11 with canonical AUTH; older decoders removed. |
| AUTH state | Canonical non-transferable AUTH in AccountState and GenesisAllocation; state root commits it | Implemented (`393657f`): `AccountState.authority`, `GenesisAllocation.authority`, state v11. Legacy AuthorityIndex/AuthorityPolicy removed. Desktop reads AUTH from AccountState (`52c5406`). |
| AUTH transitions | +1 per finalized Identity-authorized operation; PoA-signed `PoaAuthAdjustment` GRANT / BURN | Implemented: flat +1 in `ExecuteBlockOperations` (`99af6db`; AccountCreate and `PoaAuthAdjustment` earn nothing, saturating). `PoaAuthAdjustment` (`4a393fd`) is signed by the genesis PoA key, bound to one block height, once per block, verified by every node; never relayed. No CLI/GUI action yet issues adjustments. |
| Candidate execution | Every full node executes candidates before relay; one pool per node, used by PoA for blocks | Implemented (`7f447d4`): `CybouNodeRuntime` owns the single `OperationPool`; local, peer and relayed operations pass `OperationPool::Admit` before staging or forwarding; every node revalidates after each finalized block. `CybouFinalizerNode` seals that pool. |
| Validation | `ValidationAttestation` after local execution by an Identity with finalized AUTH > 1,000,000 | Implemented: codec, eligibility and vault-backed signer (`c9422a8`), bounded RAM `ValidationPool` and `OperationStatus::IsValidated()` (`f7f894f`), CYP2 frame v4 `VALIDATION_ATTESTATION_POLL` / `VALIDATION_ATTESTATION` (`037e6b0`), desktop Validated state and signer (`885986f`). An attestation served before the receiver holds the candidate is not stored; it is re-offered on the next finalized base or session. Mail/Files publication jobs do not yet show Validated. No penalty rule. |
| PoA | Sole independent canonical finalizer | Single-operator PoA re-executes every candidate through the node pool and finalizes with zero attestations; a multi-node CYP2 test covers validator, ordinary node and PoA. |

## Current DEV VPS deployment

- The DEV VPS (`debian@vps-d0669a91.vps.ovh.net`) runs the standalone prototype
  `cybou-bootstrap.service` (`/home/debian/cybou/build/bin/cybou-bootstrap serve`),
  with state under `/var/lib/cybou/bootstrap/state`.
- Its TLS SPKI pin is compiled into `src/cybou/official_networks.h` for transport discovery.
- No production network exists. The coordinated ordinary full-peer cutover has not occurred.

## Open integration gates

1. Gitignore `/private/`; provision DEVNET Network and ordinary `cybou.cybou` Identity secrets once and generate only public C++ constants.
2. Make `NetworkKind::DEVNET` select its compiled Network Public Key, signed `NetworkGenesis`, initial state, PoA public key and bootstrap locator. Keep MAINNET unprovisioned and GUI-disabled.
3. Remove CYG1/CYN1 external official network loaders, file options and separate profile `GenesisDigest` pinning. CLI `network init-dev` has already been removed.
4. Transition runtime, wire, persistence and cryptographic binding to exact Network Public Key NetworkID.
5. Remove legacy bootstrap binding/protocol and prototype binary; migrate DEV VPS to ordinary `cybou-node` after coordinated state reset.
6. Implement AUTH transitions, universal per-node candidate execution and Validation signatures (see `22_ROADMAP.md` Phase 3).
