# Implementation status

Status: repository HEAD `818c683` plus current uncommitted workspace changes.
This page reports code/deployment reality separately from the target in
`04_NETWORK_LIFECYCLE.md`. Working-tree code changes may move before commit.

## Current DEV deployment

- `cybou-bootstrap.service` runs standalone `cybou-bootstrap serve` on
  `51.255.46.58:29461`, with state under `/var/lib/cybou/bootstrap/state`.
  Its TLS SPKI pin is compiled into `src/cybou/bootstrap_nodes.h`.
- The service is a pre-genesis `EMPTY`/`BOUND` prototype, not a CYP2 full
  node or finalizer. Legacy `cybou-node.service` and both provider services
  are inactive. No desktop finalizer is deployed on this VPS.
- There is no production or Beta network. A planned new-genesis DEV cutover
  replaces the old testnet only after acceptance and coordinated activation.

## Implemented in repository

- Deterministic state execution, hybrid Ed25519 + ML-DSA-65 PoA certificates,
  durable signing journal, conflict halt, operation and state-root validation.
- Account Identity, separate Recovery/Authorization/KEM roles, CVID5 vault,
  rotation/recovery, names, balances and System Balance.
- RootPublication, encrypted ROOT/INDEX/DATA trees, ChunkID verification,
  finalized-publication Merkle admission, provider transfer and placement.
- CYP2 v3 TLS transport, bounded operation relay, France-only Geo admission.
- Standalone bootstrap prototype with pinned TLS and activation store.
- Desktop finalizer core exists, but the official locator/binding/peer workflow
  and operator UX are not wired end to end or deployed.

## Obsolete implementation debt scheduled for deletion

The code still derives operational PoA authority from the genesis key, retains
bootstrap Identity/grant and `CAP_BOOTSTRAP` paths, legacy state versions,
bundled desktop `network.bin`, Validation UI scaffolding and the standalone
bootstrap prototype. These are implementation facts, not current architecture.
The target is immutable Network Root `R`, root-signed
`OfficialNetworkBinding` and height-specific PoA `P` assignments, with no
cross-generation Identity migration. Code cleanup must preserve current DEV
service until a coordinated cutover passes.

## Open integration gates

1. Remove obsolete consensus/transport/UI paths and install one clean state
   version at cutover.
2. Implement root-signed binding/assignment verification and separate `R`
   custody from routine desktop finalization.
3. Wire desktop bootstrap discovery, verified network creation/join, direct
   peer mesh and Authority operation under assigned `P`.
4. Make generation replacement atomic and wipe the complete old network-bound
   domain; keep epoch rotation non-destructive.
5. Complete Windows/Linux clean-install, sync, Mail/Files and recovery
   acceptance; then harden two-independent-replica Beta durability.
