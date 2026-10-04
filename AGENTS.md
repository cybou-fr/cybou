# AGENTS.md — CYBOU implementation authority

## Uniform Full Node invariant

CYBOU defines exactly one network node type: Full Node. Every Full Node
implements the complete CYBOU P2P baseline: blocks, announcements,
discovery, operation relay, Validation transport and encrypted storage. There
is no capability bitmap and no network role announcement. Storage is intrinsic;
capacity is local policy. Bootstrap is only a known locator of an ordinary Full
Node. Validation requires an Identity with finalized AUTH > 10,000,000. PoA is
possession of the private key matching the public key in genesis, with durable
signing safety. IP, endpoints, TLS sessions, StorageId and peer declarations
never confer consensus authority. StorageId is proven on demand only for a
storage relationship. Peer sync completion is a liveness/UX hint, never proof
of global freshness or a prerequisite for creating an Identity.

Read the active CYBOU documents before coding. Git history records superseded
architecture; do not keep obsolete runtime paths alive for compatibility.

## Single-current-baseline invariant

If there is only one supported form of something, it has no version. It is simply
the current form. A version appears ONLY when at least two forms actually coexist
simultaneously or temporary migration between them is required. Once a migration
window concludes and only one form remains supported, the older form and the
version discriminator are removed. Code and architecture do not record development
history in type names, wire headers, schema discriminators, or conceptual models
(`CYBOU P2P`, `state`, `schema`, `wire`, `CYID`, `CYBV`). Git records
history; code embodies only the active truth.

Cryptographic domain separation strings (`CYBOU/NETWORK-ID`, `CYBOU/OP-ID`, etc.)
are exact bytes of cryptographic hash functions and key derivation. They are never
renamed mechanically in source; instead, they transition to eternal unversioned
domain strings (`CYBOU/NETWORK-ID`, `CYBOU/OP-ID`, `CYBOU/STORAGE-ID`, etc.)
exclusively during coordinated network genesis resets before MAINNET.

## CYBOU official networks constitution

```text
CYBOU OFFICIAL NETWORKS
=======================

Official networks:
    DEVNET
    MAINNET

Network identity:
    NetworkID = Network Public Key
    NetworkBinding = SHA-256("CYBOU/NETWORK-ID" || NetworkID), the 32-byte
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
    - same CYBOU P2P protocol
    - no node-role announcement
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
    - finalized network utility operations (RootPublication, SystemLock) -> +1 AUTH
      to their authorizing account, velocity-capped at max +1 AUTH per account per block
    - AccountCreate, Payment, IdentityRotate, NameCommit, NameReveal earn no AUTH
      (prevents zero-cost and ping-pong Sybil farming)
    - PoaAuthAdjustment GRANT (signed by the genesis PoA key) -> +N AUTH
    - PoaAuthAdjustment BURN  (signed by the genesis PoA key) -> -N AUTH, floor 0
    - PoaAuthAdjustment itself earns no AUTH
    - no transfer between Identities
    - Authority > 10,000,000 AUTH makes an Identity eligible to sign Validation
    - Authority never grants PoA finalization power
    - automatic penalties are not frozen

Candidate execution:
    - every full node independently validates and executes every candidate
      operation against its latest finalized state
    - invalid -> reject, do not relay
    - valid   -> keep in bounded volatile pool and relay

Validation:
    - an additional signature, never a substitute for local execution
    - an Identity with finalized AUTH > 10,000,000 may sign an operation
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
    - operations travel the ordinary mesh relay
    - PoA ownership is never announced or authenticated in a P2P session
    - nodes verify finalized blocks against the genesis public key

Canonical truth:
    latest valid PoA-finalized state

Only a valid PoA-finalized block changes canonical state.

No:
    voting against PoA
    validator fork-choice
    validator quorum finality
    validator registry, ValidatorSet
    provisional state or provisional storage from Validation
    validation.enabled / validation.min_signatures
    BFT
```

## DEV VPS deployment — migration state

There is no production network.
- **Target architecture**: The DEV bootstrap is an ordinary CYBOU full peer process.
- **Current deployment**: `cybou-node.service` runs the ordinary headless
  `cybou node run` on the immutable current DEVNET, with state under
  `/var/lib/cybou/node/state` and intrinsic automatic storage allocation.
  The preceding network domain is retired under `/var/lib/cybou/node-retired-20261003-de-version`;
  the earlier hardening retirement is preserved separately. Neither state may
  be reused by the current network. The prototype service is inactive.
  TLS files remain under `/etc/cybou-bootstrap/tls/` for transport identity only.
