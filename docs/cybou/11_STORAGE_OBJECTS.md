# 11 — Distributed storage object model

Implementation status (2026-09-27): CYBOU has a bounded encrypted-chunk
profile, a durable network-bound provider store, opt-in CYP2 upload, manifest
commit and retrieval, and a local client file roundtrip through the encrypted
Storage Key Ring. StorageService can use the local provider or one explicitly
selected, already-connected CYP2 provider. The peer-backed path uploads and
retrieves a file and uses `CAP_STORAGE_ABORT` to release chunks after a failed
uncommitted upload. Lost commit acknowledgments return an uncertain result and
retain the encrypted private metadata; failed cleanup is reported separately.
This is still not a complete distributed Storage service:
automatic provider connection/selection, multi-peer placement, leases,
durability proofs, audits, repair, retention, and accounting are not
operational. A successful one-provider upload makes no durability promise.
The provider stores ciphertext bytes, their opaque identifiers, and a public
manifest; it receives no file metadata or key material. Storage is disabled by
default and can be enabled for `cybou-node serve` by supplying a positive
`STORAGE_CAPACITY_BYTES` after its optional peer file. The provider database is
stored beside the node database with an `.objects` suffix. Beta Mail cannot be
declared ready until the required Store path is complete. See
`81_BETA_PRODUCT_SCOPE.md` and `88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md` for the
separate object/key contract.

CYBOU storage is a cooperative object network, not a host-price marketplace.

## Opaque object principle

Storage nodes should not know whether an object is:

- backup data;
- Files directory metadata;
- Email body object;
- Email attachment;
- parity;
- manifest.

Application semantics must be encrypted before storage.

## Logical pipeline

```text
source data
    -> optional compression
    -> bounded chunking
    -> client-side encryption
    -> opaque protected units
    -> bounded replication OR a reviewed erasure-coding profile
    -> peer placement
    -> lease / audit / repair / retrieval
```

The implemented v1 provider transfer is opt-in CYP2 capability `CAP_STORAGE`.
Each CYP2 frame remains bounded to 4 KiB; a 1 MiB ciphertext chunk and the
bounded public manifest travel as sequences of data frames after a length
header. A provider accepts a chunk idempotently, rejects a conflicting value
for the same ObjectID/index, and does not serve it until a valid public
manifest commits to the ordered ChunkIDs and ciphertext lengths. PUT and
manifest writes are durably synced. Provider quota counts the encoded chunk
records and public manifest bytes. This quota is local capacity enforcement,
not a lease or an economic contribution score. Retrieval validates the
network-bound ChunkID and its manifest descriptor; the client still compares
the manifest commitment with its locally held expectation before trusting it.
This wire/store profile does not itself place replicas or promise durability.

Compression precedes encryption. Storage providers receive ciphertext only.
The initial implementation may use bounded replication while the network
establishes storage operation. A replication factor such as three is a
candidate profile, not a frozen Beta parameter and not the separate 3:1
contribution-to-entitlement policy. Erasure coding remains a later profile
choice, not a prerequisite for the first end-to-end Store slice.

## Object identifiers

Object identifiers must avoid leaking original filenames, paths or user IDs.

The provisional v1 client generates random ObjectIDs and per-object salts.
Deduplication and wider protocol privacy behavior remain unfrozen. See
`88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md` for the key hierarchy and private
manifest boundary.

## Erasure profiles

Do not hard-code `8+4`.

Protocol must support a profile identifier and explicit bounded parameters.

Examples only:

```text
development: 2+1 or 4+2
production candidate: 8+4
```

Production profile is chosen through reliability simulation and network measurements.

## Metadata

Encrypted manifests are stored as ordinary opaque objects.

A storage peer must not receive clear:

- filename;
- directory name;
- MIME type;
- original path;
- message subject;
- application object type.

Minimum routing/lease metadata is defined separately and minimized.

## Beta Mail integration

The initial DEV/Alpha Mail profile may remain text-only. Beta Mail requires
Storage-backed encrypted attachments and uses this boundary:

```text
Client:
    encrypts the message, attachment manifest, and attachment bytes
    uploads ciphertext objects and verifies retrieval

Store:
    retains opaque encrypted objects/shards
    performs placement, leases, audits, repair, and retrieval

MailTx / BFT / state:
    registers the required content commitment and opaque reference
    never contains attachment bytes

Recipient client:
    retrieves after sync, verifies, and decrypts locally
```

The manifest containing filenames, MIME types, object keys and attachment
metadata is E2E encrypted. Providers receive ciphertext only. Exact chunk
sizes, erasure profile, replication, leases, audit cadence, repair deadlines,
and accounting parameters remain open freeze points; product scope does not
set their values. Files is the Beta user-facing product over this Storage
layer. Backup is a post-Beta application; “Drive-like” is a Files usability
reference, not a separate CYBOU product.
