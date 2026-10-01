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

The revised target is specified in
[`04_NETWORK_BOOTSTRAP_AND_GENESIS.md`](04_NETWORK_BOOTSTRAP_AND_GENESIS.md):
all participants run the same full-node software with optional capabilities;
genesis authorizes one to four bootstrap Identities; and production/DEV public
P2P admission is France-only. Node, bootstrap, and desktop automatically check
DB-IP Lite for updates at startup and every 14 days, verify the vendor's
published archive digest and the CSV contents, and atomically activate a fresh
cache. A cached release older than 45 days fails closed. Acceptance tests for
the desktop/bootstrap boundaries remain. DEV currently runs one
bootstrap-only VPS service with an empty bootstrap store and no finalizer or
storage providers. The bootstrap executable remains a prototype. Before the
target architecture is complete, implement the AccountID plus
RecoveryKeyID genesis roster, pre-genesis pinned Identity proof, current-key
per-session bootstrap proof, unified node capabilities, network creation
across selected bootstrap seeds, generation-safe replacement history, and
optional anonymizer filtering. Pass the multi-bootstrap,
clean-install, compromise, rollback, restart, France-admission, LAB-bypass,
Authority-mobility, offline-authority, and replacement acceptance tests, then
complete the coordinated DEV bootstrap setup and desktop-finalizer acceptance.
No production/Beta network is being migrated.

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
