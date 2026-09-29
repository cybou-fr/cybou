# RootPublication

RootPublication is the single generic application-content protocol operation.

## Public body

Conceptually:

```text
RootPublication {
    root_chunk_id
    chunk_authorization_root
    chunk_count
    recipient_capsules[]
}
```

The active wire profile/canonical encoding remains the implementation
authority. This document does not introduce a new format.

## Privacy

Consensus does not learn:

```text
Mail vs Files
filename/folder
Mail subject/body
recipient AccountID from capsule
private graph topology
```

The outer Identity authorization supplies authoritative publisher AccountID,
nonce and key epoch.

## Recipient capsule

A capsule wraps the main root ContentKey to a recipient KEM capability and is
bound to network/publication/sender authorization context.

Failed unwrap means the publication is not accessible with that local key
epoch; do not retain a negative discovery record.

## Application-layer bundle

One RootPublication may authorize chunks belonging to multiple encrypted trees.

Example:

```text
attachment A tree
attachment B tree
Mail main root
      |
      v
one global chunk staging order
one authorization Merkle root
one RootPublication
```

`root_chunk_id` identifies the main private application root.

Child RootChunkIDs and child ContentKeys are private metadata inside the
encrypted main root.

This does not create:

```text
BundleID
bundle wire format
MailTx
FileTx
```

## Self capsule

Application recovery policy requires a self capsule for publisher content that
must survive clean-machine recovery.

For one-recipient Mail:

```text
recipient capsule
self capsule
```

Files mutation publications use a self capsule.

This is an application rule; the generic RootPublication wire still carries
only opaque recipient capsules.

## Authorization Merkle tree

The authorization root commits to every staged ChunkID in durable staging
order. Providers verify per-chunk inclusion against a finalized publication.

ChunkID itself commits to exact encrypted stored bytes.

## Finality and durability

PoA finality authorizes remote admission.

It does not prove:

```text
provider possession
availability
replica count
durability
```

The publisher retains retryable encrypted local content until StorageService
reports the required remote durability state.
