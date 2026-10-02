# CYBOU architecture

CYBOU is an Identity-centered private Mail and Files platform over one
content-addressed encrypted P2P substrate.

## Layers

```text
Qt GUI
  |
  v
Identity Application DB / product model
  |
  v
ApplicationService / PublicationService / StorageService
  |
  v
native CYBOU NodeRuntime
  + canonical state execution
  + single-operator hybrid-PQ PoA finality (Authority chain K0 -> K1 -> ...)
  + P2P mesh synchronization and operation relay
  + RootPublication
  + common encrypted ChunkStore
```

`APPLICATION_DATA_PLANE.md` defines the local/network data boundary.
`04_NETWORK_LIFECYCLE.md` defines the official network lifecycle, bootstrap rendezvous,
network replacement, and Authority rotation.

## Documentation hierarchy

CYBOU architecture adheres to a strict hierarchy of authority. Lower levels
cannot introduce protocol mechanics absent from higher levels:
- **Level 0 (Implementation authority)**: `AGENTS.md`
- **Level 1 (Frozen architecture / decisions)**: `docs/cybou/24_DECISIONS.md`, `docs/cybou/02_ARCHITECTURE.md`
- **Level 2 (Normative domain documents)**: `04_NETWORK_LIFECYCLE.md`, `05_CHAIN_STATE.md`, `08_P2P.md`, `POA_FINALITY.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, `10_IDENTITY_NAMES.md`, `18_ECONOMICS_FEES.md`, `57_IDENTITY_AUTHORITY.md`
- **Level 3 (Mutable implementation truth)**: `docs/cybou/26_IMPLEMENTATION_STATUS.md`
- **Level 4 (Roadmap / unresolved work)**: `docs/cybou/22_ROADMAP.md`, `docs/cybou/25_OPEN_QUESTIONS.md`
- **Level 5 (Product / UX)**: `docs/cybou/81`–`85`, `APPLICATION_DATA_PLANE.md`
- **Level 6 (Sovereignty / legal / strategy)**: `docs/cybou/37`–`43`
- **Level 7 (Machine-readable mirrors)**: `spec/*`
- **Level 8 (Public projection)**: `README.md`, `www/*`, `www/llms.txt`

## Node capabilities and peer admission

Every participant runs the same full-node core software.

Optional operational capabilities:
- **Storage**: admits and serves authorized encrypted chunks.
- **Central Authority / PoA**: orders transactions and finalizes blocks.

**Bootstrap** is a known rendezvous service that distributes signed official
network state and seeds initial peer discovery. It is not a consensus role or Identity entity.

Optional advisory Validation is non-canonical future functionality and does
not participate in network operation or finality.

Public P2P admission is France-only for inbound and outbound connections across
all capabilities. Policy rules and local fail-closed Geo enforcement are
detailed in [`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`](37_FRANCE_SOVEREIGN_NETWORK_POLICY.md).

## Identity

One AccountID is the stable Identity. Mnemonic-derived Recovery, Authorization,
and KEM roles are separate. Device is not a protocol entity.

Storage providers prove their own service keys per CYP2 session. The PoA finalizer
proves the current Authority key ($K_{\text{epoch}}$). There is no canonical service-node registry.

## Finality

Single-operator hybrid-PQ PoA finalizes blocks. Genesis establishes the initial
Authority key $K_0$, which may rotate monotonically ($K_0 \to K_1 \to \dots$).
Every full node independently verifies PoA certificates, operation validity,
and state transitions. Anti-equivocation journaling and fail-closed halt protect against conflicting blocks.
There is no BFT or validator quorum.

## Application content and storage

`RootPublication` is the only application-content protocol operation.
Mail and Files share the same encrypted content substrate.
ChunkID is the full BLAKE3-256 digest of stored encrypted bytes.
Remote chunk admission is finality-first: authorized chunks are admitted only
after their RootPublication is finalized.
Development targets 1 remote full replica; Beta targets 2 independent remote full replicas.

## Application projection

Each unlocked Identity maintains a private encrypted rebuildable Application DB.
The GUI renders this semantic projection and never browses provider storage directly.

## Derived Authority metric

Authority is an informational, read-only metric derived from finalized account
history. It is non-transferable and grants no protocol or PoA power. See
[`57_IDENTITY_AUTHORITY.md`](57_IDENTITY_AUTHORITY.md).
