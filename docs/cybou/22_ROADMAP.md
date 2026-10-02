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
8. **Canonical AUTH clean cut**: store AUTH in AccountState and GenesisAllocation, remove initial_authority and AuthorityIndex, update desktop, bump state format, and verify supply independence.
9. **DEVNET acceptance**: verify desktop, ordinary peers, finality, operation relay and storage end to end before Validation or provisional storage work.

## Phase 3 — After DEVNET acceptance

10. **Validation attestation protocol**: implement advisory Validation wire messaging for eligible Identities (`AccountState.authority > 1,000,000 AUTH` in latest PoA-finalized state).
11. **Provisional execution & rollback**: implement local acceptance policy (`validation.enabled`, `min_signatures`), provisional state staging, and mandatory atomic rollback upon PoA conflict.
12. **Finalizer simplification**: ensure PoA finalizer acts purely as an independent candidate evaluator and certificate signer with no canonical pending mempool.

## Phase 4 — Product integration and end-to-end acceptance

13. **Application data plane integration**: connect Mail and Files publication, provisional staging, and finalized promotion to the desktop model.
14. **Storage durability hardening**: verify Beta target of 2 independent remote full replicas plus local copy (3 physical copies total) with audit and repair.
15. **MAINNET provisioning and launch**: create its own keys, genesis and bootstrap only after full DEVNET soak and formal acceptance.
