# AGENTS.md — CYBOU implementation authority

Read the active CYBOU documents before coding. Git history records superseded
architecture; do not keep obsolete runtime paths alive for compatibility.

## CYBOU official networks constitution

```text
CYBOU OFFICIAL NETWORKS
=======================

Official networks:
    DEVNET
    MAINNET

Network identity:
    NetworkID = Network Public Key

Network Private Key:
    - generated before network launch
    - never used online
    - never stored on bootstrap
    - never stored on PoA
    - owner can create/recreate/edit signed genesis with monotonic genesis_generation
    - owner is the root authority of that network

Bootstrap:
    - ordinary CYBOU full peer
    - same executable
    - same CYP2 protocol
    - no CAP_BOOTSTRAP
    - no BootstrapNode class
    - no special consensus role
    - IP:port is known in advance for initial discovery
    - bootstrap status itself grants no authority

Bootstrap Identity:
    - ordinary CYBOU Identity
    - initial Authority assigned by genesis
    - DEV bootstrap initial Authority = 1,000,001

Authority:
    - deterministic Identity property
    - canonical value comes only from PoA-finalized history/state
    - Authority > 1,000,000 makes an Identity eligible to sign Validation
    - Authority never grants PoA finalization power

Validation:
    - optional pre-finalization
    - non-canonical
    - peer chooses locally whether to trust it
    - default minimum signatures = 1
    - only signatures of eligible Identities count
    - eligibility is evaluated against latest FINALIZED state

PoA:
    - sole canonical finalizer
    - independently executes every candidate
    - trusts no validator/bootstrap/peer state
    - valid -> signs/finalizes
    - invalid -> drops
    - owns no special canonical pending state; only finalized state is canonical

Canonical truth:
    latest valid PoA-finalized state

If Validation conflicts with PoA:
    discard provisional state
    rollback provisional effects
    adopt PoA-finalized state unconditionally

No:
    voting against PoA
    validator fork-choice
    validator quorum finality
    merge of conflicting provisional state
    BFT
```

## DEV VPS deployment — migration state

There is no production network.
- **Target architecture**: The DEV bootstrap is an ordinary CYBOU full peer process.
- **Current migration state**: The DEV VPS currently runs the standalone prototype
  `cybou-bootstrap.service` (`/home/debian/cybou/build/bin/cybou-bootstrap serve` on `0.0.0.0:29461`,
  state in `/var/lib/cybou/bootstrap/state`, TLS files under `/etc/cybou-bootstrap/tls/`)
  until the coordinated migration to the ordinary CYBOU full-peer service.
- Its pinned TLS endpoint is the approved DEV Bootstrap locator (`51.255.46.58:29461`);
  its SPKI SHA-256 pin is compiled in `src/cybou/official_networks.h` for initial transport
  discovery only. This grants no consensus role, no special protocol capability, and does not make
  the bootstrap a separate node class.
- The target uses one full-node core software with optional storage and PoA finalization
  capabilities, with the authorized PoA key holder finalizing from the Central Authority desktop.
  France-only public peer admission is mandatory in production/DEV.
- Central Authority is identified solely by the PoA key authorized by network genesis.
  Never add a persistent Authority IP, host, endpoint, or NodeID to bootstrap
  state or consensus. Authenticate its current route per live session and
  discard that route on disconnect.
- Connect as `debian@vps-d0669a91.vps.ovh.net`; checkout: `/home/debian/cybou`;
  service binary: `/home/debian/cybou/build/bin/cybou-node`.

## Network and node architecture

- Every participant runs the same full-node software core. Storage and
  Central Authority PoA finalization are optional operational capabilities,
  not protocol node classes.
- A standard CYBOU installation knows official network profiles (DEVNET, MAINNET).
  Each profile pins the compiled Network Public Key (`NetworkID`), the bootstrap
  IP:port and its TLS SPKI pin.
- The Network Private Key is strictly offline and never online (including on DEVNET).
  It is used solely by the network owner to sign genesis specifications containing
  a strictly monotonic `genesis_generation`.
- Network genesis defines the initial chain state, protocol parameters, authorized
  PoA public key, and initial Authority assignments for designated ordinary Identities
  (e.g., DEV bootstrap Identity initial Authority = 1,000,001).
- Cross-network migration does not exist. A newer valid official network replacement
  (verified with `genesis_generation > installed_generation` and valid Network Key signature)
  wipes all local network-bound state cleanly:
  chain/state, network definition, genesis, Identity, vault, AccountID,
  Recovery/Auth/KEM keys, balances, names, Mail, Files, application DB,
  peer DB, pending operations, storage metadata, and Authority indexes.
- Ordinary peers form a direct P2P mesh after initial discovery. Bootstrap is
  an initial rendezvous peer, not a mandatory traffic intermediary or separate node type.
- There is no distributed mempool and PoA owns no canonical pending state. Operations
  propagate across peers via bounded volatile relays until executed and finalized into blocks.
- Public P2P admission is France-only in production/DEV, for inbound and
  outbound peers across all capabilities. Classification uses local Geo data;
  unavailable/corrupt data fails closed. LAB loopback/private test traffic
  requires an explicit LAB bypass. Optional VPN/proxy/Tor filtering is local
  policy and never changes consensus or Identity.
- Bootstrap nodes do not vote, form a quorum, or finalize. PoA remains
  single-operator finality under the active Authority key.

## Identity

