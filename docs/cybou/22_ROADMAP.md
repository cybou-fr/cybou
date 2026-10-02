# CYBOU protocol and product roadmap

CYBOU is an Identity-centered private Mail and Files platform over an
encrypted P2P mesh with single-operator hybrid-PQ PoA finality.

## Phase 1 — Architectural alignment and clean core (In Progress)

1. **Constitutional documentation alignment**: establish single truth across Level 0, 1, and 2 documents (Completed in `AGENTS.md`, `24_DECISIONS.md`, `02_ARCHITECTURE.md`, `04_NETWORK_LIFECYCLE.md`, `VALIDATION.md`, and core domain specs).
2. **State & transport cleanup**: completely eliminate obsolete bootstrap Identity, consensus grants, `CAP_BOOTSTRAP`, and legacy state decoders; establish clean `CYBOU_STATE_VERSION = 10` (Completed in code HEAD `0437427`).
3. **Network Key and immutable signed genesis**: canonical Network Public Key encoding, CYG1 signing, digest and verified bundle reader are implemented. CLI now loads verified CYG1 and binds local state to its digest. Official profile pins, bundled desktop artifact and remaining runtime integration are open.

## Phase 2 — Network identity and bootstrap transition

4. **NetworkID transition**: transition `NetworkId` from SHA256 definition hash to exact Network Public Key.
5. **Official profile cleanup**: DEVNET and MAINNET are the only declared profiles (completed); populate and enforce their NetworkID and GenesisDigest pins before official startup.
6. **VPS bootstrap conversion**: transition DEV VPS from standalone `cybou-bootstrap.service` prototype to an ordinary full-peer running `cybou-node`.

## Phase 3 — Validation and provisional lifecycle

7. **Validation attestation protocol**: implement advisory Validation wire messaging for eligible Identities (`Authority > 1,000,000` in latest PoA-finalized state).
8. **Provisional execution & rollback**: implement local acceptance policy (`validation.enabled`, `min_signatures`), provisional state staging, and mandatory atomic rollback upon PoA conflict.
9. **Finalizer simplification**: ensure PoA finalizer acts purely as an independent candidate evaluator and certificate signer with no canonical pending mempool.

## Phase 4 — Product integration and end-to-end acceptance

10. **Application data plane integration**: connect Mail and Files publication, provisional staging, and finalized promotion to the desktop model.
11. **Storage durability hardening**: verify Beta target of 2 independent remote full replicas plus local copy (3 physical copies total) with audit and repair.
12. **DEVNET coordinated cutover**: deploy fresh signed genesis on DEV VPS and execute end-to-end multi-node desktop acceptance.
13. **MAINNET launch**: launch MAINNET only after full soak and formal acceptance.
