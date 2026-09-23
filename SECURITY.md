# Security Policy — CYBOU

The CYBOU project takes security, cryptographic integrity, and protocol resilience seriously.

---

## 1. Supported Versions

Security updates are actively provided for:

| Version | Status |
|---|---|
| CYBOU v0.0.1 (Development Baseline) | Active development |

---

## 2. Reporting a Vulnerability

If you discover a security vulnerability, cryptographic weakness, or consensus denial-of-service issue in CYBOU, please report it responsibly.

- **Security Email**: `security@cybou.org` *(or contact the designated project operator)*
- **Sensitive Reports**: Encrypt communications using the project's official Release / Security PGP key.
- **Do not disclose publicly**: Please allow the core engineering team sufficient time to investigate, reproduce, and deploy a fix before disclosing vulnerabilities publicly.

When reporting, please include:
1. Detailed description of the potential vulnerability or attack vector.
2. Steps to reproduce or proof-of-concept code/test vectors.
3. Impact assessment (e.g., consensus desync, memory exhaustion, state forgery, privilege escalation).

---

## 3. Cryptographic Invariants & Key Architecture

Security reviewers and contributors should note the mandatory cryptographic gates established in CYBOU architecture:

### 4-Way Operator Key Separation
As specified in [`docs/cybou/68_OPERATOR_KEY_SEPARATION.md`](docs/cybou/68_OPERATOR_KEY_SEPARATION.md), the operator maintains four strictly segregated keys:
- **Operator Authority Key**: Signs one-time invite vouchers and validator admission proposals.
- **Operator Validator Key**: Signs BFT block proposals and finality votes.
- **Release Signing Key**: Signs binary releases and package manifests.
- **Treasury Key**: Manages operational balances and funding reserves.

*Compromise of one key must never implicitly compromise the authority or functions of another.*

### Invite Voucher and State Transition Gates
Per [`docs/cybou/70_INVITE_VOUCHER_SIGNING_GATE.md`](docs/cybou/70_INVITE_VOUCHER_SIGNING_GATE.md):
- Welcome Grants (6,000 CYBOU) are redeemable strictly once per `AccountId`.
- Vouchers must include domain separation, network identifier (`network_id`), and beneficiary binding to prevent cross-account theft or cross-network replay.
- Deserialization enforces strict integer bounds and error handling.

### End-to-End Encryption
Mail contents are protected by hybrid Post-Quantum HPKE profiles ([`docs/cybou/49_EMAIL_E2EE_HPKE_PQ.md`](docs/cybou/49_EMAIL_E2EE_HPKE_PQ.md)) and salted content commitments ([`docs/cybou/69_MAIL_EVIDENCE_BUNDLE.md`](docs/cybou/69_MAIL_EVIDENCE_BUNDLE.md)).
