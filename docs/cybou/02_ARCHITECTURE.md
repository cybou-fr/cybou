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
  + canonical state execution and independent candidate execution
  + single-operator hybrid-PQ PoA finality (genesis-authorized P)
  + Validation signatures (local Identity AUTH > 1,000,000)
  + P2P mesh synchronization and operation relay
  + RootPublication
  + common encrypted ChunkStore
```

`APPLICATION_DATA_PLANE.md` defines the local/network data boundary.
`04_NETWORK_LIFECYCLE.md` defines official network trust, creation, joining,
and network replacement.

## Official network trust

```text
Compiled OfficialNetwork public constants:
  Network Public Key (NetworkID)
  immutable offline-signed NetworkGenesis object
  initial genesis state
  bootstrap IP:port and TLS SPKI pin
  -> Verify compiled genesis signature and initial state root
  -> Ordinary bootstrap peer seeds initial CYP2 discovery
  -> Direct P2P mesh
  -> Every node executes candidates; eligible Identities add Validation signatures
  -> Central Authority P signs finalized blocks (absolute canonical truth)
```

The Network Private Key is strictly offline and used solely at network creation
time to sign the immutable genesis specification once. For each NetworkID, exactly
one signed genesis is valid. The official release compiles its public genesis and
initial state into the client; no external official network/genesis file
is loaded at runtime. Nodes independently verify these constants before connecting;
bootstrap is an ordinary CYBOU full peer providing initial transport discovery only.
The Central Authority operates the genesis-authorized PoA key `P` and finalizes
blocks.

DEVNET is the enabled official profile with locator `51.255.46.58:29461`.
MAINNET remains unprovisioned, has no bootstrap locator, and is disabled in the
GUI until its keys, genesis, and bootstrap are ready. Provisioning keeps Network
and `cybou.cybou` private material under gitignored `/private/`; only public
material and signed constants enter the source tree.

Every full node independently validates blocks, operation validity, and state
transitions, and executes every candidate operation before relaying it.
Validation signatures are pre-finalization evidence only; they never replace
local or PoA execution and never change state.

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
- **Central Authority / PoA**: independently executes candidates and finalizes valid blocks.

**Bootstrap** is an ordinary CYBOU full peer whose IP:port is known in advance
for initial peer discovery. It runs the same executable and CYP2 protocol.
It has no `CAP_BOOTSTRAP`, no consensus role, and no special node class.

Public P2P admission is France-only for inbound and outbound connections across
all capabilities. Policy rules and local fail-closed Geo enforcement are
detailed in [`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`](37_FRANCE_SOVEREIGN_NETWORK_POLICY.md).

## Identity

One AccountID is the stable Identity. Mnemonic-derived Recovery, Authorization,
and KEM roles are separate. Device is not a protocol entity.

`cybou.cybou` is an ordinary Identity with these roles, Mail/support, and a
distinct PoA key role from its mnemonic. Its name is not a consensus authority:
only the PoA public key authorized in genesis grants finalization right.

Storage providers prove their own service keys per CYP2 session. The PoA finalizer
proves the genesis-authorized PoA key. There is no canonical service-node registry.

## Finality and Validation

Canonical finality is single-operator hybrid-PQ PoA under the genesis-authorized
PoA key. The PoA finalizer MUST execute operations independently and signs
blocks; Validation signatures are never sufficient for finalization.
There is no BFT or validator quorum.

Authority is a canonical non-transferable AUTH account value committed by the
state root, separate from CYBOU Balance and System Balance. It changes only
through finalized transitions (genesis, +1 per finalized Identity-authorized
operation, PoA-signed `PoaAuthAdjustment` GRANT / BURN). An Identity whose finalized
AccountState.authority exceeds 1,000,000 AUTH may add a Validation signature to
an operation its own node has independently validated. Every receiving node and
PoA still re-execute the operation.

## Application content and storage

`RootPublication` is the only application-content protocol operation.
Mail and Files share the same encrypted content substrate.
ChunkID is the full BLAKE3-256 digest of stored encrypted bytes.

Storage admission is finality-first: authorized chunks are admitted remotely
only after a finalized RootPublication authorizes them by Merkle proof.
Validation never authorizes chunk admission. Application publication remains
local until finality.
Recoverable owner content requires an application-layer self capsule.
Beta storage durability targets 2 independent remote full replicas plus 1 local
physical copy (3 physical copies total); erasure coding is disabled.

## Economics

Protocol fees transfer in each paid operation from System Balance to the
unique genesis-granted `cybou` allocation before claim, and to its ordinary
claimant Balance afterwards. The DEV OnboardingPool begins at 100,000,000
CYBOU and only decreases through AccountCreate. AUTH is excluded from supply.
See `18_ECONOMICS_FEES.md`.
