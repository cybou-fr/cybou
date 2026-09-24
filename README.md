# CYBOU

> **CYBOU** — European sovereign identity and communication network, designed in France.<br>
> *Réseau européen souverain d'identité et de communication, conçu en France.*

CYBOU is an open-source peer-to-peer network for user-controlled identity,
secure communication, and sovereign online services. The intended node owns
verified state locally and reaches explicit BFT finality without centralized
account or cloud providers. **Identity is the platform primitive; CYBOU Email
is the first product.**

## Architecture

The protocol uses a random, stable 256-bit AccountID. A 24-word recovery phrase controls a hybrid Ed25519 and ML-DSA-65 Recovery Root. Each device has a separate Ed25519 and ML-DSA-44 key, an activation number, and its own operation nonce. Encrypted portable vaults hold local recovery and device material.

The native core and Qt identity flow now create random AccountIDs, save a portable CYBV2 vault before AccountCreate, and restore a device from 24 words through a root-authorized operation. The running DEV network has not completed the full PQ Mail and multi-validator cutover; do not treat its identities or balances as durable assets.

## Services

| Service | Scope |
| --- | --- |
| **Identity** | Local key ownership, permissionless anti-Sybil account creation, recovery, device rotation, and `.cybou` names. |
| **Email** | One-recipient, text-only encrypted MailTx with deterministic size-aware fees and recipient-owned local indexes. |
| **Wallet** | Native balances, SystemBalance service budget, payments, and protocol fee routing. |
| **Storage** | Later encrypted distributed objects; required before large attachments. |
| **Backup** | Later encrypted decentralized backup and recovery. |

The services share one verified CYBOU state. Storage, Backup, and Drive are
gated on demonstrated Identity + Email usage and operational maturity. The
pre-Store Mail design is intentionally bounded for the pilot, not for
mass-scale Email.

## Network and economics

CYBOU uses validator-based BFT finality. Operator-approved admission gives every validator weight one; four validators are required before claiming tolerance of one Byzantine fault. Development Authority Mode has one validator and no such fault tolerance.

The maximum supply is **100,000,000,000 CYBOU**, with zero decimals. For each four units of protocol fees, three go to Security and one to Onboarding. Account creation is permissionless and uses protocol anti-Sybil work. Successful creation funds SystemBalance from the OnboardingPool. DEV, Beta, and Mainnet have separate parameters and genesis state.

## Desktop

The desktop client uses Qt 6 and modern C++. Its intended path is:

```text
Qt application → native CYBOU runtime → peer-to-peer network
```

Identity creation and recovery save and verify the portable vault before submitting an operation. Network results remain pending until verified finality. Name claiming, PQ Mail confidentiality, vault password change, and full device management still need desktop integration.

## Status and documentation

CYBOU is experimental and **not production-ready**. The remaining integration covers Mail encryption and delivery, `.cybou` claims, validator operations, network transport, persistence, crash recovery, and desktop flows. The development network will be reset once those pieces share the canonical protocol format.

- [Protocol and architecture](docs/cybou/)
- [Implementation status](docs/cybou/26_IMPLEMENTATION_STATUS.md)
- [Build instructions](INSTALL.md)
- [Contributing](CONTRIBUTING.md)
- [License](COPYING) and [notices](NOTICE.md)

**Do not treat DEV identities, balances, validator keys, or network state as production assets.**
