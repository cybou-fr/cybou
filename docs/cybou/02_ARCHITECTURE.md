# CYBOU architecture

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
Identity Application DB / product model
  |
  v
ApplicationService / PublicationService / StorageService
  |
  v
native CYBOU NodeRuntime
  + canonical state execution and independent candidate execution
  + single-operator hybrid-PQ PoA finality (genesis-authorized P)
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
is a capacity/service objective, not measured proof of contribution: automatic
local capacity is an explicit operator choice (`V >= 15 GiB`), not proof of service.
Storage is paid by finalized leases; only PoA-signed settlements record service.

Canonical state currently records publications, roots and recipient capsules,
not provider placements or audit reliability. Mutual-audit transport, PoA
notarization and canonical reliability coefficients are unimplemented target
work requiring an evidence/privacy/accounting design before implementation.
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

## Built-in direct observation target

DEC-289 defines non-canonical, untrusted resource/traffic reports over existing
admitted same-network TLS mesh sessions. A bounded local cache supplies minimized
aggregates independently of the GUI/Identity. Fresh challenges bind replies to
the requesting session; this proves neither measurement truth nor a stable
node/machine identity. One selected report per transport-address group prevents
ports/session churn from multiplying a reporting group, without claiming host
independence or network coverage. Canonical operation/register streams stay local
and independently verified. There is no telemetry NodeID, role, storage proof,
Identity signature, PoA route or privileged bootstrap aggregator.

[`NETWORK_OBSERVATION_REPORTS.md`](NETWORK_OBSERVATION_REPORTS.md) freezes bounds,
rounding, replay/expiry, grouping, aggregation and acceptance for the first direct
report target. Payload codec, local cache, runtime guard and direct TLS transaction
are implemented; automatic polling/grouping/consolidation remain targets.
Source P2P ends at message 28; deployed upgrade and governing privacy/security
review gates remain. Relayed
reports, cross-address deduplication and global census remain open.

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
