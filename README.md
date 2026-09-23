# CYBOU

> **CYBOU** — sovereign decentralized communication infrastructure, designed in France.
> *Infrastructure souveraine de communication décentralisée, conçue en France.*

CYBOU is an open engineering project for sovereign peer-to-peer communication infrastructure. Email is its first application layer, not the boundary of the platform.

CYBOU is being built around a hardened C++ peer-to-peer engine, deterministic state transitions, a target multi-validator BFT consensus, and a target hybrid post-quantum cryptographic profile. BFT finality and complete MailTx encryption are not yet deployed production guarantees. The first application layer is **CYBOU Email**, with Object Storage, Backup and Drive planned on the same infrastructure.

---

## The Vision: European Digital Sovereignty

Modern communication infrastructure is fragile and centralized. European organizations, enterprises, and citizens depend overwhelmingly on foreign cloud platforms, exposing sensitive correspondence to extraterritorial jurisdiction, metadata surveillance, and single-point-of-failure outages.

CYBOU establishes a fundamentally different foundation:

- **Zero Foreign Cloud Dependency**: No mandatory foreign identity providers, proprietary authentication hubs, or external mailbox hosts.
- **Client-Side Key Authority**: Cryptographic keys are generated and held exclusively on user devices. There is no master key, back door, or operator override capable of decrypting user messages or seizing user balances.
- **Autonomous Continuity**: While commercially guided by the CYBOU operator, the network is decentralized and independently operable. It continues validating transactions and delivering messages even in the absence of a centralized operational server.
- **Verifiable Provenance**: Every message carries mathematical evidence of origin, sender signature, and network timestamping, creating tamper-evident records without relying on centralized intermediaries.

---

## Flagship Product: CYBOU Email

CYBOU Email is not a chat application dressed in an email skin, nor is it a repackaged SMTP relay. It is a native asynchronous protocol designed from the ground up to fit a decentralized peer-to-peer state machine.

### Key Characteristics

- **First-Class Protocol Operation**: Messages are processed as native `MailTx` transactions directly within the CYBOU peer-to-peer protocol, never hidden inside secondary metadata channels or arbitrary script payloads.
- **Target Post-Quantum Hybrid Profile**: The MailTx design targets hybrid ML-KEM-768 + X25519 encryption. Full MailTx integration, wire-profile freeze and external security review remain required before any production post-quantum claim.
- **Asynchronous Delivery by Design**: Recipients do not need to be online when an email is dispatched. Senders receive finalized cryptographic proof of inclusion, and recipients retrieve and decrypt their mail upon reconnecting.
- **Local Mailbox Indexing**: User inboxes, sent items, read states, and thread hierarchies are indexed and managed locally by the client application. The consensus network stores only validation counters and cryptographic commitments, preventing state bloat.
- **V1 Scope**: The initial release focuses on ultra-secure, text-only, single-recipient correspondence. Attachments and large-scale bulk storage will be introduced alongside the decentralized CYBOU Object Storage layer.

---

## Core Network Architecture

CYBOU derives from a hardened C++ full-node codebase, heavily refactored to eliminate legacy Proof-of-Work mining, address balkanization, and unnecessary multi-process overhead.

### Permissioned BFT Consensus
The target consensus model gives finalized blocks explicit, irreversible finality through an operator-approved Byzantine Fault Tolerant engine. The multi-validator engine is not implemented yet.
- All active validators have equal voting weight.
- Operates with a single validator in local development, requiring a minimum of four independent validators to ensure fault tolerance in production ($f=1$).
- Eliminates energy-intensive mining, block reorganizations, and probabilistic settlement delays.

### Proof of Trust (PoT) Epochs
Consensus timing and operational quotas are governed by Proof of Trust epochs derived deterministically from finalized block height.
- Consensus logic uses integer arithmetic exclusively, completely eliminating vulnerabilities related to floating-point nondeterminism or local system wall-clock manipulation.
- Newly created accounts receive an operational baseline quota (such as 25 outgoing emails per epoch) to maintain network throughput while deterring spam bursts.

### Pre-Store Retention Strategy
To enable mass adoption on consumer devices without prohibitive hardware requirements:
- Desktop full nodes retain recent block data and are permitted to prune historical transaction bodies.
- Active network validators retain the required pre-Store message history to guarantee synchronization for offline recipients.
- Long-term archiving and multimedia attachments will migrate to dedicated, cooperative encrypted Object Storage as the network scales.

---

## Predictable Economics & Onboarding

CYBOU replaces volatile transaction fee markets with deterministic, predictable economic routing:

- **Fixed Maximum Supply**: The total money supply is hard-capped at 100,000,000,000 CYBOU with zero decimal places (atomic units only).
- **Zero Priority Fee Bidding**: Transaction priority fees are disabled. Senders cannot bid against each other to crowd out normal communication; fees are strictly size-aware and predictable.
- **Deterministic Fee Flow**: Transaction fees are automatically split at the protocol level: 3 parts are allocated to network security and validator rewards, and 1 part is recycled into the onboarding pool.
- **Dual Balance Accounting**: User accounts distinguish between transferable liquid Balance and non-withdrawable System Balance (used specifically for protocol operations and message fees).
- **Permissionless Onboarding with Anti-Sybil PoW**: Creating an identity is protocol-native and permissionless. New accounts include proof-of-work (`AccountCreationWorkV1`) to prevent spam, and automatically receive an onboarding bonus credited from the Onboarding Pool into System Balance. No operator invites, vouchers, or central approval are needed.

---

## Roadmap & Horizons

The development of CYBOU follows a disciplined, phased progression:

1. **Architecture Hardening (Current Stage)**: Network quarantine, deterministic state serialization, permissionless account creation anti-Sybil gates, and desktop client hardening (`cybou.exe`).
2. **CYBOU Email Beta**: Full local mailbox management (Inbox, Sent, Drafts, Threads), hybrid post-quantum encryption pipeline, and multi-validator BFT testnets.
3. **Decentralized Object Storage**: Cooperative, encrypted peer-to-peer storage nodes providing resilient hosting for email attachments, verified backups, and large payloads.
4. **CYBOU Drive & Ecosystem**: Encrypted sovereign file storage, enterprise administrative controls, and pilot deployments for European institutions, legal practitioners, and businesses requiring guaranteed data confidentiality.

---

## Documentation & Getting Started

Comprehensive technical documentation is maintained within this repository:

- **Architecture & Specifications**: Read [`docs/cybou/`](docs/cybou/) for detailed system blueprints, cryptography profiles, and protocol definitions.
- **Documentation Index**: Consult [`doc/README.md`](doc/README.md) for the complete developer technical directory.
- **Implementation Authority**: Review [`AGENTS.md`](AGENTS.md) for mandatory invariants governing consensus, fees, and cryptographic rules.
- **Building from Source**: Follow [`INSTALL.md`](INSTALL.md) for step-by-step compilation guides on Windows (MSVC) and Linux.
- **Contributing**: Refer to [`CONTRIBUTING.md`](CONTRIBUTING.md) for pull request standards, coding guidelines, and review requirements.

---

## Provenance and License

CYBOU is derived from Bitcoin Core and incorporates custom sovereign protocols, consensus engines, and application layers designed in France.

Historical upstream source files retain their original copyright notices and are distributed under the MIT license in [`COPYING`](COPYING). Upstream baseline provenance is documented in [`NOTICE.md`](NOTICE.md) and [`docs/cybou/UPSTREAM_BASELINE.md`](docs/cybou/UPSTREAM_BASELINE.md).