- Its pinned TLS endpoint is the approved DEV Bootstrap locator (`51.255.46.58:29461`);
  its SPKI SHA-256 pin is compiled in `src/cybou/official_networks.cpp` for initial transport
  discovery only. This grants no consensus role, no special protocol capability, and does not make
  the bootstrap a separate node class.
- The target uses one full-node core software with intrinsic encrypted storage and an optional local PoA signer, with the authorized PoA key holder finalizing from the Central Authority desktop.
  France-only public peer admission is mandatory in production/DEV.
- Central Authority is identified solely by the PoA key authorized by network genesis.
  Never add a persistent Authority IP, host, endpoint, or NodeID to bootstrap
  state or consensus. No peer session authenticates a PoA route. Verify block signatures only.
- Connect as `debian@vps-d0669a91.vps.ovh.net`; checkout: `/home/debian/cybou`;
  service binary: `/home/debian/cybou/build/bin/cybou` (headless build).

## Executables and test networks

- `cybou` is the single production executable: without a command it is the
  desktop; `cybou node run` (optional `--poa-key-file`), `cybou network ...`,
  `cybou doctor`, `cybou operation ...` and `cybou storage ...` run headless.
  A `BUILD_GUI=OFF` build contains only the headless commands.
- Production `cybou` has no provisioning command or Network Root derivation/signing.
  `cybou-provision` is a separate explicitly built offline tool (`BUILD_PROVISION_TOOL=ON`).
  Creation never overwrites existing private material or public constants; verification never signs.
- Every production Full Node has a positive local storage allocation. Omitted capacity is
  automatic; explicit zero is confined to memory-only unit tests. Low disk space rejects
  new admission without changing node type, consensus authority, or mesh participation.
- `cybou-loadgen`, storage smoke/soak and other tools exist only with
  `BUILD_TESTS=ON`.
- Development and integration run on DEVNET with its existing compiled genesis
  and locally stored pre-generated keys. No alternate development network, runtime
  genesis generator, automatic key export or Geo bypass exists. Keep one active
  signer and one durable signing history per official PoA key.
- Component tests may use in-memory fixtures and synthetic inputs; these are not
  selectable networks and never enter the production executable.
- Cleanup does not authorize new keys, a new NetworkID or replacement genesis.
  Preserve the accepted network material and its derivation domains.

## Network and node architecture

- Every participant runs the same full-node software core. Storage is intrinsic to every Full Node. PoA is an optional local signer,
  never a network node class.
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
  outbound peers for every Full Node. Classification uses local Geo data;
  unavailable/corrupt data fails closed. Development uses the same admission rule. Optional VPN/proxy/Tor filtering is local
  policy and never changes consensus or Identity.
- Bootstrap nodes do not vote, form a quorum, or finalize. PoA remains
  single-operator finality under the active Authority key.

## Identity

- One protocol Identity is account-level. Device is not a protocol entity.
- Stable random AccountID is independent from mnemonic and keys.
- Recovery: Ed25519 + ML-DSA-65. Authorization: Ed25519 + ML-DSA-44.
- Identity KEM uses its own mnemonic-derived role; do not reuse signing keys.
- Portable CYBOU Identity Vault vault stores stable AccountID plus recovery entropy.
- IdentityRotate atomically replaces Recovery, Authorization, and KEM roles.
- No device registry, primary-device concept, session authorization, identity
  transfer, DeviceAdd, or DeviceRevoke.
- `.cybou` names use finalized commit/work/reveal and the active name rules.

## Documentation hierarchy and sources of truth

Applicable French/EU legal obligations (including RGPD and NIS2 where applicable),
adopted ANSSI/ISO risk and control requirements, and confidentiality, integrity
and availability objectives take precedence over internal architecture and
product decisions. `docs/cybou/SECURITY_GOVERNANCE.md` governs this baseline;
`docs/cybou/SECURITY_STANDARDS.md` is its supporting technical register.
Distinguish mandatory law, adopted guidance, analysis models and certification.
Applicable published technical security standards govern acceptance within
their scope, including over this file and frozen decisions. Drafts
are explicitly experimental; using a standardized primitive does not certify
its composition or the product. A conflict requires correcting the internal
decision and documenting the compatibility/migration plan before release.
This priority does not silently change deployed wire bytes, cryptographic
domains, keys or immutable genesis; provisioning and cutover still require
their established authorization. Never claim conformity without evidence.

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
  latest finalized state exceeds 10,000,000, made only after its own node
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

