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

## Identity

One AccountID is the stable Identity. Mnemonic-derived Recovery, Authorization
and KEM roles are separate. Device is not a protocol entity.

A future service node may bind a dedicated NodeID to an AccountID for
contribution accounting. NodeID is not an Identity credential and does not
gain access to the mnemonic, private Mail or Files.

## Finality

A genesis-bound single-operator hybrid-PQ PoA signer finalizes blocks. Every
full node independently verifies the certificate, executes operations and
checks the resulting state root.

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

## Identity Authority

Identity Authority supersedes the earlier Proof-of-Trust concept.

Authority is non-transferable, deterministic and separate from CYBOU and System
Balance. It never grants PoA finalization power.

The target sources are:

```text
Age
bounded finalized Activity
voluntary System Balance contribution
verified bound-node Liveness when canonical evidence exists
verified Storage contribution when canonical evidence exists
minus durable penalties
```

Authority derives generic Protocol, Storage and Bandwidth budgets using
immutable network parameters and bounded integer arithmetic.

## Future provisional validation

The signed provisional validation research target is described in
[`PROVISIONAL_VALIDATION.md`](PROVISIONAL_VALIDATION.md). It is a future,
non-canonical sidecar for claims about operations against a finalized base;
it does not add a consensus phase or gate PoA finality. Any validator
eligibility derived from NodeID and Authority still depends on future binding
and immutable network parameters.

Only PoA finality advances canonical state and authorizes remote storage.
