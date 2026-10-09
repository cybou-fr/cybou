# RootPublication

Status: CURRENT
Scope: Encrypted-content/KEM and application recovery source audit at 531dc0da, 2026-10-09. Existing component regressions are identified; no fresh C++ suite, live acceptance or standards conformity is claimed.

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
zero chunk counts and trailing bytes are rejected. The codec checks shape;
execution/fee validation enforces the global chunk bound and lease-period bound.
The public operation admits
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
bound to network/publication/sender authorization context. The implemented wrapper
is X-Wing encapsulation plus a CYBOU HKDF/AEAD transcript; it is not a serialized
HPKE base-mode message merely because the KEM profile uses an HPKE KEM identifier.

HKDF-SHA256 takes the X-Wing shared secret as input, NetworkBinding as salt,
and `"CYBOU/ROOT-CAPSULE-KEY" || root_chunk_id || sender_account_id ||
sender_nonce_u64be || sender_key_epoch_u64be || recipient_key_epoch_u64be` as info.
AEAD associated data is `"CYBOU/ROOT-CAPSULE-AAD" || NetworkBinding ||
root_chunk_id || sender_account_id || sender_nonce_u64be ||
sender_key_epoch_u64be || recipient_key_epoch_u64be`.
ChaCha20-Poly1305 wraps 32 bytes of ContentKey into nonce[12], ciphertext[32],
tag[16]. Context integers are big-endian even though capsule wire integers are
little-endian. No terminator/separator is appended to these domain strings.

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
publication is rejected. Providers admit chunks only while the lease is active. With zero initial periods
no lease is funded; finality alone does not allow remote admission. The displayed
formula assumes the current 86400-second period; generic escrow arithmetic also
multiplies by period length and divides by 86400.

## Defining source and regression boundary

[root_publication.cpp](../../src/cybou/root_publication.cpp) defines the wire and
exact `CYBOU/ROOT-PUBLICATION/P4` commitment bytes. `/P4` here is an existing
cryptographic domain, not a selectable wire-version field; preserve it.
[root_recipient_capsule.cpp](../../src/cybou/root_recipient_capsule.cpp) defines
the wrapper transcript. [PublicationService](../../src/cybou/publication_service.cpp)
creates recipient/self capsules and resumable local publication jobs today.
Existing [publication regressions](../../src/test/cybou_publication_service_tests.cpp)
cover recipient plus owner capsules, finality wait, exact-operation retry,
corrupt jobs and pin cleanup. Generic RootPublication parsing does not prove
that any capsule can be opened; that is a recipient-side check.