## Canonical Identity Authority and Resource Governance

- Every AccountState has three canonical account values: spendable Balance in CYBOU,
  non-transferable System Balance in CYBOU, and non-transferable Authority in AUTH.
  All three are committed by the finalized state root.
- AUTH changes only through deterministic finalized transitions:
  GenesisAllocation (claimed exactly once by AccountCreate), +1 AUTH to the
  authorizing account of finalized utility operations (RootPublication,
  SystemLock) subject to an anti-Sybil per-block velocity limit of max +1 AUTH
  per account per block, and the PoA-signed `PoaAuthAdjustment` GRANT (+N) / BURN
  (-N, floor 0). Payments, key rotations, and name claims earn no AUTH to prevent
  ping-pong and zero-cost Sybil farming. AUTH is never transferred between Identities.
  Amounts locked, paid as fees, credited at onboarding or stored never scale AUTH;
  such an operation earns only the flat +1.
- Authority > 10,000,000 AUTH qualifies an Identity to sign Validation.
- Authority grants NO PoA finalization power, NO consensus voting rights, and NO stake weight.
- AUTH acts as the anti-spam and resource scaling governor (DEC-272). Block
  execution enforces, per Identity and against the parent finalized AUTH, a tier
  limit of metered operations per block and per epoch, a remote storage quota and a
  largest single publication (file). Quota is counted in 512 KiB chunks of the
  finalized publication register. Limits expand with AUTH up to the Validator tier
  (AUTH > 10,000,000), whose limits are high but finite: one Identity can never
  take a whole block. AccountCreate and PoaAuthAdjustment are not metered.
- Every user operation carries relay proof-of-work (DEC-273): SHA-256 of
  `CYBOU/OP-WORK || NetworkBinding || OperationID || nonce` with tier-dependent
  difficulty (names harder). Every Full Node, the PoA included, admits and relays
  only operations whose work meets the author's finalized tier. The nonce travels
  with the exact bytes until finalization and never enters a block.
- One `.cybou` name per Identity: NameCommit and NameReveal refuse an Identity that
  already owns a name or holds a pending commit.
- Every newly created Identity receives an immediate Onboarding Trust Credit of 5 GB
  remote storage in the network, grounded in the reciprocal 1:3 physical storage obligation
  (storing 10–15 GB of foreign data locally on desktop).
- Automatic AUTH penalties require objectively verifiable protocol evidence and
  are not frozen. Signed Validation of an operation that is invalid against its
  stated finalized base is evidence a future penalty rule may use.
- Local peer failures use local disconnect, backoff, and abuse limits; they do
  not change global Authority.

## Notarial object storage register, mutual proofs, and pruning

- The blockchain is the canonical Notarial Register: it records object publications,
  Merkle roots, recipient capsules, and mutual storage proofs. Quotas, allowances,
  and admission rights are derived deterministically strictly from PoA-finalized state.
  Local capacity declarations and off-chain vouchers convey zero authority.
- Mutual Proof of Storage & Uptime: storing peers periodically challenge each other
  with randomized byte-offset/nonce verification of stored chunks. Verified challenge
  results are notarized in PoA blocks, establishing deterministic peer reliability coefficients.
- State Synthesis & Object Pruning: block finalization synthesizes history into state.
  When an object is deleted by its author (`RevokePublication`), its active state record
  is retired/tombstoned, authorizing storing nodes to immediately purge the underlying chunks
  from local ChunkStore, preventing storage bloat. `RevokePublication` is author-only,
  costs the payment fee from System Balance, earns no AUTH and frees its chunks from the
  author's quota; a revoked publication no longer authorizes chunk admission.

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

## Simplified implementation boundary

The only node type is Full Node. Nodes announce no roles or capabilities.
PoA authority is possession of the genesis-authorized private key; Validation
is an eligible Identity signature checked against finalized state. Storage is
intrinsic; StorageId proves replica independence only. Rendezvous is a known
location of an ordinary Full Node. Runtime takes VerifiedNetworkGenesis and
uses its signed specification digest as the height-zero chain anchor.

PublicationService stages directly into pinned local encrypted chunks, stores
one encrypted ordered leaf list and generates Merkle proofs on demand in RAM.
RootPublication wire and encrypted/private schema are bounded binary
layouts with exact consumption. Hash256 hex follows its raw 32-byte order.
No legacy runtime, CBOR or reversed-hash decoder is retained. Provisioning and
network cutover require the previously established operator authorization.
