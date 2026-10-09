# AGENTS.md — CYBOU implementation authority

Status: CURRENT
Scope: Mandatory implementation invariants and agent workflow.

Read [the documentation index](docs/cybou/README.md), current architecture,
[current decisions](docs/cybou/24_DECISIONS.md) and applicable normative documents
before coding. CURRENT describes authority, not proof of implementation or Beta
acceptance. PROPOSAL does not authorize implementation; EVIDENCE is limited to its
revision/scenario; HISTORICAL cannot create current tasks. Git and dated archives
record development history; runtime embodies only the current supported form.

## Architecture and trust

- C++20 native Full Node and Qt desktop share the same core; no REST/JSON-RPC
  boundary between desktop and runtime. Core services are ApplicationService,
  PublicationService and StorageService.
- Exactly one node type: Full Node. Every node implements blocks, announcements,
  discovery, candidate relay and encrypted storage. Storage is intrinsic; capacity
  is local policy. No capability bitmap, node-role announcement, provider registry,
  BootstrapNode class or special consensus role. Bootstrap is an ordinary known
  locator. StorageId proves a storage-key relationship only.
- PoA is possession of the genesis-authorized private key with durable signing
  safety. IP, TLS, endpoints, StorageId, peer claims, names and balances confer no
  consensus authority. No announced/authenticated PoA route, persistent Authority
  endpoint or NodeID. cybou.cybou is an ordinary Identity with a distinct PoA role.
- Every node independently executes candidates against latest finalized state
  before bounded volatile retention/relay; the PoA independently executes them
  too. Only valid PoA-finalized blocks change canonical state. Submitted is not
  finalized. No canonical pending/provisional state, distributed mempool, AUTH,
  reputation token, Validation, validator quorum, voting or BFT.
- Keep one active signer and one durable history per official PoA key. Journal
  fails closed; signed equivocation evidence is durable and conflicts resolve
  deterministically by min(BlockID). Ordinary restarts preserve history.

## Official networks and immutable material

- DEVNET and MAINNET are the only official profiles. NetworkID is the exact Network
  Public Key; NetworkBinding is SHA-256("CYBOU/NETWORK-ID" || NetworkID), 32 bytes.
  Exactly one immutable signed genesis exists per NetworkID forever.
- OfficialNetwork consists of compiled public key, signed genesis, initial state
  and bootstrap locators/pins. No external runtime genesis file/loader, separate
  genesis digest pin, NetworkTrustStore, re-genesis or in-place replacement.
  Runtime takes VerifiedNetworkGenesis; its signed specification digest anchors
  height zero and signing safety.
- Network Private Key stays strictly offline, signs genesis once at creation,
  never runs on bootstrap/PoA and never changes an existing network. Provision
  once only with explicitly built offline cybou-provision; no production
  provisioning command. Secrets remain under gitignored /private/; commit only
  public material. Cleanup never authorizes new keys or replacement genesis.
- Existing DEVNET uses binding eee26eca…3665 (2026-10-04); compiled bootstrap
  51.255.46.58:29461 has a transport-only SPKI pin. MAINNET remains unprovisioned
  and GUI-disabled. A separately authorized cutover to a new NetworkID replaces
  every local network-bound chain/state, Identity/vault/key, application, peer,
  pending and storage record; no cross-network migration or old-state reuse.

## Single current baseline

If exactly one supported form exists, it has no version. Discriminators exist
only during actual coexistence/temporary migration and are removed afterwards.
Do not retain obsolete runtime paths, legacy decoders, reversed hashes, CBOR or
historical naming. Cryptographic domain separation strings are exact hash/KDF
bytes: never rename them mechanically. Transition to eternal unversioned domains
only during explicitly coordinated pre-MAINNET network genesis resets.

## Storage and application boundaries

- Explicit production V >= 15 GiB (GUI default 15 GiB; headless --capacity).
  One content-addressed ChunkBlobStore is bounded by V; provider obligations by
  floor(2V/3), remainder local reserve. Zero/smaller capacities only in memory-only
  component fixtures. Low disk rejects new admission without changing mesh/type.
  No StorageVolume, custom filesystem, storage role, stake, slashing or auction.
- RootPublication is the sole content operation. Mail/Files metadata and schemas
  stay encrypted. ChunkID is full BLAKE3-256 of exact stored encrypted bytes.
  Remote admission requires finalized publication/Merkle authorization and funded
  lease; no provisional admission. Bundled trees create no new wire entity.
- Mail/Files use separate per-Identity encrypted rebuildable Application DBs.
  Index only capsules the Identity can open. GUI consumes semantic projections,
  never enumerates common ChunkStore/provider objects. Publisher recovery needs
  an application self capsule. Direct pinned staging and one encrypted ordered
  leaf list; transient Merkle levels/proofs, no separate staging/proof DB.
- DEV target is one remote full replica; Beta target two independent remote full
  replicas plus local copy. Cache never counts as remote durability. Distinct
  StorageIds, payout accounts or addresses do not prove independent hosts/operators.
  Production placement excludes this machine and deduplicates addresses. Storage
  selection/health/audit/repair are local service concerns, not consensus authority.
- Receipts, offset audits and full GET verification are off-chain. Audit transport
  exists; network evidence aggregation/assignment attestation remain explicit
  implementation gaps. No per-audit blockchain/reliability state. Canonical service
  payments are PoA-signed StorageSettlement only. Do not silently rewrite accepted
  DEC-280/DEC-282 targets into current wire structures; record and resolve gaps.
