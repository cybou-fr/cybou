# CYBOU

> **One identity. Private communication. Your data under your control.**<br>
> *Une identité unique. Des communications privées. Vos données sous votre contrôle.*

CYBOU is an experimental protected communication platform built around personal digital identity. Its Beta target brings together verified identity, private messaging, encrypted Files, and service funding; Backup is a post-Beta application.

> **Not a blockchain with features — a protected identity with services.**

> **Protocol status:** `main` targets genesis-bound hybrid-PQ PoA and generic
> RootPublication over encrypted chunks. The current DEV chain is a pre-cutover
> deployment and will be discarded only after the integration gates pass. PoA
> is centralized finalization and does not provide Byzantine fault tolerance;
> see [the protocol decision](docs/cybou/24_DECISIONS.md) and [cutover gates](docs/cybou/26_IMPLEMENTATION_STATUS.md).

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
- **Single security and recovery model:** A 24-word recovery phrase restores one account-level Identity; there is no device registry in the target protocol.
- **Post-quantum security target:** Identity authorization/recovery, Identity KEM, and PoA finality use separate key roles. Production signatures require the configured hybrid classical + PQ components; no classical-only fallback is permitted.
- **Service-native utility wallet:** Two deterministic balance tiers — `SystemBalance` for protocol services (mail, storage, name registration) and spendable `Balance`. Account onboarding automatically seeds service credits.
- **Sovereign and local-first target:** The client manages Identity keys locally and owns encrypted Mail indexes and portable recovery material. Mail, Files, and Backup use private schemas over one encrypted chunk substrate; distributed durability and product integration remain open.

---

## Architecture & Security Foundation

Behind the user-facing services runs a deterministic, peer-to-peer C++20 engine:

- **Identity & Key Separation:**
  - **Recovery:** Hybrid Ed25519 + ML-DSA-65 (NIST FIPS 204).
  - **Account authorization:** Hybrid Ed25519 + ML-DSA-44; it is account-scoped, not device-scoped.
  - **Identity KEM:** X-Wing draft-05 is the selected DEV source profile; the current chain has not cut over to publish/use it.
  - **PoA finality:** A separate genesis-bound Ed25519 + ML-DSA-65 signing role in the next DEV target.
- **Finality target:** one genesis-bound, hybrid-PQ PoA signer with independent full-node state validation. This is centralized finalization and makes no Byzantine-fault-tolerance claim.
- **Deterministic Economics:**
  - Maximum supply capped at **100,000,000,000 CYBOU** (0 decimals).
  - Fees are deterministic and size-aware: each 4-unit fee routes **3 to Security** and **1 to `OnboardingPool`**.
  - Atomic onboarding: permissionless anti-Sybil proof-of-work credits new accounts with an initial `SystemBalance`.
- **Local Client Integrity:** The Qt desktop client embeds the native C++ runtime directly and independently validates state roots and finalized history rather than relying on trusted RPC gateways.

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

- The current DEV deployment predates the target protocol and remains isolated until the coordinated cutover gate.
- Identities, balances, and state on the DEV network are subject to reset as cryptographic integrations finalize.
- Do not treat DEV tokens or test keys as production assets.

### Documentation & Guides

- [PoA finality target](docs/cybou/POA_FINALITY.md)
- [Encrypted chunk tree target](docs/cybou/ENCRYPTED_CHUNK_TREE.md)
- [Generic RootPublication target](docs/cybou/ROOT_PUBLICATION.md)
- [Finalized chunk storage admission](docs/cybou/STORAGE_ADMISSION.md)
- [Identity discovery and clean-machine recovery](docs/cybou/IDENTITY_DISCOVERY_AND_RECOVERY.md)
- [PoA + chunk-tree machine-readable target](spec/poa_chunk_tree.yaml)
- [Identity-Centric Product UX Contract](docs/cybou/79_IDENTITY_CENTRIC_PRODUCT_UX.md)
- [Identity & Name Registry Architecture](docs/cybou/10_IDENTITY_NAMES.md)
- [Identity Security Substrate](docs/cybou/86_IDENTITY_SECURITY_SUBSTRATE.md)
- [Identity Operation Coordinator](docs/cybou/87_IDENTITY_OPERATION_COORDINATOR.md)
- [Encrypted chunk tree](docs/cybou/ENCRYPTED_CHUNK_TREE.md)
- [Mail UI/UX Contract](docs/cybou/82_MAIL_UI_UX.md)
- [Files UI/UX Contract](docs/cybou/83_STORAGE_UI_UX.md)
- [Mail + Files Architecture Freeze](spec/mail_files_architecture.yaml)
- [Implementation Status & Architecture Audit](docs/cybou/26_IMPLEMENTATION_STATUS.md)
- [Building CYBOU from Source](INSTALL.md)
- [Contribution Guidelines](CONTRIBUTING.md)
- [Security Policy](SECURITY.md)

---

## License

CYBOU is open-source software licensed under the [MIT License](COPYING).
Copyright © 2026 Stanislav Saveliev. Designed in France.