- One protocol Identity is account-level. Device is not a protocol entity.
- Stable random AccountID is independent from mnemonic and keys.
- Recovery: Ed25519 + ML-DSA-65. Authorization: Ed25519 + ML-DSA-44.
- Identity KEM uses its own mnemonic-derived role; do not reuse signing keys.
- Portable CYBV2/CVID5 vault stores stable AccountID plus recovery entropy.
- IdentityRotate atomically replaces Recovery, Authorization, and KEM roles.
- No device registry, primary-device concept, session authorization, identity
  transfer, DeviceAdd, or DeviceRevoke.
- `.cybou` names use finalized commit/work/reveal and the active name rules.

## Documentation hierarchy and sources of truth

The documentation has a strict hierarchy; lower levels cannot introduce
architecture that is absent from higher levels:
- LEVEL 0 — Implementation authority: `AGENTS.md`
- LEVEL 1 — Frozen architecture and decisions: `docs/cybou/24_DECISIONS.md`, `docs/cybou/02_ARCHITECTURE.md`
- LEVEL 2 — Normative domain documents: `04_NETWORK_LIFECYCLE.md`, `05_CHAIN_STATE.md`, `08_P2P.md`, `POA_FINALITY.md`, `VALIDATION.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, `10_IDENTITY_NAMES.md`, `18_ECONOMICS_FEES.md`, `57_IDENTITY_AUTHORITY.md`, etc.
- LEVEL 3 — Mutable implementation truth: `docs/cybou/26_IMPLEMENTATION_STATUS.md`
- LEVEL 4 — Roadmap and unresolved work: `docs/cybou/22_ROADMAP.md`, `docs/cybou/25_OPEN_QUESTIONS.md`
- LEVEL 5 — Product and UX contracts: `docs/cybou/81_BETA_PRODUCT_SCOPE.md`–`85_BETA_UI_ACCEPTANCE.md`, `APPLICATION_DATA_PLANE.md`
- LEVEL 6 — Business, legal, and sovereign policy: `docs/cybou/37`–`43`
- LEVEL 7 — Machine-readable mirrors: `spec/*`
- LEVEL 8 — Public projection: `README.md`, `www/*`, `www/llms.txt`

## Finality and Validation

- Canonical finality is single-operator hybrid-PQ PoA under the genesis-authorized
  PoA key for the block height.
- The Central Authority PoA finalizer executes operations independently, trusts no
  external validator state, and publishes finalized blocks.
- Full nodes independently validate every operation, block transition, state
  root, and PoA certificate.
- Advisory Validation is optional, non-canonical pre-finalization evidence.
  A peer chooses locally whether to accept provisional validation.
  Only signatures from Identities whose Authority in the latest finalized state
  exceeds 1,000,000 are eligible.
- PoA finality unconditionally overrides provisional Validation. In case of any conflict,
  provisional state is discarded, provisional effects are rolled back, and the
  PoA-finalized state is adopted unconditionally.
- PoA is centralized finality, not BFT.
- Durable signing journal and equivocation conflict halt must fail closed.
- Authority metric never grants PoA finalization power.

## Application content and storage admission

- `RootPublication` is the only application-content protocol operation.
- Mail, Files, filenames, folders, recipients, and application schemas remain
  encrypted application data.
- ChunkID is full BLAKE3-256 of exact stored encrypted bytes.
- Default storage admission is finality-first: chunks are admitted remotely only
  after a finalized RootPublication authorizes them by Merkle proof. Application
  publication remains local until finality.
- Nodes or providers enabling provisional validation policy may optionally admit
  and cache chunks upon receiving sufficient eligible Validation signatures, but
  such chunks remain provisional until PoA finality. If PoA rejects the publication,
  provisional admissions are purged and rolled back.
- A RootPublication may locally bundle multiple encrypted content trees under
  one authorization root; this is an application implementation pattern, not
  a new wire entity.
- Recoverable owner content requires an application-layer self capsule.
- Mail and Files share the same application publication/storage substrate.

## Application data plane

- The common ChunkStore is a network store of encrypted bytes keyed by ChunkID.
  It has no user-facing own/foreign classification.
- GUI/pages never enumerate the common ChunkStore or provider DB.
- Each unlocked Identity uses a separate encrypted rebuildable Application DB
  for Mail/Files semantic state.
- Publication discovery indexes only publications whose capsules the Identity
  can open.
- Core services are intentionally few: `ApplicationService`,
  `PublicationService`, and `StorageService`.
- The GUI consumes semantic model data, never provider storage topology.

## Storage durability

- Development target: 1 remote full replica per required chunk.
- Beta target: 2 independent remote full replicas per required chunk (plus local copy = 3 physical copies total).
- DEV may run extra providers for failover, repair, and soak; they do not raise
  the durability target.
- Local encrypted content is useful cache/staging but does not count toward
  remote durability. Beta uses full replication; erasure coding is disabled.
- Placement, provider selection, health, audit, and repair are StorageService
  concerns, not consensus state.
- The default provider policy is finality-first.
- A file/message is not `Protected`/`Sent` merely because its RootPublication
  is finalized; durability requires confirmed remote replicas.

## Derived Identity Authority

- Authority is a deterministic, non-transferable property derived exclusively from
  PoA-finalized history and state.
- Authority > 1,000,000 qualifies an Identity to sign provisional Validation attestations.
- Authority grants NO PoA finalization power, NO consensus voting rights, NO stake weight,
  and NO balance or resource allocations.
- There is no canonical ValidatorSet, validator registry, NodeID binding,
  liveness/storage evidence, reward/penalty system, resource budget,
  reservation, grant, ticket, or per-I/O accounting.
- Local peer failures use local disconnect, backoff, and abuse limits; they do
  not change global Authority.

## Economics

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
4 fee units -> 3 Security + 1 Onboarding
```

Balance is spendable. System Balance is an irreversible service budget.
Authority is non-transferable and derived.
