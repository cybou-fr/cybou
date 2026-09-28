# Finalized chunk storage admission

Status: frozen architecture target; a durable local provider admission store
and proof encoding are implemented. Canonical-history integration, wire
serialization, provider selection, retention, and economics remain gates.

## Authorization commitment

The client builds a domain-separated Merkle tree over every stored chunk in
exact durable staging order (DATA in source order, INDEX nodes at the point
they are generated, ROOT last):

```text
leaf = H(CYBOU/CHUNK-AUTH/LEAF || ChunkID || stored_size)
```

The root and chunk count are committed by a finalized RootPublication and
verified by each inclusion proof. `authorized_stored_bytes` is an authenticated
per-publication provider ceiling, not a proven aggregate sum of all Merkle
leaves. The provider enforces the ceiling against data it accepts for that
publication. The provider records each leaf index as it durably stages a chunk.

## Admission

Providers expose content-addressed operations only:

```text
PutChunk(publication_reference, ChunkID, stored_bytes, admission_proof)
GetChunk(ChunkID)
HasChunk(ChunkID)     # optional hint
```

Before storing, a provider verifies the full BLAKE3 ChunkID, individual stored
size, and Merkle inclusion against the publication reference. Since a provider
is a full node, it resolves that reference in its own canonical finalized
history; PUT does not carry a separate finality proof. It enforces its
per-publication accepted-byte ceiling. Invalid or not-yet-finalized chunks are rejected. Storage is
immutable and idempotent by ChunkID. Providers retain the publication
reference, inclusion proof, and lease/accounting metadata beside the bytes for
repair and revalidation.

Physical layout is sharded by the full 64-character lowercase hex ChunkID;
the sharding directories have no protocol meaning. No public manifest commit,
indexed chunk address, or remote abort operation is required by this target.

## Durability and privacy

Chunk placement operates independently per ChunkID. A RootPublication may
authorize a graph before every chunk reaches its required durability target;
the client must retain retryable ciphertext until acknowledgments meet the
frozen threshold. Storage metadata reveals opaque chunk IDs, stored sizes,
provider placement, and publication proofs only. Padding and traffic-analysis
risks remain explicit privacy limitations.

Exact capacity accounting, expiry/retention, provider loss, repair, retrieval
fallback, denial-of-service bounds, and Beta durability thresholds are not
defined by the hash-addressing decision and must pass the Storage readiness
gates before Beta.

\n
