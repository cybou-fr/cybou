# CYBOU

> **CYBOU** — sovereign decentralized communication infrastructure, designed in France.<br>
> *Infrastructure souveraine de communication décentralisée, conçue en France.*

CYBOU is an open-source C++20 project for secure communication, digital identity, and sovereign online services. It is being built around native state transitions, explicit BFT finality, locally controlled identity keys, and a peer-to-peer network without mandatory centralized account or cloud providers. **CYBOU Email** is the first planned application.

The repository contains working DEV components alongside inherited Bitcoin Core runtime code and unfinished product services. CYBOU is **not production-ready**.

## What we are building

The intended shared CYBOU network supports:

| Service | Current boundary |
| --- | --- |
| **Identity** | Protocol-native `AccountCreateOpV1`, anti-Sybil work, local Ed25519 identity key handling, and DEV creation flow. Human-readable `.cybou` aliases such as `stan.cybou` are a target, not a live name registry. |
| **Email** | Native, bounded, one-recipient Mail operation; deterministic fees, discovery filters, and evidence code exist in core. End-to-end encryption, delivery, and usable local Inbox/Sent indexes are incomplete. |
| **Wallet** | Canonical Balance and System Balance transitions, payments, and fee routing exist in core. The desktop wallet UI is capability-gated while live paths are integrated. |
| **Storage** | Protocol and desktop UI design exist; distributed object placement and retrieval are not operational. |
| **Backup** | Desktop UI and recovery design exist; decentralized backup service is not operational. |

These services are intended to use the same verified CYBOU state rather than unrelated centralized accounts. Drive follows Object Storage and Backup on the roadmap.

## CYBOU Email

CYBOU Email is designed as native asynchronous messaging, without requiring an SMTP gateway or a centralized mailbox provider. Mail is a first-class CYBOU operation, never an arbitrary Bitcoin Script payload. Version 1 is one-recipient, text-only, size-bounded, and has no attachments or priority-fee bidding.

Core code validates Mail operations, calculates size-aware fees, persists compact recipient discovery filters, and constructs evidence from inclusion proofs, BFT finality certificates, historical sender-key authorization, and salted content commitments. A complete encrypted send/receive flow and local Inbox/Sent/read-state indexes still need integration. The target hybrid ML-KEM-768 + X25519 encryption profile is not a production security claim.

Attachments and large objects are deferred until CYBOU Object Storage exists. Until then, active validators must retain the required canonical pre-Store Mail history; desktop nodes may prune according to protocol rules.

## Network and consensus

The native CYBOU state path has validator sets, typed operations, deterministic execution, blocks, and verifiable BFT finality certificates. `cybou-node` can run a **single-validator DEV Authority Mode** producer (`f=0`), accept bounded remote operation submissions, and serve finalized blocks to verifying observers. The Qt desktop uses the native runtime to follow DEV finality and to submit identity creation operations.

Multi-validator BFT is implemented and tested in the core engine, including validator-set transitions, but independent validator operation, crash-safe consensus, and a production P2P network remain integration work. At least **four equal-weight validators** are required before claiming `f=1` tolerance. The one-request DEV TCP feed and compiled-in bootstrap endpoint do not constitute a decentralized production network.

The inherited Bitcoin bootstrap chain still uses Bitcoin-derived PoW and subsidy semantics. It is separate from the native CYBOU state path and **does not implement CYBOU monetary policy or BFT finality**. CYBOU-DEV is disposable and may be reset as the protocol evolves.

## Desktop client

The Qt 6 desktop application (`cybou.exe`) has Identity, Wallet, Email, Storage, Backup, and Network pages. Its intended data path is:

```text
Qt application → native CYBOU runtime → verified network state
```

The desktop is designed to own a local state store and identity keys. On Windows, the identity keystore uses OS-protected storage. Normal desktop operation is not intended to depend on a REST or JSON-RPC application service. Pages expose live capabilities and keep unavailable service actions disabled; a visible screen alone does not mean its backend service is operational.

## Economics and onboarding

The CYBOU specification fixes `MAX_SUPPLY` at **100,000,000,000 CYBOU**, with **zero decimals**. Native protocol fees are deterministic and route three parts to Security and one part to Onboarding. `SystemBalance` is a service budget; it does not boost the Beta Proof of Trust score.

Account creation is permissionless and requires `AccountCreationWorkV1` anti-Sybil validation plus proof of possession of the initial key. A successful operation atomically moves the network's onboarding bonus from `OnboardingPool` to the new account's `SystemBalance`. There are no operator vouchers or central account activation. DEV is experimental; Beta and Mainnet are planned with separate genesis and economic parameters, and Beta balances will not carry to Mainnet.

## Current status

CYBOU is in native network integration and architecture hardening. Working code includes deterministic state and account operations, local identity creation, a DEV authority producer, BFT finality verification, remote operation submission, verified block synchronization, and Qt runtime integration. Current work focuses on multi-validator operation, native P2P transport, secure key custody, complete Email encryption and delivery, and removal of inherited Bitcoin runtime dependencies.

See [implementation status](docs/cybou/26_IMPLEMENTATION_STATUS.md), the [DEV node runbook](docs/cybou/75_DEV_NODE_RUNBOOK.md), and the [migration inventory](spec/bitcoin_code_removal.yaml) for precise boundaries.

## Documentation and builds

- [CYBOU architecture and protocol documentation](docs/cybou/)
- [Documentation index](doc/README.md)
- [Build entrypoint](INSTALL.md) and [verified Windows MinGW procedure](docs/cybou/71_WINDOWS_MINGW_BUILD.md)
- [Contribution guidelines](CONTRIBUTING.md)
- [Implementation authority](AGENTS.md)

## Provenance and license

CYBOU originated from Bitcoin Core and progressively replaces Bitcoin-specific consensus, wallet, networking, and application components with native CYBOU protocols. Historical upstream files retain their original copyright notices. The project is distributed under the MIT license; see [COPYING](COPYING), [NOTICE.md](NOTICE.md), and [upstream baseline](docs/cybou/UPSTREAM_BASELINE.md).

**CYBOU-DEV is experimental. Do not treat development identities, balances, validator keys, or network state as production assets.**
