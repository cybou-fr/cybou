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
    NetworkBinding = SHA-256("CYBOU/NETWORK-ID/V6" || NetworkID), the 32-byte
      form used in HELLO, signatures, certificates, operations and DB keys
    Exactly one signed genesis per NetworkID forever

OfficialNetwork:
    - compiled public constants: Network Public Key, immutable signed
      NetworkGenesis object, initial genesis state, bootstrap locators
    - no external official network/genesis file or runtime file loader
    - no separate genesis digest profile pin

Provisioning:
    - generate Network and cybou.cybou private material once, offline
    - keep private material only under gitignored /private/
    - commit only public keys, public Identity data, and signed genesis constants

Network Private Key:
    - creation-time root of trust
    - strictly offline
    - never used online
    - never stored on bootstrap
    - never stored on PoA
    - signs the immutable genesis specification ONCE at network creation
    - never used by runtime
    - never changes an existing network

No:
    genesis_generation
    re-genesis
    in-place genesis replacement
    rollback between genesis versions
    NetworkTrustStore

Bootstrap:
    - ordinary CYBOU full peer
    - same executable (`cybou`)
    - nodes dial the compiled locators first, checking the compiled TLS SPKI pin
    - same CYP2 protocol
    - no CAP_BOOTSTRAP
    - no BootstrapNode class
    - no special consensus role
    - IP:port is known in advance for initial discovery
    - bootstrap status itself grants no authority and no AUTH
    - any AUTH held by the bootstrap operator's Identity is an ordinary
      GenesisAllocation decision, not a property of the bootstrap role

Account values (AccountState, committed by the state root):
    Balance         spendable, transferable CYBOU
    System Balance  non-transferable CYBOU service budget
    Authority       non-transferable AUTH, separate unit, not CYBOU supply

AUTH:
    - changes only through deterministic finalized state transitions
    - GenesisAllocation may assign initial AUTH
    - finalized Identity-authorized operation -> +1 AUTH to its authorizing
      account; AccountCreate earns nothing (only its genesis AUTH, if any)
    - PoaAuthAdjustment GRANT (signed by the genesis PoA key) -> +N AUTH
    - PoaAuthAdjustment BURN  (signed by the genesis PoA key) -> -N AUTH, floor 0
    - PoaAuthAdjustment itself earns no AUTH
    - no transfer between Identities
    - Authority > 1,000,000 AUTH makes an Identity eligible to sign Validation
    - Authority never grants PoA finalization power
    - automatic penalties are not frozen

Candidate execution:
    - every full node independently validates and executes every candidate
      operation against its latest finalized state
    - invalid -> reject, do not relay
    - valid   -> keep in bounded volatile pool and relay

Validation:
    - an additional signature, never a substitute for local execution
    - an Identity with finalized AUTH > 1,000,000 may sign an operation
      only after its own node independently validated it
    - signs NetworkBinding, OperationID, finalized base BlockID, AccountID
      with the ordinary Identity Authorization key
    - receiving nodes MUST independently validate the operation regardless
      of Validation signatures
    - eligibility is evaluated against latest FINALIZED state
    - pre-finalization evidence only; never changes balances or state
    - no provisional state, no provisional storage admission

PoA:
    - sole canonical finalizer
    - MUST independently execute every candidate
    - Validation signatures are never sufficient for finalization
    - trusts no validator/bootstrap/peer state
    - valid -> signs/finalizes
    - invalid -> drops
    - owns no special canonical pending state; only finalized state is canonical
    - cybou.cybou is an ordinary CYBOU Identity with a PoA key role
    - finalization right is the PoA public key authorized by genesis
    - no separate PoA Identity entity
    - no special operation routing to PoA: operations travel the ordinary
      mesh relay; the PoA key holder only proves its key in-session
      (FINALIZER_PROOF) so peers can confirm finalized tips

Canonical truth:
    latest valid PoA-finalized state

Only a valid PoA-finalized block changes canonical state.

No:
    voting against PoA
    validator fork-choice
    validator quorum finality
    validator registry, ValidatorSet, CAP_VALIDATOR
    provisional state or provisional storage from Validation
    validation.enabled / validation.min_signatures
    BFT
