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
  + single-operator hybrid-PQ PoA finality (genesis-authorized P)
  + provisional Validation evaluation (Authority > 1,000,000)
  + P2P mesh synchronization and operation relay
  + RootPublication
  + common encrypted ChunkStore
```

`APPLICATION_DATA_PLANE.md` defines the local/network data boundary.
`04_NETWORK_LIFECYCLE.md` defines official network trust, creation, joining,
and network replacement.

## Official network trust

```text
Compiled Network Public Key (NetworkID) + Bootstrap IP:port & TLS SPKI pin
  -> Offline-signed genesis (defines network parameters, initial Authority, PoA P)
  -> Ordinary bootstrap peer seeds initial CYP2 discovery
  -> Direct P2P mesh
  -> Provisional Validation (optional pre-finalization by eligible Identities)
  -> Central Authority P signs finalized blocks (absolute canonical truth)
```

Bootstrap is an ordinary CYBOU full peer with a known locator; it distributes
the signed genesis and initial peer hints. The Network Private Key is strictly
offline and used solely by the network owner to sign genesis specifications.
The Central Authority operates the genesis-authorized PoA key `P` and finalizes
blocks.

Every full node independently validates blocks, operation validity, and state
transitions. Provisional Validation provides optional pre-finalization evidence
under local policy; if Validation conflicts with PoA, provisional state is
discarded, provisional effects are rolled back, and the PoA-finalized state is
adopted unconditionally.

## Documentation hierarchy

CYBOU architecture adheres to a strict hierarchy of authority. Lower levels
cannot introduce protocol mechanics absent from higher levels:
- **Level 0 (Implementation authority)**: `AGENTS.md`
- **Level 1 (Frozen architecture / decisions)**: `docs/cybou/24_DECISIONS.md`, `docs/cybou/02_ARCHITECTURE.md`
- **Level 2 (Normative domain documents)**: `04_NETWORK_LIFECYCLE.md`, `05_CHAIN_STATE.md`, `08_P2P.md`, `POA_FINALITY.md`, `VALIDATION.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, `10_IDENTITY_NAMES.md`, `18_ECONOMICS_FEES.md`, `57_IDENTITY_AUTHORITY.md`
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

**Bootstrap** is an ordinary CYBOU full peer whose IP:port is known in advance
for initial peer discovery. It runs the same executable and CYP2 protocol.
It has no `CAP_BOOTSTRAP`, no consensus role, and no special node class.

Public P2P admission is France-only for inbound and outbound connections across
all capabilities. Policy rules and local fail-closed Geo enforcement are
detailed in [`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`](37_FRANCE_SOVEREIGN_NETWORK_POLICY.md).

## Identity

One AccountID is the stable Identity. Mnemonic-derived Recovery, Authorization,
and KEM roles are separate. Device is not a protocol entity.

Storage providers prove their own service keys per CYP2 session. The PoA finalizer
proves the genesis-authorized PoA key. There is no canonical service-node registry.

## Finality and Validation

Canonical finality is single-operator hybrid-PQ PoA under the genesis-authorized
PoA key. The PoA finalizer executes operations independently and signs blocks.
There is no BFT or validator quorum.

Advisory Validation is optional pre-finalization evidence. Identities whose
Authority in the latest finalized state exceeds 1,000,000 are eligible to sign
Validation attestations. A peer configures locally whether to accept provisional
validation. PoA finality unconditionally overrides Validation.

## Application content and storage

`RootPublication` is the only application-content protocol operation.
Mail and Files share the same encrypted content substrate.
ChunkID is the full BLAKE3-256 digest of stored encrypted bytes.

Storage admission is finality-first by default: authorized chunks are admitted
remotely only after a finalized RootPublication authorizes them by Merkle proof.
Peers or storage providers enabling provisional validation policy may optionally
admit chunks upon sufficient eligible Validation signatures, but purge and roll
back such chunks if the candidate is rejected by PoA.
Application publication remains local until finality (or provisional admission).
Recoverable owner content requires an application-layer self capsule.
Beta storage durability targets 2 independent remote full replicas plus 1 local
physical copy (3 physical copies total); erasure coding is disabled.
