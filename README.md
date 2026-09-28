# CYBOU

> **One identity. Private communication. Your data under your control.**<br>
> *Une identité unique. Des communications privées. Vos données sous votre contrôle.*

CYBOU is an experimental protected communication platform built around personal digital identity. Its Beta target brings together verified identity, private messaging, encrypted Files, and service funding; Backup is a post-Beta application.

> **Not a blockchain with features — a protected identity with services.**

---

## Why CYBOU exists

Today, digital life is fractured across competing platform silos:
- One proprietary account for email.
- Another corporate cloud for documents and photos.
- Third-party utilities for device backups and two-factor authentication.
- Fragmented accounts and payment gateways with endless passwords and surveillance.

When platforms change terms, suffer data breaches, or terminate accounts, users lose their contacts, communication history, and digital continuity. 

CYBOU reorganizes digital services around **you**: one cryptographically protected identity that you own completely, from which all essential communication and data services operate.

---

## What makes CYBOU different

- **Identity-centric, not speculation-centric:** The wallet exists to fund services and secure the network, not as a speculative trading instrument. Communication, privacy, and user sovereignty come first.
- **Human-readable `.cybou` names:** Simple addresses such as `stanislav.cybou` or `alice.cybou` replace cumbersome cryptographic strings, resolved directly on a decentralized registry.
- **Single security and recovery model:** A single 24-word recovery phrase protects your identity root. Devices are authorized cryptographically without handing master credentials to a central server.
- **Post-quantum security target:** Identity authorization uses hybrid signatures (Recovery Root Ed25519 + ML-DSA-65; device Ed25519 + ML-DSA-44). Hybrid recipient encryption (X25519 + ML-KEM-768) is a target profile; recipient KEM keys are not yet published and Mail sending fails closed.
- **Service-native utility wallet:** Two deterministic balance tiers — `SystemBalance` for protocol services (mail, storage, name registration) and spendable `Balance`. Account onboarding automatically seeds service credits.
- **Sovereign and local-first target:** The client manages identity keys locally and is designed to own mailbox indexes and encrypted portable vaults. Distributed Object Storage, Beta Mail attachments, and Backup are not yet implemented.

---

## Architecture & Security Foundation

Behind the user-facing services runs a deterministic, peer-to-peer C++20 engine:

- **Identity & Key Separation:**
  - **Recovery Root:** Hybrid Ed25519 + ML-DSA-65 (NIST FIPS 204). Operates offline for recovery and device delegation.
  - **Device Keys:** Hybrid Ed25519 + ML-DSA-44. Unique per device, authorized by the root.
  - **Mail Encryption Keys (target):** Hybrid X25519 + ML-KEM-768, isolated from identity signing keys; the required recipient-key publication and interoperable profile remain unimplemented.
- **BFT Finality:** Deterministic block finality certificates with equal validator weight (`weight = 1`). Development Authority Mode runs with a single validator (`f=0`); public networks target equal-weight consensus requiring $\ge 4$ independent validators for `f=1` tolerance.
- **Deterministic Economics:**
  - Maximum supply capped at **100,000,000,000 CYBOU** (0 decimals).
  - Fees are deterministic and size-aware: each 4-unit fee routes **3 to Security** and **1 to `OnboardingPool`**.
  - Atomic onboarding: permissionless anti-Sybil proof-of-work credits new accounts with an initial `SystemBalance`.
- **Local Client Integrity:** The Qt desktop client embeds the native C++ runtime directly, verifying state roots and finality certificates locally rather than relying on trusted RPC gateways.

---

## Desktop Application

The desktop application is built with **Qt 6** and **modern C++**:

```text
Qt Desktop Interface → Native CYBOU Runtime → Peer-to-Peer Network
```

- Clean, identity-centric user interface ([Product UX Specification](docs/cybou/79_IDENTITY_CENTRIC_PRODUCT_UX.md)).
- Local encrypted vault (`CYBV2`) created and verified prior to network broadcast.
- Zero tracking, telemetry, or remote dependency injection.

---

## Status and Transparency

CYBOU is in active development and **not yet a public production service**. 

- The current DEV network operates in single-validator Development Authority Mode for core functional verification.
- Identities, balances, and state on the DEV network are subject to reset as cryptographic integrations finalize.
- Do not treat DEV tokens or test keys as production assets.

### Documentation & Guides

- [Identity-Centric Product UX Contract](docs/cybou/79_IDENTITY_CENTRIC_PRODUCT_UX.md)
- [Identity & Name Registry Architecture](docs/cybou/10_IDENTITY_NAMES.md)
- [Identity Security Substrate](docs/cybou/86_IDENTITY_SECURITY_SUBSTRATE.md)
- [Identity Operation Coordinator](docs/cybou/87_IDENTITY_OPERATION_COORDINATOR.md)
- [Encrypted Object and Key Model](docs/cybou/88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md)
- [Mail UI/UX Contract](docs/cybou/82_MAIL_UI_UX.md)
- [Files UI/UX Contract](docs/cybou/83_STORAGE_UI_UX.md)
- [Mail + Files Architecture Freeze](spec/mail_files_architecture.yaml)
- [Implementation Status & Architecture Audit](docs/cybou/26_IMPLEMENTATION_STATUS.md)
- [Building CYBOU from Source](INSTALL.md)
- [Contribution Guidelines](CONTRIBUTING.md)
- [Security & Threat Model](docs/cybou/19_SECURITY_THREAT_MODEL.md)

---

## License

CYBOU is open-source software licensed under the [MIT License](COPYING).
Copyright © 2026 Stanislav Saveliev. Designed in France.
