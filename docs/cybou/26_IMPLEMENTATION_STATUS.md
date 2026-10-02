# Implementation status

Status: code baseline `5095d01` (immutable CYG1 bundle). This page describes
implementation and deployment reality; `AGENTS.md` and the frozen architecture
define the target. Local uncommitted work is not counted as a released baseline.

## Target architecture vs. current code reality

| Component | Target constitution | Current code reality |
|---|---|---|
| Official networks | DEVNET and MAINNET only | The enum contains DEVNET and MAINNET. Both profile Network Public Key pins are empty; neither is ready for official startup. |
| Network identity | `NetworkID = Network Public Key` | CYG1 carries the exact canonical Network Public Key, but most runtime paths still use the SHA-256 hash of a `CybouNetworkDefinition`. |
| Signed genesis | One immutable, offline-signed CYG1 bundle per NetworkID | Canonical signed genesis, digest, CYG1 writer, verified bundle reader, and genesis state root check exist. Runtime still accepts CYN1 through `CybouNetworkFile`. |
| Genesis pinning | Official profiles pin exact NetworkID and GenesisDigest | Profile pins are not populated; the runtime is not yet bootstrapped exclusively from a verified official bundle. |
| Network Private Key | Strictly offline, signs genesis once | `network-genesis-create` signs CYG1 from a supplied secret file. Operational separation and one-time creation procedure remain to be enforced. |
| Bootstrap | Ordinary CYBOU full peer | Standalone prototype binary and bootstrap protocol still exist; the DEV VPS still runs `cybou-bootstrap.service`. |
| Consensus bootstrap state | No grants, roster, or `CAP_BOOTSTRAP` | Removed. |
| Consensus state | Unified current state format | `CYBOU_STATE_VERSION = 10`; legacy v7/v8/v9 decoding was removed. |
| Validation | Optional, non-canonical, eligible Authority > 1,000,000 | Attestation wire protocol and local evaluation policy are not implemented. |
| PoA | Sole independent canonical finalizer | Single-operator PoA exists; candidate relay and finalizer-only pending cleanup remain. |

## Current DEV VPS deployment

- The DEV VPS (`debian@vps-d0669a91.vps.ovh.net`) runs the standalone prototype
  `cybou-bootstrap.service` (`/home/debian/cybou/build/bin/cybou-bootstrap serve`),
  with state under `/var/lib/cybou/bootstrap/state`.
- Its TLS SPKI pin is compiled into `src/cybou/official_networks.h` for transport discovery.
- No production network exists. The coordinated ordinary full-peer cutover has not occurred.

## Open integration gates

1. Pin the actual DEVNET Network Public Key and GenesisDigest and package its CYG1 bundle.
2. Feed only a verified, pinned CYG1 bundle into official runtime startup; remove CYN1 and legacy network-init paths.
3. Transition runtime, wire, persistence, and cryptographic binding to exact Network Public Key NetworkID.
4. Migrate DEV VPS to an ordinary `cybou-node` full-peer service after the coordinated state reset.
5. Separate the operational PoA secret from the user Identity vault.
6. Implement deterministic Authority eligibility and optional Validation against finalized state.
7. Complete end-to-end DEVNET acceptance before any MAINNET launch.
