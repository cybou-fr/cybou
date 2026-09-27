# 11 — Distributed storage object model

Implementation status: this is a Beta-required target protocol design, not an
operational service. The Qt desktop has a capability-gated Storage page, but
distributed object placement, retrieval, durability proofs, repair, and
accounting are not operational in the DEV runtime. Beta Mail cannot be declared
ready until the required Store path is live. See `81_BETA_PRODUCT_SCOPE.md`.

CYBOU storage is a cooperative object network, not a host-price marketplace.

## Opaque object principle

Storage nodes should not know whether an object is:

- backup data;
- directory metadata;
- Drive content;
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
    -> padding policy
    -> client-side encryption
    -> erasure coding
    -> opaque shards
    -> peer placement
    -> lease / audit / repair / retrieval
```

Compression precedes encryption. Storage providers receive ciphertext only.

## Object identifiers

Object identifiers must avoid leaking original filenames, paths or user IDs.

Exact content-addressed vs randomized-ID behavior remains to be frozen after privacy/deduplication analysis.

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
set their values. Backup and Drive are post-Beta applications of this same
Storage layer.
