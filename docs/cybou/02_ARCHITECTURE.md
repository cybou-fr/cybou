# CYBOU architecture

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

## Uniform Full Node invariant

CYBOU defines exactly one network node type: Full Node. Every Full Node
implements the complete CYBOU P2P baseline: blocks, announcements,
discovery, operation relay and encrypted storage. There
is no capability bitmap and no network role announcement. Storage is intrinsic;
capacity is local policy. Bootstrap is only a known locator of an ordinary Full
Node. PoA is
possession of the private key matching the public key in genesis, with durable
signing safety. IP, endpoints, TLS sessions, StorageId and peer declarations
never confer consensus authority. StorageId is proven on demand only for a
storage relationship. Peer sync completion is a liveness/UX hint, never proof
of global freshness or a prerequisite for creating an Identity.

CYBOU is an Identity-centered private Mail and Files platform over one
content-addressed encrypted P2P substrate.

## Layers

```text
Qt GUI
  |
  v
LocalApplicationService: local.db + encrypted local content
  | immutable durable Outbox / semantic import
  v
NetworkSyncService: app.db
  + ApplicationService / PublicationService / StorageService
  |
  v
native CYBOU NodeRuntime
  + canonical state execution and independent candidate execution
  + single-operator hybrid-PQ PoA finality (genesis-authorized P)
  + P2P mesh synchronization and operation relay
  + RootPublication
  + common encrypted ChunkStore
```

Local user actions commit independently of the network executor. The local
store owns indispensable drafts and desired state; the network store owns
finalized indexes and exact publication jobs. Their implementation and remaining
acceptance work are recorded separately from this accepted architecture target.

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
  -> Ordinary bootstrap peer seeds initial CYBOU P2P discovery
  -> Direct P2P mesh
  -> Every node executes candidates before relay
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


## Storage implementation evidence boundary

The Beta target remains two independent remote full replicas; the current
placement algorithm deduplicates proven StorageIds, which does not establish
independent hosts, operators or failure domains. The 1:3 reciprocal baseline
is a capacity/service objective, not measured proof of contribution: explicit
local capacity is an operator choice (`V >= 15 GiB`), not proof of service.
Storage is paid by finalized leases; only PoA-signed settlements record service.

Canonical state currently records publications, roots and recipient capsules,
not provider placements or audit reliability. Off-chain audit transport is implemented under DEC-276. Autonomous mutual-audit
scheduling and PoA evidence aggregation still need explicit design/integration.
Per-audit canonical notarization/reliability state are outside the current
accepted storage model; this does not change the deployed wire or genesis.
Off-chain storage evidence (DEC-276) consists of provider-signed receipts,
random-offset audits and periodic full GET plus ChunkID verification; none of
it is consensus state or a canonical proof.

Finalized revocation stops admission and closes the author's lease after the current period.
Compliant providers journal purge of unshared chunks, retaining physical byte
accounting until unlink succeeds or absence is confirmed; maintenance/restart
retry failures. This does not prove deletion of hidden copies or crypto-erasure.
See `docs/cybou/DATA_ASSURANCE_AND_ERASURE.md` for scoped regression evidence.

## Documentation hierarchy

Applicable law (RGPD and NIS2 where applicable), adopted ANSSI/ISO risk and
control requirements, and CIA objectives govern security/privacy acceptance
above this architecture and frozen decisions. See
[`SECURITY_GOVERNANCE.md`](SECURITY_GOVERNANCE.md) for the governing baseline;
[`SECURITY_STANDARDS.md`](SECURITY_STANDARDS.md) supports technical acceptance.
Conflicting internal requirements must be corrected with a documented migration
plan; a citation does not certify the implementation or authorize a genesis reset.

