# Encrypted chunk tree

Status: frozen architecture target; cross-implementation vectors and storage
admission remain cutover gates. This is the shared encrypted payload format
for Mail, Files, and later Backup. Application schemas exist only inside
authenticated encryption.

## Addressing and privacy

```text
ChunkID = full 256-bit BLAKE3(stored encrypted chunk bytes)
```

The full hash is the only network and provider filename. Interfaces expose no
ObjectID, filename, MIME type, recipient AccountID, or plaintext length.
Encryption uses fresh random salt, nonce, padding, and randomized DATA chunk
boundaries. Identical source bytes therefore produce different chunk IDs in
normal operation. Use the pinned vetted BLAKE3 implementation and existing
ChaCha20-Poly1305, HKDF-SHA256, and X-Wing draft-05 profiles.

## Stored and decrypted forms

Each encrypted chunk uses the `CYCH` version-1 envelope: a 32-byte random KDF
salt, 12-byte nonce, ciphertext, and 16-byte tag. HKDF-SHA256 derives a
per-chunk key from the tree content key, salt, and NetworkID. ChaCha20-Poly1305
authenticates `CYBOU/CHUNK-AAD/v1 || header || NetworkID`. ChunkID hashes the
entire stored envelope.

The encrypted frame is `uint32_be plaintext_length || plaintext || random
padding`. Plaintext is bounded to 512 KiB minus the four-byte length. Padding
uses the configured buckets and may select the next bucket when it adds no
more than 256 KiB. The crypto API accepts and returns bytes; it does not parse
CBOR.

The tree is immutable and ordered:

```text
ROOT (private CBOR) -> DATA* (raw application bytes)
       or
ROOT (private CBOR) -> INDEX* (private CBOR) -> ... -> DATA*
```

ROOT and INDEX contain one child kind and an ordered array of ChunkIDs. The
encrypted tree schema has no per-child plaintext-size declarations. DATA is
the original byte range directly, with no CBOR byte-string wrapper. INDEX is
introduced only when the ordered child list no longer fits inside ROOT's
128-child bound. An empty or metadata-only object is a ROOT with zero children.
Mail, Files catalog, attachment, and Backup schemas are private data inside the
decrypted stream. ROOT can carry up to
240 KiB of opaque private canonical-CBOR application metadata, surfaced to the
client after decryption.
Generic protocol code validates its encoding but does not interpret its
schema.

## Streaming local builder and reader

The builder reads from a source callback, chooses randomized 160–320 KiB DATA
boundaries, encrypts each piece, and durably stages it locally before moving
on. It retains only bounded buffers and at most 128 child references at each
tree level. The durable staging callback receives the assigned leaf index and
must reject duplicate ChunkIDs; this lets the local store build its proof index
without holding the object in memory. `ChunkAuthorizationProofIndex` stores
ordered ChunkIDs and Merkle levels in LevelDB, then returns a requested leaf
proof without materializing the complete proof set in RAM. After upload or
abandonment, the client discards the proof index to release its local metadata.
The builder returns the root ChunkID, content key, chunk-authorization root,
and uint64 counters; it does not return a whole-file vector or all encrypted
chunks. It performs no network writes. Unfinalized chunks remain in local
staging.

The reader fetches one encrypted chunk at a time, verifies its full BLAKE3
address, authenticates and decrypts it, then writes DATA bytes to a caller
sink and clears the temporary buffer. The caller supplies a disk-backed
unique-ID visitor so duplicate references and cycles fail without retaining
every visited ID in RAM. A caller-provided output/entitlement limit and local
disk capacity bound the transfer. Sink output may be partial when a later
chunk fails, so callers write to local staging and expose the result only after the complete tree traversal succeeds. The protocol has no
artificial 256 MiB per-file limit; primitive chunk, tree depth, count, and
uint64 byte-counter bounds remain enforced.

Providers admit a chunk only after a finalized RootPublication and a valid
Merkle inclusion proof for that ChunkID. The leaf needs no separate size field:
ChunkID commits to the complete stored encrypted bytes. RootPublication does
not publish the complete chunk-ID list. Each provider enforces its own physical
capacity limit. Finality authorizes storage but does not prove durability.
