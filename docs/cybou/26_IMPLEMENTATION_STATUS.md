# Implementation status

Status: documentation aligned to compiled OfficialNetwork target at HEAD `3d00945`.
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
| Consensus state | Unified current state format | `CYBOU_STATE_VERSION = 10`; legacy v7/v8/v9 decoding was removed. |
| Authority | Canonical non-transferable AUTH in AccountState and GenesisAllocation; state root commits it | Code still derives a local AuthorityIndex from age, activity and SystemLock history. State v11 and UI migration are pending. |
| Validation | Optional, non-canonical, eligible Authority > 1,000,000 | Attestation wire protocol and local evaluation policy are not implemented. |
| PoA | Sole independent canonical finalizer | Single-operator PoA exists; candidate relay and finalizer-only pending cleanup remain. |

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
6. Replace derived Authority with canonical AUTH in state v11 and desktop, then complete DEVNET end-to-end acceptance before Validation or provisional storage work.
