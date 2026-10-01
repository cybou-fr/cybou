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
  + genesis-bound hybrid-PQ PoA finality
  + P2P synchronization
  + RootPublication
  + common encrypted ChunkStore
```

`APPLICATION_DATA_PLANE.md` defines the local/network data boundary.

`04_NETWORK_BOOTSTRAP_AND_GENESIS.md` defines the frozen target for network
creation, official bootstrap, and Central Authority operation. The current
binary/deployment has not completed that migration.

## Identity

One AccountID is the stable Identity. Mnemonic-derived Recovery, Authorization
and KEM roles are separate. Device is not a protocol entity.

Storage providers prove their own service keys where the transport requires a
provider identity. The PoA finalizer proves the genesis-bound key derived for
the Central Authority Identity. There is no canonical service-node registry
or binding of provider processes to an AccountID.

## Finality

A genesis-bound single-operator hybrid-PQ PoA signer finalizes blocks. In the
target deployment, the Central Authority desktop runs that signer after the
matching Identity is unlocked. The official VPS bootstrap only relays and
caches data; it never finalizes. Every full node independently verifies the
certificate, executes operations and checks the resulting state root.

CYBOU is not BFT. Durable anti-equivocation signing and a fail-closed conflict
halt protect against conflicting valid PoA certificates.

## Application content

`RootPublication` is the only application-content protocol operation.

Mail/Files type, filenames, folders, recipient identity and graph topology are
encrypted application data. Recipient capsules wrap the main root ContentKey
without exposing recipient AccountID.

One RootPublication may authorize chunks from multiple private content trees
under one authorization Merkle root. This is an application-layer bundle, not
a new protocol operation or wire entity.

## Storage

ChunkID is full BLAKE3-256 of exact stored encrypted bytes. The common
ChunkStore has no semantic own/foreign distinction and is invisible to the GUI.

Remote admission is finality-first:

```text
prepare encrypted chunks locally
-> finalize RootPublication by PoA
-> providers admit authorized chunks
-> reach durability target
```

Development targets one remote full replica; Beta targets two independent
remote full replicas. The local encrypted copy is cache/staging and does not
count toward remote durability, though it is normally a further physical copy. Beta does not use erasure coding.

## Application projection

Each unlocked Identity has a separate encrypted rebuildable Application DB for
Mail/Files semantic state. It contains only content the Identity can
cryptographically open.

The GUI renders this private projection and canonical Wallet/Names/Authority;
it never browses the provider ChunkStore.

## Authority and Validation

Authority is a read-only metric derived from finalized account history. It is
separate from CYBOU and System Balance, non-transferable, and grants no
protocol, resource-allocation, or PoA power. See
[`57_GLOBAL_PROOF_OF_TRUST_POLICY.md`](57_GLOBAL_PROOF_OF_TRUST_POLICY.md).

Any full node may produce optional advisory Validation. Recipients verify it
and decide locally whether to trust it. A local Authority threshold of
1,000,000 may label an opinion as validator-qualified; it is not an admission
rule. Validation never affects state transitions, finality, or provider
authorization. Only PoA finality advances canonical state and authorizes
remote storage.
