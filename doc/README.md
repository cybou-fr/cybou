# CYBOU Documentation Hub

Welcome to the technical documentation for **CYBOU**. Start with the
[current implementation status](../docs/cybou/26_IMPLEMENTATION_STATUS.md):
the native DEV state path and the inherited Bitcoin bootstrap runtime are
distinct, and only the former has CYBOU BFT finality.

CYBOU is a commercially operated European sovereign peer-to-peer network designed in France.
Its first planned product is **CYBOU Email**. Native Mail operations, BFT
certificates, PoT epochs, and deterministic fees exist in core, while complete
encrypted Email delivery and production multi-validator operation remain open.

---

## Authoritative Documentation (`docs/cybou/`)

The primary architecture, protocol requirements, and design choices are documented in [`docs/cybou/`](../docs/cybou/):

### Core Architecture & Strategy
- [`00_VISION.md`](../docs/cybou/00_VISION.md) — Product and sovereignty vision
- [`01_BASELINE_AND_SCOPE.md`](../docs/cybou/01_BASELINE_AND_SCOPE.md) — Upstream baseline and product scope
- [`02_ARCHITECTURE.md`](../docs/cybou/02_ARCHITECTURE.md) — NodeCore single-process architecture
- [`03_BITCOIN_DERIVATION.md`](../docs/cybou/03_BITCOIN_DERIVATION.md) — Derivation rules from Bitcoin Core
- [`04_NETWORK_QUARANTINE.md`](../docs/cybou/04_NETWORK_QUARANTINE.md) — Network quarantine and port isolation
- [`26_IMPLEMENTATION_STATUS.md`](../docs/cybou/26_IMPLEMENTATION_STATUS.md) — Current code boundary and remaining integration
- [`73_CORE_DESKTOP_CONTRACT.md`](../docs/cybou/73_CORE_DESKTOP_CONTRACT.md) — Native runtime to Qt desktop contract
- [`74_AUTHORITY_BLOCK_FEED.md`](../docs/cybou/74_AUTHORITY_BLOCK_FEED.md) — DEV producer and bounded transport
- [`75_DEV_NODE_RUNBOOK.md`](../docs/cybou/75_DEV_NODE_RUNBOOK.md) — Standalone DEV node and observer

### Consensus & Network Time
- [`07_BFT_CONSENSUS.md`](../docs/cybou/07_BFT_CONSENSUS.md) — Permissioned BFT explicit finality
- [`66_BFT_VALIDATOR_SET_HARDENING.md`](../docs/cybou/66_BFT_VALIDATOR_SET_HARDENING.md) — Equal validator weight, minimum $f=1$ set (4 validators)
- [`57_GLOBAL_PROOF_OF_TRUST_POLICY.md`](../docs/cybou/57_GLOBAL_PROOF_OF_TRUST_POLICY.md) — Deterministic consensus epochs
- [`67_POT_EPOCHS_AND_ONBOARDING.md`](../docs/cybou/67_POT_EPOCHS_AND_ONBOARDING.md) — Epoch formulas and onboarding

### CYBOU Email & MailTx
- [`16_MAIL_PROTOCOL.md`](../docs/cybou/16_MAIL_PROTOCOL.md) — First-class MailTx operation
- [`17_EMAIL.md`](../docs/cybou/17_EMAIL.md) — Email delivery and UX specification
- [`34_MAIL_STATE_AND_RETENTION.md`](../docs/cybou/34_MAIL_STATE_AND_RETENTION.md) — Pre-Store validator retention and desktop pruning
- [`62_MAIL_REGISTRATION_AND_PROOF_MODEL.md`](../docs/cybou/62_MAIL_REGISTRATION_AND_PROOF_MODEL.md) — Registration and proof model
- [`65_MAILTX_PROTOCOL_OPERATION.md`](../docs/cybou/65_MAILTX_PROTOCOL_OPERATION.md) — Wire serialization and execution
- [`69_MAIL_EVIDENCE_BUNDLE.md`](../docs/cybou/69_MAIL_EVIDENCE_BUNDLE.md) — Inclusion proofs, finality certificates, and salted commitments

### Economics & Onboarding
- [`18_ECONOMICS_FEES.md`](../docs/cybou/18_ECONOMICS_FEES.md) — Fee model: 4 CYBOU fees (3 Security + 1 Onboarding)
- [`52_BALANCE_AND_SYSTEM_BALANCE.md`](../docs/cybou/52_BALANCE_AND_SYSTEM_BALANCE.md) — Liquid Balance vs. non-withdrawable System Balance
- [`54_EMISSION_AND_MONETARY_POLICY.md`](../docs/cybou/54_EMISSION_AND_MONETARY_POLICY.md) — Fixed maximum supply: 100,000,000,000 CYBOU (0 decimals)
- [`59_DETERMINISTIC_FEE_ROUTER.md`](../docs/cybou/59_DETERMINISTIC_FEE_ROUTER.md) — Deterministic fee router (priority fees disabled)
- [`70_ACCOUNT_CREATION_ANTI_SYBIL.md`](../docs/cybou/70_ACCOUNT_CREATION_ANTI_SYBIL.md) — Permissionless account creation and anti-Sybil proof-of-work

### Security & Cryptography
- [`09_CRYPTO_PQ.md`](../docs/cybou/09_CRYPTO_PQ.md) — Post-Quantum crypto roadmap
- [`19_SECURITY_THREAT_MODEL.md`](../docs/cybou/19_SECURITY_THREAT_MODEL.md) — Threat model and attack surface
- [`49_EMAIL_E2EE_HPKE_PQ.md`](../docs/cybou/49_EMAIL_E2EE_HPKE_PQ.md) — Hybrid post-quantum HPKE encryption
- [`68_OPERATOR_KEY_SEPARATION.md`](../docs/cybou/68_OPERATOR_KEY_SEPARATION.md) — 4-way key separation (Authority, Validator, Release, Treasury)

---

## Building CYBOU

Build entrypoint: [`INSTALL.md`](../INSTALL.md).

- **Windows (verified local setup)**: [Qt MinGW + vcpkg procedure](../docs/cybou/71_WINDOWS_MINGW_BUILD.md).
- **Linux core CI**: [Native CYBOU workflow](../.github/workflows/cybou-core.yml).
- **Inherited notes**: [MSVC](build-windows-msvc.md), [Unix](build-unix.md), and [dependencies](dependencies.md) still contain transitional Bitcoin assumptions.

---

## Developer Tooling & Testing

Development practices and technical references:

- [Developer Notes](developer-notes.md) — Coding conventions, formatting, and codebase organization.
- [Benchmarking](benchmarking.md) — Microbenchmark framework for consensus and crypto.
- [Fuzz Testing](fuzzing.md) — Fuzz test suite for parsers, serializers, and state machines.
- [Tracing](tracing.md) — Subsystem observability and debug hooks.
- [Productivity Notes](productivity.md) — Helpful developer tips and tooling setups.

---

## Governance & Upstream Attribution

- [`AGENTS.md`](../AGENTS.md) — Hard operational rules for developers and autonomous agents.
- [`MANIFEST.md`](../MANIFEST.md) — SHA-256 manifest of all authoritative CYBOU documents.
- [`NOTICE.md`](../NOTICE.md) — Provenance notice regarding derivation from Bitcoin Core.
- [`COPYING`](../COPYING) — Historical MIT license of upstream Bitcoin Core.
