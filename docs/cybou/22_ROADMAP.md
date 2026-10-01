# CYBOU protocol and product roadmap

The active protocol is genesis-bound hybrid-PQ PoA, generic RootPublication,
and one encrypted content-addressed chunk store.

## Delivered substrate

- stable AccountID, Identity authorization, recovery, names, and balances;
- PoA finality with durable signing journal and conflict halt;
- generic RootPublication and recipient capsules;
- encrypted ROOT/INDEX/DATA trees and BLAKE3 ChunkID;
- finalized-publication Merkle chunk admission;
- CYP2 v3 over TLS 1.3, finalized block sync, and content-addressed PUT/GET;
- removal of legacy BFT, MailTx, and indexed StorageObject paths.

## Application integration

The encrypted per-Identity Application DB, ApplicationService,
PublicationService, StorageService, private Mail and Files backends,
clean-machine RecoveryBridge flow, one-remote-replica development target,
audit/repair, and placement recovery are integrated with the desktop.

## Current phase: soak, hardening, Beta preparation

1. Run multi-process provider soak tests covering loss, restart, repair, and
   lost local state.
2. Run reproducible core and Qt builds and tests after each implementation
   batch.
3. Complete clean-install desktop acceptance.
4. Measure two independent remote full replicas for Beta.
5. Keep Authority informational and Validation advisory unless a concrete
   product need justifies a separately reviewed change.

## Network lifecycle migration

The approved target is specified in
[`04_NETWORK_BOOTSTRAP_AND_GENESIS.md`](04_NETWORK_BOOTSTRAP_AND_GENESIS.md):
one non-finalizing/non-provider bootstrap service on the official VPS and the
genesis-key holder finalizing from its Central Authority desktop. The current
DEV executable and services still use the legacy arrangement. Before cutover,
implement bootstrap identity pinning, EMPTY/BOUND state, one-use initial claim,
generation-safe replacement, desktop genesis creation/finalizer lifecycle,
and authenticated relay. Pass clean-install, compromise, rollback, restart,
offline-authority, and replacement acceptance tests, then perform an explicit
DEV reset/cutover. Routine deployments continue to preserve the existing DEV
network and key until that procedure is ready.

## Authority and Validation

Authority is a read-only metric derived from finalized account history. A
recipient may use Authority >= 1,000,000 to label a signed opinion as
validator-qualified. That local label does not affect admission, state,
finality, or resources. Any full node may provide an optional Validation
opinion; the recipient verifies and trusts it locally. PoA remains the sole
source of finality.

There is no canonical NodeID binding, validator registry, liveness/storage
evidence, resource budget, reservation, ticket, reward, or penalty subsystem.

## Beta

Beta requires:

- Gmail-familiar Mail and Google-Drive-familiar Files UX;
- clean-machine recovery without the old Application DB;
- two independent remote full replicas per required chunk;
- measured provider loss and repair behavior;
- honest `Protected` and `Sent` states;
- operational cost and local abuse-policy measurements;
- no Reed-Solomon/erasure-coding requirement.

Do not block Mail/Files Beta on optional Validation. Do not freeze a
deterministic provider-placement algorithm before provider independence can be
measured reliably.
