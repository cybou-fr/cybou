# CYBOU protocol and product roadmap

CYBOU is an Identity-centered private Mail and Files platform over an
encrypted P2P mesh with single-operator hybrid-PQ PoA finality.

## Phase 1 — Architectural alignment and clean core (In Progress)

1. **Constitutional documentation alignment**: establish single truth across Level 0, 1, and 2 documents (Completed in `AGENTS.md`, `24_DECISIONS.md`, `02_ARCHITECTURE.md`, `04_NETWORK_LIFECYCLE.md`, `VALIDATION.md`, and core domain specs).
2. **State & transport cleanup**: completely eliminate obsolete bootstrap Identity, consensus grants, `CAP_BOOTSTRAP`, and legacy state decoders; establish clean `CYBOU_STATE_VERSION = 11` (Completed in code HEAD `0437427`).
3. **Compiled official network definition** (Completed `ec76ef3`, `d5d90cb`): one `OfficialNetwork` built from the compiled Network Public Key, signed `NetworkGenesis`, initial state and bootstrap locators; CYG1/CYN1 and external network files are removed.

## Phase 2 — Network identity and bootstrap transition

4. **DEVNET provisioning** (Re-provisioned with a new Network key in `ec76ef3`): gitignore `/private/`; generate Network and ordinary `cybou.cybou` Identity secret material once; generate and compile only the public Network Key, signed genesis, initial state, Identity and PoA key data. MAINNET stays unprovisioned and GUI-disabled.
5. **One official startup path** (Completed `ec76ef3`): select compiled DEVNET constants, verify signature/state root, and remove external official CYG1/CYN1 loaders, profile digest pins and `--network` file startup.
6. **NetworkID transition** (Completed `66d5fd3`): NetworkID is the exact Network Public Key; 32-byte fields use `ComputeNetworkBinding(key)`.
7. **Bootstrap conversion**: legacy bootstrap binding/protocol/store and the standalone executable are removed (`ec76ef3`); nodes dial the compiled locator with its SPKI pin; remaining: transition the DEV VPS to an ordinary headless `cybou` node after coordinated state reset.
8. **Canonical AUTH state** (Completed in `393657f`, `52c5406`): AUTH in AccountState and GenesisAllocation, state v11, AuthorityIndex removed, desktop reads AUTH directly.
9. **DEVNET acceptance**: verify desktop, ordinary peers, finality, operation relay and storage end to end.

## Phase 3 — AUTH and Validation

- **A — AUTH transitions** (Completed `99af6db`, `4a393fd`): +1 AUTH per finalized Identity-authorized operation; one PoA-signed `PoaAuthAdjustment` operation (GRANT / BURN, floor 0).
- **B — Full-node independent candidate execution** (Completed `7f447d4`): move `OperationPool` from `CybouFinalizerNode` into `CybouNodeRuntime`; every node executes candidates before relay; PoA produces blocks from the same pool.
- **C — Validation signatures** (Completed `c9422a8`, `f7f894f`, `037e6b0`): `ValidationAttestation` (NetworkBinding, OperationID, finalized base BlockID, AccountID, Authorization signature), local signing when AUTH > 1,000,000, bounded Validation store, `VALIDATION_ATTESTATION` CYP2 gossip.
- **D — UI** (Wallet completed `885986f`): AUTH, Validation eligibility, Submitted / Validated · N / Finalized; Mail/Files publication jobs and a PoA adjustment action remain.
- **E — Hardening**: evidence for invalid Validation and a frozen AUTH penalty table.

## Phase 4 — Product integration and end-to-end acceptance

13. **Application data plane integration**: connect Mail and Files publication and finalized storage placement to the desktop model.
14. **Storage durability hardening**: verify Beta target of 2 independent remote full replicas plus local copy (3 physical copies total) with audit and repair.
15. **MAINNET provisioning and launch**: create its own keys, genesis and bootstrap only after full DEVNET soak and formal acceptance.