CYBOU architecture adheres to a strict hierarchy of authority. Lower levels
cannot introduce protocol mechanics absent from higher levels:
- **Level 0 (Implementation authority)**: `AGENTS.md`
- **Level 1 (Frozen architecture / decisions)**: `docs/cybou/24_DECISIONS.md`, `docs/cybou/02_ARCHITECTURE.md`
- **Level 2 (Normative domain documents)**: `04_NETWORK_LIFECYCLE.md`, `05_CHAIN_STATE.md`, `08_P2P.md`, `POA_FINALITY.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, `10_IDENTITY_NAMES.md`, `18_ECONOMICS_FEES.md`
- **Level 3 (Mutable implementation truth)**: `docs/cybou/26_IMPLEMENTATION_STATUS.md`
- **Level 4 (Roadmap / unresolved work)**: `docs/cybou/22_ROADMAP.md`, `docs/cybou/25_OPEN_QUESTIONS.md`
- **Level 5 (Product / UX)**: `docs/cybou/81`–`85`, `APPLICATION_DATA_PLANE.md`
- **Level 6 (Sovereignty / legal / strategy)**: `docs/cybou/37`–`43`
- **Level 7 (Machine-readable mirrors)**: `spec/*`
- **Level 8 (Public projection)**: `README.md`, `www/*`, `www/llms.txt`

## Full Node resources and peer admission

Every participant runs the same full-node core software.

Storage admits and serves authorized encrypted chunks on every Full Node;
its quota is local policy and is positive in production. Target (DEC-275): an
explicit capacity `V >= 15 GiB`, with provider obligations bounded by `floor(2V/3)`
inside the same ChunkBlobStore; `V` is never consensus state. Zero capacity is reserved
for memory-only unit tests. Possession of the genesis-authorized
PoA private key activates the independent block-production worker.

**Bootstrap** is an ordinary CYBOU full peer whose IP:port is known in advance
for initial peer discovery. It runs the same executable and CYBOU P2P protocol.
It has no network-role announcement, no consensus role, and no special node class.

Public P2P admission is France-only for inbound and outbound connections for
every Full Node. Policy rules and local fail-closed Geo enforcement are
detailed in [`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`](37_FRANCE_SOVEREIGN_NETWORK_POLICY.md).

## Identity

One AccountID is the stable Identity. Mnemonic-derived Recovery, Authorization,
and KEM roles are separate. Device is not a protocol entity.

`cybou.cybou` is an ordinary Identity with these roles, Mail/support, and a
distinct PoA key role from its mnemonic. Its name is not a consensus authority:
only the PoA public key authorized in genesis grants finalization right.

Storage keys are proven on demand within the storage relationship. PoA authority
is proven by finalized block certificates, never by a transport declaration. There is no canonical service-node registry.

## Finality

Canonical finality is single-operator hybrid-PQ PoA under the genesis-authorized
PoA key. The PoA finalizer MUST execute operations independently and signs
blocks. There is no BFT, validator or quorum.

Accounts hold only CYBOU: spendable Balance and non-transferable System Balance.
There is no AUTH or reputation unit; spam is priced by fees, storage rent and a
flat relay proof-of-work (DEC-284).

## Application content and storage

`RootPublication` is the only application-content protocol operation.
Mail and Files share the same encrypted content substrate.
ChunkID is the full BLAKE3-256 digest of stored encrypted bytes.

The blockchain functions as a canonical Notarial Register: it records object
publications, Merkle roots and recipient capsules. Mutual storage proofs remain
a target, not a current canonical record.
Storage admission rights (an active finalized lease) are derived deterministically
from PoA-finalized state; local capacity declarations and off-chain vouchers convey zero authority.

Storage admission is finality-first: authorized chunks are admitted remotely
only after a finalized RootPublication authorizes them by Merkle proof.
Application publication remains
local until finality.
Recoverable owner content requires an application-layer self capsule.
Beta storage durability targets 2 independent remote full replicas plus 1 local
physical copy (3 physical copies total); erasure coding is disabled.

Users pay storage rent from System Balance and may earn it back by serving
foreign storage; a node offering `2V/3` can roughly offset `V/3` of its own
two-replica storage at full demand, which is an estimate, never a guarantee. The target mutual-audit protocol would use randomized byte-offset and nonce
proofs; it is not implemented. Under the storage-economy target audits stay
off-chain and only daily PoA-signed StorageSettlement payouts become canonical.
When content is deleted by its author (`RevokePublication`), its active state
record is removed from the active publication register; historical blocks remain, initiating managed purge by compliant storing nodes of
underlying chunks from local storage.

## Economics

100% of every finalized protocol fee transfers atomically from the payer's System
Balance to the Central Authority's spendable Balance (the unique genesis-granted `cybou`
allocation before claim, and its ordinary claimant Balance afterwards). The whole
100,000,000,000 CYBOU genesis monetary base belongs to that Central Treasury and is
conserved; AccountCreate transfers the 20,000 CYBOU onboarding budget from it.
Storage is paid: StorageLease rent moves from payer System Balance to
StorageEscrow and is paid by PoA-signed StorageSettlement to providers with
verified foreign storage service, while protocol fees still go to the Central
Treasury. See `24_DECISIONS.md` DEC-274–DEC-283 and `18_ECONOMICS_FEES.md`.
See `18_ECONOMICS_FEES.md`.

## Built-in monitoring

DEC-289 limits Beta monitoring to passive local runtime metrics, one independently
verified canonical chain stream, ordinary peer liveness and service-owned storage
evidence. Broad resource telemetry, address-group aggregation, cohort history
and a separate polling scheduler remain absent. DEC-290 adds a small direct
storage query: ordinary TLS carries two existing FinalizedChunkStore counters
(provider budget and admitted encrypted bytes), without a new signature/audit. The existing independent storage probe samples
known/configured endpoints at most every 30 seconds per endpoint; aggregate only
direct samples not older than 90 seconds, once per StorageId, with coverage/age.
Physical provider copies count; declarations are not audited service,
guaranteed disk or a global census. No remote aggregate is forwarded or counted.
The P2P baseline ends at STORAGE_USAGE (28); higher codes fail closed. No
compatibility parser, consensus change, role or new worker is introduced.

Local CPU/RAM/frame traffic are Technical/Console diagnostics. Network retains
three primary values over the centered full map: verified-stream operations/min,
approximate provider capacity and admitted encrypted bytes across sampled nodes.
Local storage/PUT/GET and content protection remain Advanced detail. At most two
bounded local charts show operations
and completed payload. Local samples never imply network-wide totals or capacity.
See NETWORK_OBSERVABILITY_PLAN.md for the frozen metric set and addition gate.

## Simplified implementation boundary

The only node type is Full Node. Nodes announce no roles or capabilities.
PoA authority is possession of the genesis-authorized private key. Storage is
intrinsic; StorageId proves possession of a cryptographic storage key only. Rendezvous is a known
location of an ordinary Full Node. Runtime takes VerifiedNetworkGenesis and
uses its signed specification digest as the height-zero chain anchor.

PublicationService stages directly into pinned local encrypted chunks, stores
one encrypted ordered leaf list and generates Merkle proofs on demand in RAM.
RootPublication wire and encrypted/private schema are bounded binary
layouts with exact consumption. Hash256 hex follows its raw 32-byte order.
No legacy runtime, CBOR or reversed-hash decoder is retained. Provisioning and
network cutover require the previously established operator authorization.