- Finalized author revocation stops admission/closes lease after current period;
  compliant providers journal purge of unshared chunks and retain byte accounting
  until unlink/confirmed absence, retrying failures. No proof of hidden-copy
  deletion or crypto-erasure. Full replication; no Reed-Solomon.

## Identity and economy

- Identity is account-level; no protocol Device registry, primary-device concept,
  session authorization, DeviceAdd/DeviceRevoke or identity transfer. Stable random
  AccountID is independent of mnemonic/keys. Recovery Ed25519 + ML-DSA-65;
  Authorization Ed25519 + ML-DSA-44; KEM has a separate mnemonic role.
  Portable vault contains AccountID/recovery entropy; IdentityRotate atomically
  replaces Recovery/Authorization/KEM. Names use finalized commit/work/reveal;
  one .cybou name per Identity, no second owned name or simultaneous pending commit.
- CYBOU originates only from genesis: 100,000,000,000, decimals 0, Treasury
  allocation cybou.cybou. No MAX_SUPPLY, OnboardingPool, mint or burn.
  TotalCybou(parent) equals TotalCybou(candidate), including escrow/unclaimed funds.
  Spendable Balance and irreversible System Balance are canonical CYBOU values.
- AccountCreate retains consensus PoW; Treasury transfers 20,000 CYBOU to new
  System Balance (Treasury claimant excluded). Fees: System Balance to Treasury,
  100%. Rent: System Balance to StorageEscrow to verified foreign providers, 100%.
  Onboarding-origin payouts never become transferable. StorageLease bills 512 KiB
  units/two replicas; no variable price or capacity/score-weighted winners.
- Flat relay work on user operations: SHA-256(CYBOU/OP-WORK || NetworkBinding ||
  OperationID || nonce), 22 leading zeros, names +4. Check before execution/relay;
  nonce accompanies exact candidate bytes, never enters finalized blocks. No AUTH
  operation tiers; local abuse/disconnect/backoff never changes canonical state.

## Operations and development

cybou is the sole production executable: desktop without command; node run,
network, doctor, operation and storage headless commands. BUILD_GUI=OFF is
headless; BUILD_TESTS=ON enables loadgen/smoke/soak and in-memory fixtures only.
No alternate selectable development network, automatic key export or Geo bypass.
Every Full Node listens (29461 or free port), announces listen_port in HELLO,
and shares inbound locators only after successful connect-back (DEC-287).
Direct mesh follows bootstrap discovery. Public inbound/outbound admission is
France-only, uses integrity-checked local Geo data and fails closed when missing.
Loopback/RFC1918/link-local/IPv6 ULA are local, admitted without Geo (DEC-285).
Optional VPN/proxy/Tor filtering is local policy, not identity/consensus.

Current VPS layout, commands and retired-state locations belong to
[DEVNET_DEVELOPMENT.md](docs/cybou/DEVNET_DEVELOPMENT.md). Documentation refactoring
never authorizes stopping nodes, deploying, changing signer or clearing state.

### Explicit destructive DEVNET reset

Before MAINNET, the operator may explicitly authorize a coordinated DEVNET reset
to height zero while retaining the exact compiled genesis, Network Key, ordinary
Identity key material, PoA key and bootstrap TLS identity. This is a destructive
development exercise, not a new network, genesis replacement or production recovery.
It overrides the ordinary requirement to retain active signing history only for
this explicitly authorized DEVNET reset.

Stop every participating signer and node first. Archive the prior chain, state,
signing journal, safety evidence, vaults, application indexes, encrypted chunks,
peer records and pending operations before clearing active network-bound data.
Restart exactly one signer with a fresh DEVNET signing journal. Do not import old
blocks or journals into the restarted exercise. Preserve all private key material
and transport pins. No Network Root signing or provisioning is required.

The protocol cannot distinguish old and new block histories under the same
NetworkID and genesis. Old signed blocks remain cryptographically valid and can
cause replay, conflicting history or a safety halt. This reset therefore requires
control of the participating development nodes, is not a network isolation
guarantee, and must never be represented as production-safe recovery. Keep normal
signature, conflict detection and fail-closed journal checks enabled. MAINNET and
ordinary DEVNET restarts retain the immutable history and durable signing rules.


## Documentation hierarchy and change discipline

Applicable law, adopted ANSSI/ISO risk/control requirements and CIA objectives
precede internal decisions under [SECURITY_GOVERNANCE.md](docs/cybou/SECURITY_GOVERNANCE.md).
Distinguish mandatory obligations, guidance, experimental drafts, analysis and
certification. Standards conflicts require a documented decision/compatibility
plan, not silent wire/genesis changes; never claim conformity without evidence.

0 AGENTS; 1 architecture/decisions; 2 normative domains; 3 implementation status;
4 roadmap/open questions; 5 product/UX; 6 business/legal strategy; 7 spec mirrors;
8 public projection. Lower levels cannot invent higher-level architecture.

Compare disputed claims with defining headers/tests. Separate documentation
errors, accepted unimplemented requirements, code/decision deviations and ideas.
Use [the conflict register](docs/cybou/DOCUMENTATION_CONFLICT_REGISTER.md).
Do not automatically make decisions match code. Preserve dated FAIL evidence.
Documentation-only packages cannot change consensus/wire/crypto/genesis or live
operational state. Keep stable normative paths; historical redirects carry no
requirements. Update MANIFEST and run documentation checks after changes.
