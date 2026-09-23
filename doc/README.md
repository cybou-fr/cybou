# CYBOU Documentation Hub

Welcome to the internal technical documentation for **CYBOU** (v0.14).

CYBOU is a commercially operated European sovereign peer-to-peer network designed in France.
Its first core product is **CYBOU Email**, built on first-class on-chain MailTx operations, permissioned BFT explicit finality, deterministic Proof of Trust (PoT) epochs, and zero-priority-fee economic routing.

---

## Authoritative Documentation (`docs/cybou/`)

The primary architecture, protocol requirements, and design choices are documented in [`docs/cybou/`](../docs/cybou/):

### Core Architecture & Strategy
- [`00_VISION.md`](../docs/cybou/00_VISION.md) — Product and sovereignty vision
- [`01_BASELINE_AND_SCOPE.md`](../docs/cybou/01_BASELINE_AND_SCOPE.md) — Upstream baseline and product scope
- [`02_ARCHITECTURE.md`](../docs/cybou/02_ARCHITECTURE.md) — NodeCore single-process architecture
- [`03_BITCOIN_DERIVATION.md`](../docs/cybou/03_BITCOIN_DERIVATION.md) — Derivation rules from Bitcoin Core
- [`04_NETWORK_QUARANTINE.md`](../docs/cybou/04_NETWORK_QUARANTINE.md) — Network quarantine and port isolation

### Consensus & Network Time
- [`07_BFT_CONSENSUS.md`](../docs/cybou/07_BFT_CONSENSUS.md) — Permissioned BFT explicit finality
- [`66_BFT_VALIDATOR_SET_HARDENING.md`](../docs/cybou/66_BFT_VALIDATOR_SET_HARDENING.md) — Equal validator weight, minimum $f=1$ set (4 validators)
- [`57_GLOBAL_PROOF_OF_TRUST_POLICY.md`](../docs/cybou/57_GLOBAL_PROOF_OF_TRUST_POLICY.md) — Deterministic consensus epochs
- [`67_POT_EPOCHS_AND_INVITE_VOUCHERS.md`](../docs/cybou/67_POT_EPOCHS_AND_INVITE_VOUCHERS.md) — Epoch formulas and voucher gates

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
- [`70_INVITE_VOUCHER_SIGNING_GATE.md`](../docs/cybou/70_INVITE_VOUCHER_SIGNING_GATE.md) — One-time 6,000 CYBOU Welcome Grant redemption gate

### Security & Cryptography
- [`09_CRYPTO_PQ.md`](../docs/cybou/09_CRYPTO_PQ.md) — Post-Quantum crypto roadmap
- [`19_SECURITY_THREAT_MODEL.md`](../docs/cybou/19_SECURITY_THREAT_MODEL.md) — Threat model and attack surface
- [`49_EMAIL_E2EE_HPKE_PQ.md`](../docs/cybou/49_EMAIL_E2EE_HPKE_PQ.md) — Hybrid post-quantum HPKE encryption
- [`68_OPERATOR_KEY_SEPARATION.md`](../docs/cybou/68_OPERATOR_KEY_SEPARATION.md) — 4-way key separation (Authority, Validator, Release, Treasury)

---

## Building CYBOU

Supported compilation targets for CYBOU v0.14:

- **Windows (MSVC)**: [Windows MSVC Build Notes](build-windows-msvc.md) — Primary desktop build.
- **Linux / Unix**: [Unix Build Notes](build-unix.md) — Infrastructure and validator node build.
- **Dependencies**: [Dependencies](dependencies.md) — External third-party libraries and requirements.

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
