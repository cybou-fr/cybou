# RootPublication

RootPublication is the single generic application-content protocol operation.

## Public body

Conceptually:

```text
RootPublication {
    root_chunk_id
    chunk_authorization_root
    chunk_count
    lease_periods
    recipient_capsules[]
}
```

The current wire layout is fixed-order binary:

```text
root_chunk_id:32
chunk_authorization_root:32
chunk_count:u32 LE
lease_periods:u32 LE (initial StorageLease periods; 0 = none)
capsule_count:u16 LE (1..32)
repeat capsule_count:
    kem_profile:u16 LE
    recipient_key_epoch:u64 LE
    X-Wing encapsulation:1120
    wrapped_content_key:60
```

The body is bounded to 128 KiB. Invalid profiles, zero roots,
zero chunk counts and trailing bytes are rejected. The public operation admits
at most 1,048,576 chunks. Payload commitment is SHA-256 over
`CYBOU/ROOT-PUBLICATION/P4 || canonical_payload_bytes`.

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

## Initial storage lease

`lease_periods` makes publication and payment one atomic operation (DEC-279):
the author pays the fee to Treasury and `ceil(chunk_count x replicas x rate x
periods / 2048)` CYBOU into StorageEscrow from System Balance, or the whole
publication is rejected. Providers admit chunks only while the lease is active.
