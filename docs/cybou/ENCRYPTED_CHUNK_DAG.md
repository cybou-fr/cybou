# Encrypted chunk DAG

Status: frozen architecture target. The initial codec milestones add a full
256-bit BLAKE3 `ChunkId` over stored encrypted bytes and a bounded RFC 8949
core-deterministic CBOR value codec. BLAKE3 C 1.8.1 is pinned with known-answer
tests. The CBOR limits and accepted types are recorded in
`spec/poa_chunk_dag.yaml`. Encrypted envelopes, graph construction/fetch,
publication, and storage admission remain pending. This is the shared
encrypted payload format for Mail, Files, and later Backup. Product schemas
are private application data, not consensus operation types.

## Addressing and privacy

```text
ChunkID = full 256-bit BLAKE3(stored encrypted chunk bytes)
```

The full hash is the only network/storage name for a chunk. Network and
provider interfaces expose no ObjectID, chunk index, filename, MIME type,
parent/child relation, or plaintext logical length. Encryption must be
randomized so equal plaintext does not create equal stored chunks by default.
Do not implement BLAKE3 or cryptographic primitives locally; use a vetted
implementation and published known-answer vectors.

## Stored and decrypted forms

Stored bytes contain only a bounded format marker, nonce, and AEAD ciphertext
with tag. The ciphertext is a bounded canonical-CBOR node plus random padding.
The node can contain a private schema identifier, metadata, payload bytes, and
child ChunkIDs. All graph structure and application interpretation remain
inside authenticated encryption.

The CYBOU CBOR profile rejects indefinite lengths, duplicate or unordered map
keys, non-minimal arguments, invalid UTF-8, tags, floating-point values, excess
nesting, excess map/array elements, and values beyond configured byte limits.
It uses RFC 8949 core-deterministic bytewise lexicographic map-key ordering.
Exact codec limits, accepted types, and the encrypted-node envelope are frozen
in `spec/poa_chunk_dag.yaml`. The envelope contains `CYCH`, version 1, a
per-node random 32-byte KDF salt, and a random 12-byte nonce. HKDF-SHA256
derives a separate node key from the graph content key, salt, and NetworkID.
ChaCha20-Poly1305 authenticates `CYBOU/CHUNK-AAD/v1 || header || NetworkID`.
Its plaintext frame contains the four-byte big-endian CBOR length, encoded
CBOR, and random padding to the smallest configured bucket. The full header,
ciphertext, and tag are covered by ChunkID. Cross-implementation vectors and
padding-overhead benchmarks remain open pre-cutover gates.

Use the reviewed ChaCha20-Poly1305, HKDF-SHA256, and account X-Wing KEM
profiles already selected for DEV. Do not introduce a custom combiner or
primitive. Each immutable graph version has one random 32-byte content key;
recipient capsules wrap that key. A changed graph version gets a fresh key.
Re-sharing can add a capsule for the same graph key, but cannot revoke access
to ciphertext and keys a recipient already obtained.

## Local graph builder and fetcher

The local builder partitions source data, creates private CBOR nodes, applies
padding, encrypts each node with unique nonces, calculates ChunkIDs, and returns
the root ID, key, encrypted chunks, and authorization commitment. Before a
finalized RootPublication exists, chunks stay local and are not admitted to
distributed storage.

The fetcher starts from a decrypted root, verifies BLAKE3 before decryption,
authenticates and parses each bounded node, and walks children with a visited
set. Maximum graph depth, children per node, total chunks, and reconstructed
bytes are protocol limits. Cycles, duplicate-work amplification, oversized
values, bad hashes, and AEAD failures are rejected.

Mail, Files catalog, file versions, shares, and later Backup use private
schemas over this same graph. No service may add a parallel public manifest or
indexed-chunk protocol.
