# Encrypted chunk DAG

Status: frozen architecture target; implementation and interoperable wire
profile are pending. This is the shared encrypted payload format for Mail,
Files, and later Backup. Product schemas are private application data, not
consensus operation types.

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

The CYBOU CBOR profile rejects indefinite lengths, duplicate keys, non-minimal
integer encodings, excess nesting, excess map/array elements, and values beyond
configured byte limits. Exact limits, padding buckets, nonce/AAD encoding, and
KAT/cross-implementation vectors are open pre-cutover gates; no limits are
implied by this architecture document.

Use the reviewed ChaCha20-Poly1305, HKDF-SHA256, and account X-Wing KEM
profiles already selected for DEV. Do not introduce a custom combiner or
primitive. Each graph has a random root content key; recipient capsules wrap
that key. Key scope and rotation rules must be fixed in the root publication
wire profile.

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

\n