```

## DEV VPS deployment — migration state

There is no production network.
- **Target architecture**: The DEV bootstrap is an ordinary CYBOU full peer process.
- **Current migration state**: The DEV VPS still runs a retired prototype service
  (`cybou-bootstrap.service`, state in `/var/lib/cybou/bootstrap/state`, TLS files under
  `/etc/cybou-bootstrap/tls/`) built from an older commit. This repository no longer builds
  that executable or speaks its protocol; the coordinated migration replaces it with the
  ordinary `cybou` node on the re-provisioned DEVNET.
- Its pinned TLS endpoint is the approved DEV Bootstrap locator (`51.255.46.58:29461`);
  its SPKI SHA-256 pin is compiled in `src/cybou/official_networks.cpp` for initial transport
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
  service binary: `/home/debian/cybou/build/bin/cybou` (headless build).

## Executables and test networks

- `cybou` is the single production executable: without a command it is the
  desktop; `cybou finalizer|provider|observer run`, `cybou network ...`,
  `cybou doctor`, `cybou operation ...` and `cybou storage ...` run headless.
  A `BUILD_GUI=OFF` build contains only the headless commands.
- `cybou-loadgen`, storage smoke/soak and other tools exist only with
  `BUILD_TESTS=ON`.
- Multi-process LAB/CI networks use `--network lab`, compiled only with
  `-DCYBOU_ENABLE_LAB_NETWORK=ON` test builds: its own fixed public Network and
  PoA keys, never the DEVNET PoA key (one active signer per official key).

## Network and node architecture

- Every participant runs the same full-node software core. Storage and
  Central Authority PoA finalization are optional operational capabilities,
  not protocol node classes.
- A standard CYBOU installation knows official network profiles (DEVNET, MAINNET).
  Each official network is compiled as public constants: its Network Public Key
  (`NetworkID`), immutable signed `NetworkGenesis` object, initial genesis state,
  and bootstrap locators with TLS SPKI pins. There is no external official network
  file, runtime file loader, or separate genesis digest profile pin.
- DEVNET is the intended enabled network with bootstrap locator
  `51.255.46.58:29461`; MAINNET is unprovisioned, has no bootstrap locator,
  and remains disabled in the GUI until its keys, genesis, and bootstrap exist.
- Provisioning creates the DEVNET Network and ordinary `cybou.cybou` Identity
  secret material once under gitignored `/private/`. Only public keys, public
  Identity data, and signed genesis constants may enter Git.
- The Network Private Key is strictly offline and never online (including on DEVNET).
  It is used solely at network creation time to sign the immutable genesis specification once.
  For each NetworkID, exactly one signed genesis is valid. There is no `genesis_generation`,
  no re-genesis, and no in-place genesis replacement.
- Network genesis defines the initial chain state, protocol parameters, authorized
  PoA public key, and initial AUTH in GenesisAllocation for designated ordinary Identities.
  Such an allocation is a genesis decision, never a property of a bootstrap role.
- `cybou.cybou` is an ordinary account-level Identity with AccountID, Recovery,
  Authorization, KEM, Mail/support, and a distinct PoA key role derived from its
  mnemonic. Consensus recognizes its finalization right solely through the PoA
  public key in genesis; its name and AUTH value confer no finalization power.
- Cross-network migration does not exist. A network cutover to a new official network
  (a new Network Public Key, new NetworkID, new genesis) wipes all local network-bound state cleanly:
  chain/state, network definition, genesis, Identity, vault, AccountID,
  Recovery/Auth/KEM keys, balances, names, Mail, Files, application DB,
  peer DB, pending operations, and storage metadata.
- Ordinary peers form a direct P2P mesh after initial discovery. Bootstrap is
  an initial rendezvous peer, not a mandatory traffic intermediary or separate node type.
- There is no distributed mempool and PoA owns no canonical pending state. Every
  full node keeps a bounded volatile pool of candidate operations it has itself
  executed against its finalized state, and relays only locally valid candidates
  until they are finalized into blocks. A PoA node produces blocks from that same pool.
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
- Full nodes independently validate every candidate operation, block transition,
  state root, and PoA certificate.
- Validation is an additional signature by an Identity whose Authority in the
  latest finalized state exceeds 1,000,000, made only after its own node
  independently validated the operation. It is pre-finalization evidence and
  never substitutes local or PoA execution, never changes state, and creates
  no provisional state. Receiving nodes and PoA always re-execute.
- An operation is shown `Validated` when the local node holds it as valid and
  has at least one valid eligible Validation signature for it.
- PoA is centralized finality, not BFT.
- Durable signing journal must fail closed; equivocation conflicts are deterministically resolved by min(BlockID) while verified evidence is durably recorded.
- AUTH never grants PoA finalization power.

## Application content and storage admission

- `RootPublication` is the only application-content protocol operation.
- Mail, Files, filenames, folders, recipients, and application schemas remain
  encrypted application data.
- ChunkID is full BLAKE3-256 of exact stored encrypted bytes.
- Default storage admission is finality-first: chunks are admitted remotely only
  after a finalized RootPublication authorizes them by Merkle proof. Application
  publication remains local until finality.
- Validation never authorizes remote chunk admission; there is no provisional
  storage admission.
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
- The provider admission policy is finality-first.
- A file/message is not `Protected`/`Sent` merely because its RootPublication
  is finalized; durability requires confirmed remote replicas.

## Canonical Identity Authority

- Every AccountState has three canonical account values: spendable Balance in CYBOU,
  non-transferable System Balance in CYBOU, and non-transferable Authority in AUTH.
  All three are committed by the finalized state root.
- AUTH changes only through deterministic finalized transitions:
  GenesisAllocation (claimed exactly once by AccountCreate), +1 AUTH to the
  authorizing account of every finalized Identity-authorized operation except
  AccountCreate, and the PoA-signed `PoaAuthAdjustment` GRANT (+N) / BURN
  (-N, floor 0). AUTH is never transferred between Identities. Amounts locked,
  paid as fees, credited at onboarding or stored never scale AUTH; such an
  operation earns only the flat +1.
- Authority > 1,000,000 AUTH qualifies an Identity to sign Validation.
- Authority grants NO PoA finalization power, NO consensus voting rights, NO stake weight,
  and NO balance or resource allocations.
- Automatic AUTH penalties require objectively verifiable protocol evidence and
  are not frozen. Signed Validation of an operation that is invalid against its
  stated finalized base is evidence a future penalty rule may use.
- There is no canonical ValidatorSet, validator registry, NodeID binding,
  liveness/storage evidence, resource budget, reservation, ticket, or
  per-I/O accounting, and no derived AuthorityIndex.
- Local peer failures use local disconnect, backoff, and abuse limits; they do
  not change global Authority.

## Economics

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
DEV OnboardingPool = 100,000,000 CYBOU (genesis only; never replenished by fees)
100% protocol fee: payer System Balance -> Central Authority spendable Balance
Before claim: fees accumulate in the unique genesis allocation labelled cybou.
After claim: fees credit that allocation claimant's ordinary AccountState Balance.
```

Balance is spendable CYBOU. System Balance is an irreversible CYBOU service budget.
Authority is canonical, non-transferable AUTH and is excluded from CYBOU supply.
