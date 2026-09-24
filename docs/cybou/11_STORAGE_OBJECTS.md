# 11 — Distributed storage object model

Implementation status: this is a target protocol design. The Qt desktop has
a capability-gated Storage page, but distributed object placement, retrieval,
durability proofs, and accounting are not operational in the DEV runtime.

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
    -> chunking
    -> padding policy
    -> encryption
    -> erasure coding
    -> opaque shards
    -> peer placement
```

Compression precedes encryption.

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


## Email integration

Attachments do not exist in CYBOU Email before Object Storage.

When Store becomes production-ready, Email migrates from:

```text
encrypted text ciphertext in MailTx
```

to:

```text
encrypted mail object in Store
+
content root/object reference in MailTx
```

Attachments are encrypted Store objects referenced by an encrypted mail manifest.

The blockchain/state remains the registration/finality layer, not the bulk byte-storage layer.
