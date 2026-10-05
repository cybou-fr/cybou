# CYBOU protocol and product roadmap

CYBOU is an Identity-centered private Mail and Files platform over an
encrypted P2P mesh with single-operator hybrid-PQ PoA finality.

## Phase 1 — Architectural alignment and clean core (In Progress)

1. **Constitutional documentation alignment**: establish single truth across Level 0, 1, and 2 documents (Completed in `AGENTS.md`, `24_DECISIONS.md`, `02_ARCHITECTURE.md`, `04_NETWORK_LIFECYCLE.md`, `VALIDATION.md`, and core domain specs).
2. **State & transport cleanup**: completely eliminate obsolete bootstrap Identity, consensus grants, network-role announcement, and legacy state decoders; establish clean canonical state  (Completed in code HEAD `0437427`).
3. **Compiled official network definition** (Completed `ec76ef3`, `d5d90cb`): one `OfficialNetwork` built from the compiled Network Public Key, signed `NetworkGenesis`, initial state and bootstrap locators; CYG1/CYN1 and external network files are removed.

## Phase 2 — Network identity and bootstrap transition

4. **DEVNET provisioning** (Re-provisioned with a new Network key in `ec76ef3`): gitignore `/private/`; generate Network and ordinary `cybou.cybou` Identity secret material once; generate and compile only the public Network Key, signed genesis, initial state, Identity and PoA key data. MAINNET stays unprovisioned and GUI-disabled.
5. **One official startup path** (Completed `ec76ef3`): select compiled DEVNET constants, verify signature/state root, and remove external official CYG1/CYN1 loaders, profile digest pins and `--network` file startup.
6. **NetworkID transition** (Completed `66d5fd3`): NetworkID is the exact Network Public Key; 32-byte fields use `ComputeNetworkBinding(key)`.
7. **Bootstrap conversion**: legacy bootstrap binding/protocol/store and the standalone executable are removed (`ec76ef3`); nodes dial the compiled locator with its SPKI pin. The DEV VPS now runs the ordinary headless `cybou node run` on current DEVNET; deployment scope and retained states are recorded in `AGENTS.md`.
8. **AUTH and Validation removed** (DEC-284): accounts hold only CYBOU; flat relay PoW and fees price spam.
9. **DEVNET acceptance**: verify desktop, ordinary peers, finality, operation relay and storage end to end.

## Phase 3 — Candidate execution

- **B — Full-node independent candidate execution** (Completed `7f447d4`): move `OperationPool` from `PoaFinalizer` into `CybouNodeRuntime`; every node executes candidates before relay; PoA produces blocks from the same pool.

## Phase 4 — Product integration and end-to-end acceptance

13. **Application data plane integration** (connected; product acceptance open): Mail/Files publication, finalized history indexing, retrieval and storage projections reach the live desktop. Close remaining command acknowledgement, draft safety, interaction and evidence gaps rather than reconnecting the data plane.
14. **Storage durability hardening**: verify Beta target of 2 independent remote full replicas plus local copy (3 physical copies total) with audit and repair.
15. **Pre-MAINNET de-versioning clean-break**: eliminate all historical version markers across wire frames, state encoding, blocks, operations, schemas, and crypto domain separation strings (DEC-264).
16. **MAINNET provisioning and launch**: create its own keys, genesis and bootstrap only after full DEVNET soak and formal acceptance.

## Single-current-baseline completion

The canonical layouts omit historical format discriminators. P2P uses `CYBP`
magic; crypto domains and local storage namespaces are stable, unversioned
strings. Identity Vault uses `CYBV` and `CYID`. DEVNET cutover uses a fresh
NetworkID and signed genesis; the preceding network is retired permanently.

## Prepared economics reset

Direct Central Authority fees and state use the new compiled DEVNET
NetworkID. The previous DEVNET is retired. Hardening and deployment acceptance
close this pass before further product development.

## Simplification completion

The uniform Full Node runtime, compact CYBOU P2P transfers, verified-genesis
chain anchor, direct publication staging, transient Merkle proofs, bounded
binary schemas and native Hash256 are implemented and tested. Dependency and
legacy runtime cleanup is complete. The final hardening pass verifies existing DEVNET secrets offline, separates
provisioning tooling, enables automatic storage allocation, preserves PoA worker
liveness and hardens storage transfers. Subsequent work develops CYBOU product
features and durability rather than reopening architecture cleanup.

## Desktop product delivery (2026-10-05)

[`DESKTOP_UX_DELIVERY_PLAN.md`](DESKTOP_UX_DELIVERY_PLAN.md) is the active Level 4
work plan. Product targets are docs 72/73/78/81–85 and
[`NETWORK_AND_ADVANCED_UX.md`](NETWORK_AND_ADVANCED_UX.md). Order:

1. Measure/reproduce current defects; add durable local command results and
   loss-safe draft/send handling.
2. Stabilize item identity, activity/details updates and protection evidence.
3. Repair existing drag/drop and complete Files/Mail reference workflows.
4. Refine familiar shell, Wallet review, onboarding, accessibility and themes.
5. Add scoped Network overview/schematic France map, then extend existing
   Authority tools with verified explorer and evidence readiness.
6. Add optional own-content inspector/read-only console, with privacy bounds.
7. Close measured independent-replica/recovery/deletion acceptance; optional
   DEVNET benchmark tooling remains behind test-build boundaries.

Wider node census/top-uptime feed, pending-file compose and stronger remote
purge claims are explicit design gates. They do not change randomized paid
placement, canonical finality, network roles, keys or immutable genesis.

## Storage economy (DEC-274–DEC-283)

- **M1 Architecture freeze** (implemented): Level 0/1/2 documents and specs aligned.
- **M2 Explicit local capacity** (implemented): `V >= 15 GiB`, provider budget `floor(2V/3)`,
  automatic allocation removed; current DEVNET.
- **M3 Storage evidence** (implemented): durable obligations, receipts, audit transport, full-GET
  spot checks, rolling statistics; no money.
- **M4 Shadow economy** (implemented; DEVNET measurement pending): estimated rent and rewards on the live DEVNET; validate the
  5 CYBOU rate; no CYBOU moved.
- **M5 Consensus economics** (implemented; evidence aggregation pending): Treasury monetary base, 20,000 onboarding, StorageLease,
  StorageEscrow, StorageSettlement, storage quota removal.
- **M6 Adversarial tests** (implemented; per-account selection added; Identity-splitting price open): monetary conservation, payout abuse, storage failure,
  concentration and Sybil simulations as release gates.
- **M7 New DEVNET** (genesis provisioned, VPS cut over; desktop and live acceptance pending): new Network Root, NetworkID and signed genesis; VPS cutover;
  only under explicit operator authorization.
- **M8 Product Beta**: GUI earnings, cost, lease and escrow; measured economics.
