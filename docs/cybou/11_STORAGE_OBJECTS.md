# 11 — Distributed storage object model

Implementation status: this is a Beta-required target protocol design, not an
operational service. The Qt desktop has a capability-gated Files/Storage page,
but distributed object placement, retrieval, durability proofs, repair, and
accounting are not operational in the DEV runtime. Beta Mail cannot be declared
ready until the required Store path is live. See `81_BETA_PRODUCT_SCOPE.md` and
`88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md` for the separate object/key contract.

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

Compression precedes encryption. Storage providers receive ciphertext only.
The initial implementation may use bounded replication while the network
establishes storage operation. A replication factor such as three is a
candidate profile, not a frozen Beta parameter and not the separate 3:1
contribution-to-entitlement policy. Erasure coding remains a later profile
choice, not a prerequisite for the first end-to-end Store slice.

## Object identifiers

Object identifiers must avoid leaking original filenames, paths or user IDs.

Exact content-addressed versus randomized-ID behavior remains to be frozen
after privacy and deduplication analysis. See `88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md`
for the key hierarchy and private manifest boundary.

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
