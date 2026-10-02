# Implementation status

Status: Last code-changing baseline: `0437427`.
Documentation-only commits after that baseline do not change implementation reality.
This page reports current code and deployment reality honestly, separated from
the constitutional target in `AGENTS.md` and `04_NETWORK_LIFECYCLE.md`.

## Target architecture vs. current code reality

| Component | Target constitution | Current code reality |
|---|---|---|
| Official networks | DEVNET and MAINNET only | Profiles still declare DEVNET, TESTNET, MAINNET enum in `official_networks.h` |
| Network identity | `NetworkID = Network Public Key` | `NetworkId = SHA256("CYBOU/NETWORK-ID/V5" \|\| Serialize(definition))` |
| Network Private Key | Strictly offline root authority; signs genesis | Genesis specification parsing is present, but offline-signed genesis format is pending wire definition |
| Bootstrap role | Ordinary CYBOU full peer; same executable and CYP2 | Legacy standalone prototype `cybou-bootstrap` binary still exists in repo; VPS service runs `cybou-bootstrap serve` |
| Consensus bootstrap state | Completely removed (ordinary Identity, genesis Authority baseline) | Cleanly removed in HEAD `0437427` (no bootstrap grants, roster, or `CAP_BOOTSTRAP`) |
| Consensus state version | Single unified version: `CYBOU_STATE_VERSION = 10` | Implemented in HEAD `0437427` (legacy v7/v8/v9 decoding logic deleted) |
| Advisory Validation | Active provisional pre-finalization (Authority > 1M) | Core data models cleaned in `0437427`; active Validation wire protocol and local evaluation policy pending implementation |
| PoA Finalizer role | Sole canonical finalizer; independently executes candidates | Single-operator PoA implemented; pending operation queue cleanup to finalizer-only pending |

## Current DEV VPS deployment

- The DEV VPS (`debian@vps-d0669a91.vps.ovh.net`) currently runs the standalone prototype
  `cybou-bootstrap.service` (`/home/debian/cybou/build/bin/cybou-bootstrap serve` on `51.255.46.58:29461`,
  with state under `/var/lib/cybou/bootstrap/state`).
- Its TLS SPKI pin is compiled into `src/cybou/official_networks.h`.
- Migration state: The legacy standalone service remains running until the coordinated
  cutover to an ordinary CYBOU full-peer service running `cybou-node`.

## Open integration gates

1. Define Network Key format and canonical signed genesis encoding with monotonic `genesis_generation`.
2. Transition `NetworkId` from SHA256 definition hash to exact Network Public Key.
3. Remove legacy TESTNET profile from `official_networks.h`.
4. Implement Validation attestation wire protocol, eligibility check (`Authority > 1,000,000`), and provisional local acceptance policy.
5. Implement mandatory rollback and reconciliation of provisional state upon PoA block arrival.
6. Migrate DEV VPS service from `cybou-bootstrap.service` to ordinary full-peer `cybou-node.service`.
7. End-to-end integration and clean cutover to DEVNET.
