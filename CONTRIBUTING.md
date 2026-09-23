# Contributing to CYBOU

Thank you for your interest in contributing to **CYBOU**, a sovereign European peer-to-peer network designed in France.

All contributors—human developers and autonomous AI coding agents alike—must strictly follow the project's architectural invariants and governance principles.

---

## 1. Ground Rules & Authority

Before proposing or implementing any changes, you **must read and adhere to**:

1. **[`AGENTS.md`](./AGENTS.md)** — The absolute implementation authority for CYBOU. Hard rules defined there override any unvetted conventions.
2. **[`docs/cybou/`](./docs/cybou/)** — The architectural design documents covering BFT consensus, MailTx protocol, deterministic fee routing, Proof of Trust (PoT), and permissionless onboarding.
3. **[`MANIFEST.md`](./MANIFEST.md)** — The cryptographic inventory of authoritative documents.

### Invariant Rules (Summary from `AGENTS.md`)

- **CYBOU Email & MailTx**:
  - `MailTx` is a first-class operation, not an `OP_RETURN` or script payload.
  - Text-only, single-recipient in v1. No attachments on-chain.
  - No permanent per-mail consensus-state object (state stores validation-relevant counters/roots only).
  - Local clients own mailbox indexing; active validators retain pre-Store history until Object Storage is deployed.
- **Consensus & Time**:
  - Permissioned BFT explicit finality with operator-approved admission.
  - Equal validator weight = 1 (minimum 4 validators for $f=1$ tolerance).
  - Proof of Trust (PoT) epochs are derived strictly from finalized block height (integer arithmetic only; wall clock never dictates consensus).
- **Economics & Onboarding**:
  - Fixed supply: $100,000,000,000$ CYBOU ($0$ decimals).
  - Deterministic fee router: 4 CYBOU fees $\to$ 3 Security + 1 Onboarding. Priority fee bidding is disabled.
  - Account creation is protocol-native and permissionless via `AccountCreateOpV1`, protected by `AccountCreationWorkV1` anti-Sybil proof-of-work. An automatic onboarding bonus is debited directly from `OnboardingPool` to `SystemBalance` without operator vouchers or invites.
- **Operator Key Separation**:
  - Separate keys for Operator Authority, Operator Validator, Release Signing, and Treasury. Operator Authority does NOT participate in ordinary account creation.

---

## 2. Development Workflow

### Branching and Commits

- Work on dedicated feature branches branched off `main`.
- Commit messages should be concise, descriptive, and reference the component being modified (e.g., `cybou/state: prevent double-claiming welcome grant`).
- Keep commits atomic and logically separated.

### Coding Standards

- **Language Standard**: C++20.
- **Deterministic Logic**: Consensus, state transition, and fee calculations must use integer arithmetic only. Floating point operations are strictly prohibited in consensus code.
- **Defensive Error Handling**: Deserialization routines and network parsers must enforce strict bounds to prevent buffer overflows or memory exhaustion.
- Refer to [`doc/developer-notes.md`](doc/developer-notes.md) for detailed C++ style and naming conventions.

---

## 3. Testing and Verification

Every code change must be accompanied by appropriate automated tests:

- **Unit Tests**: Add test cases to `src/test/` for all new state machine logic, serialization routines, and cryptographic verifications.
- **Running Tests**:
  ```powershell
  ctest --test-dir build --output-on-failure
  ```
- **Benchmarks & Fuzzers**: Changes to serialization and parsers should include or update fuzz targets in `src/test/fuzz/` to verify robustness.

---

## 4. Pull Request & Review Process

1. **Self-Review**: Run unit tests and ensure code formats cleanly before submitting.
2. **Review Criteria**:
   - Does the change conform to `AGENTS.md` and related `docs/cybou/` specifications?
   - Are edge cases and bounds checks handled safely?
   - Is deterministic consensus behavior preserved?
3. **Approval**: Changes affecting consensus, fee routing, or cryptographic verification require review by core project maintainers.
