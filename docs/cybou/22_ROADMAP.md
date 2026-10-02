# CYBOU protocol and product roadmap

CYBOU is an Identity-centered private Mail and Files platform over an
encrypted P2P mesh with single-operator hybrid-PQ PoA finality.

## Phase 1 — Architectural alignment and clean core (In Progress)

1. **Constitutional documentation alignment**: establish single truth across Level 0, 1, and 2 documents (Completed in `AGENTS.md`, `24_DECISIONS.md`, `02_ARCHITECTURE.md`, `04_NETWORK_LIFECYCLE.md`, `VALIDATION.md`, and core domain specs).
2. **State & transport cleanup**: completely eliminate obsolete bootstrap Identity, consensus grants, `CAP_BOOTSTRAP`, and legacy state decoders; establish clean `CYBOU_STATE_VERSION = 10` (Completed in code HEAD `0437427`).
3. **Compiled official network definition**: documentation now selects compiled public Network Key, signed `NetworkGenesis`, initial state and bootstrap locators. Existing CYG1/CYN1 and external-file code is migration debt, not the target.

## Phase 2 — Network identity and bootstrap transition

4. **DEVNET provisioning**: gitignore `/private/`; generate Network and ordinary `cybou.cybou` Identity secret material once; generate and compile only the public Network Key, signed genesis, initial state, Identity and PoA key data. MAINNET stays unprovisioned and GUI-disabled.
5. **One official startup path**: select compiled DEVNET constants, verify signature/state root, and remove external official CYG1/CYN1 loaders, profile digest pins and `--network` file startup.
6. **NetworkID transition**: transition `NetworkId` from SHA256 definition hash to exact Network Public Key throughout runtime, wire, persistence and crypto.
7. **Bootstrap conversion**: remove legacy bootstrap binding/protocol and standalone executable; transition DEV VPS to an ordinary `cybou-node` after coordinated state reset.
8. **Canonical AUTH state** (Completed in `393657f`, `52c5406`): AUTH in AccountState and GenesisAllocation, state v11, AuthorityIndex removed, desktop reads AUTH directly.
9. **DEVNET acceptance**: verify desktop, ordinary peers, finality, operation relay and storage end to end.

## Phase 3 — AUTH and Validation

- **A — AUTH transitions** (Completed `99af6db`, `4a393fd`): +1 AUTH per finalized Identity-authorized operation; one PoA-signed `PoaAuthAdjustment` operation (GRANT / BURN, floor 0).
- **B — Full-node independent candidate execution** (Completed `7f447d4`): move `OperationPool` from `CybouFinalizerNode` into `CybouNodeRuntime`; every node executes candidates before relay; PoA produces blocks from the same pool.
- **C — Validation signatures** (Completed `c9422a8`, `f7f894f`, `037e6b0`): `ValidationAttestation` (NetworkID, OperationID, finalized base BlockID, AccountID, Authorization signature), local signing when AUTH > 1,000,000, bounded Validation store, `VALIDATION_ATTESTATION` CYP2 gossip.
- **D — UI** (Wallet completed `885986f`): AUTH, Validation eligibility, Submitted / Validated · N / Finalized; Mail/Files publication jobs and a PoA adjustment action remain.
- **E — Hardening**: evidence for invalid Validation and a frozen AUTH penalty table.

## Phase 4 — Product integration and end-to-end acceptance

13. **Application data plane integration**: connect Mail and Files publication and finalized storage placement to the desktop model.
14. **Storage durability hardening**: verify Beta target of 2 independent remote full replicas plus local copy (3 physical copies total) with audit and repair.
15. **MAINNET provisioning and launch**: create its own keys, genesis and bootstrap only after full DEVNET soak and formal acceptance.
