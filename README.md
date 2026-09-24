# CYBOU

> **CYBOU** — sovereign decentralized communication infrastructure, designed in France.<br>
> *Infrastructure souveraine de communication décentralisée, conçue en France.*

CYBOU is an open-source peer-to-peer platform for secure communication, digital identity, and sovereign online services. The intended node owns verified state locally and reaches explicit BFT finality without centralized account or cloud providers. **CYBOU Email** is the first application.

## Architecture

The protocol uses a random, stable 256-bit AccountID. A 24-word recovery phrase controls a hybrid Ed25519 and ML-DSA-65 Recovery Root. Each device has a separate Ed25519 and ML-DSA-44 key, an activation number, and its own operation nonce. Encrypted portable vaults hold local recovery and device material.

The current core contains canonical identity registration, account creation, payment authorization, state snapshots, and candidate block execution for this cryptographic profile. These components are being connected to block encoding, persisted state, transport, and the Qt desktop. The running DEV node has not completed that cutover; do not treat its identities or balances as durable assets.

## Services

| Service | Scope |
| --- | --- |
| **Identity** | Local key ownership, permissionless anti-Sybil account creation, recovery, device rotation, and `.cybou` names. |
| **Email** | One-recipient, text-only encrypted MailTx with deterministic size-aware fees and recipient-owned local indexes. |
| **Wallet** | Native balances, SystemBalance service budget, payments, and protocol fee routing. |
| **Storage** | Encrypted distributed objects; required before large attachments. |
| **Backup** | Encrypted decentralized backup and recovery. |

The services share one verified CYBOU state. Storage and Backup remain designs rather than operational network services.

## Network and economics

CYBOU uses validator-based BFT finality. Operator-approved admission gives every validator weight one; four validators are required before claiming tolerance of one Byzantine fault. Development Authority Mode has one validator and no such fault tolerance.

The maximum supply is **100,000,000,000 CYBOU**, with zero decimals. For each four units of protocol fees, three go to Security and one to Onboarding. Account creation is permissionless and uses protocol anti-Sybil work. Successful creation funds SystemBalance from the OnboardingPool. DEV, Beta, and Mainnet have separate parameters and genesis state.

## Desktop

The desktop client uses Qt 6 and modern C++. Its intended path is:

```text
Qt application → native CYBOU runtime → peer-to-peer network
```

Identity creation and restore must save and verify the portable vault before submitting an operation. Network results are shown as pending until verified finality. The desktop cutover to the PQ identity and state path remains in progress.

## Status and documentation

CYBOU is experimental and **not production-ready**. The remaining integration covers Mail encryption and delivery, `.cybou` claims, validator operations, network transport, persistence, crash recovery, and desktop flows. The development network will be reset once those pieces share the canonical protocol format.

- [Protocol and architecture](docs/cybou/)
- [Implementation status](docs/cybou/26_IMPLEMENTATION_STATUS.md)
- [Build instructions](INSTALL.md)
- [Contributing](CONTRIBUTING.md)
- [License](COPYING) and [notices](NOTICE.md)

**Do not treat DEV identities, balances, validator keys, or network state as production assets.**
