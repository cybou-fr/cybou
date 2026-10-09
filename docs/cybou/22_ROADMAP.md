# CYBOU protocol and product roadmap

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

CYBOU is an Identity-centered private Mail and Files platform over an
encrypted P2P mesh with single-operator hybrid-PQ PoA finality.

## Architecture, economics and desktop completion (operator plan, 2026-10-09)

Priority override: the operator now requires completed storage economics before
Beta hardening. Follow [economics-first completion](ECONOMICS_SETTLEMENT_COMPLETION.md):
cumulative settlement accounting, assignment/evidence, recoverable end-to-end
payouts, then tariffs/Wallet and Beta acceptance. The sequence below records
the earlier Local/Network package order; it no longer postpones economics behind
extended UX testing. Critical data-loss/security/CI fixes remain urgent.

Continue the existing Local/Network implementation rather than replacing its
services. Delivery order: (1) staging cancellation and real-disk acceptance;
(2) active Outbox index and exact-operation crash recovery; (3) local data,
rotation and snapshot fault injection; (4) pure consensus-backed cost quotes;
(5) tariff/rounding simulations; (6) durable assignment-bound storage evidence;
(7) recoverable operator settlement preparation and evidence collection;
(8) explicitly agreed settlement schema and serialization vectors;
(9) finalized Wallet lease/escrow/refund/earnings ledger; (10) publication cost
and bounded renewal policy; (11) Mail/Files batch actions, Undo and semantic
deltas; (12) native desktop, offline/restart and independent-host Beta acceptance.

The settlement schema, any accrued-rent consensus fields and tariff changes
require their own normative decision and explicit deployment/cutover plan.
This roadmap does not authorize replacing immutable genesis, resetting DEVNET
or silently changing deployed wire. Simulations and exact quotes must reuse
current authoritative integer arithmetic. Detect settlement overflow explicitly;
do not silently truncate financial obligations. Successfully imported local
content stays successful when a separate network publication lacks funds.

The independent stager already exists in 06d8d7d; its scoped tests are not
10 GiB real-disk acceptance. Outbox active indexing is the first incremental
implementation; crash recovery, active-pass scheduling and the remaining
packages retain separate acceptance gates.
Three real child-process termination/restart checkpoints now cover staging,
acknowledged local commit and submission before status persistence on disk-backed
fixtures. See implementation status; rotation faults, arbitrary crash boundaries,
large imports and native desktop acceptance remain open. The process suite now
also covers prepared key access, finalized rotation before vault promotion and
restart after vault promotion. Concurrent desktop rotation and crashes inside
vault replacement retain separate acceptance gates.
The shared pure publication/renewal quote module is now implemented and checked
against consensus debits. Cost previews, tariff simulation and any tariff decision
are subsequent packages; no deployed economics changes follow from these quotes.
The dated economics simulation now covers the requested workload/duration matrix,
renewal rounding and provider/onboarding sensitivity. DOC-020 captures the current
period-cap versus accrued-rent gap. Selecting tariffs, cumulative payout rules,
assignment/evidence and GUI financial projections remain separate work.

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

13. **Application data plane integration** (connected; product acceptance open): Mail/Files publication, finalized history indexing, retrieval and storage projections reach the live desktop. Command acknowledgments, draft safety and shared interaction/evidence surfaces are implemented; verify live outage/restart and clean-client recovery rather than reconnecting the data plane.
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

## Desktop Beta acceptance and performance evidence (2026-10-08)

Product scope correction, 2026-10-09: the primary Network page must eventually
show exactly current finalized op/min, reported network storage capacity and actual
network-hosted encrypted bytes. Local node counters belong in Home/Advanced,
not a substitute network summary. The full-map layout has been restored.
The lightweight direct provider-counter query, approximate aggregate and header
overlay are source-implemented under DEC-290. Update peers and verify actual
response coverage. No new accounting/audit platform or fresh capacity benchmark
is a prerequisite for these indicative values. The full-map layout is retained
and benchmark UI removed. Independent Beta acceptance gates below remain open.

[DESKTOP_BETA_ACCEPTANCE_PLAN.md](DESKTOP_BETA_ACCEPTANCE_PLAN.md) is the active
Level 4 desktop plan reviewed at `1f5417fb`; historical implementation delivery
remains in `DESKTOP_UX_DELIVERY_PLAN.md`. Existing Console, Monitor, Authority,
Mail/Files and shared task architecture is retained. Current order:

1. P0: resolve/reproduce current core CI build failure and establish matching
   green core/desktop/Qt/Python evidence for the repaired revision.
2. P0: prepare clean clients and independently documented remote failure domains
   before distributed durability acceptance; preserve one signer/history.
3. P0: live Identity restore/rotation, Wallet uncertainty/restart and operator
   acceptance; live Mail/Files offline/retrieval/repair/shared-retention scenarios.
4. P0: physical keyboard/mouse/drag/native dialogs, mixed DPI and real assistive
   technology acceptance in FR/EN and both themes.
5. Freeze Beta monitoring under DEC-289 and
   [NETWORK_OBSERVABILITY_PLAN.md](NETWORK_OBSERVABILITY_PLAN.md): local passive
   metrics, one verified chain stream, ordinary peer liveness and storage-service
   evidence. No remote telemetry or automatic expansion of the metric set. Any
   addition needs a concrete operational question and explicit scope decision.
6. P1: multi-host stepped/mixed capacity benchmark for historical throughput
   ceilings, then targeted polish. Retain op/min and simulation scope.

Implementation and component passes do not close live Beta gates. Wider census,
pending-file Compose and stronger purge claims remain explicit design gates;
none changes placement, canonical finality, network roles or immutable genesis.

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
