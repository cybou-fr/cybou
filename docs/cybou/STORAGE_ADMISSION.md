# Finalized chunk storage admission

Status: frozen architecture target; proof encoding, provider selection,
retention, and economics remain implementation gates.

## Authorization commitment

The client builds a domain-separated Merkle tree over every stored chunk:

```text
leaf = H(CYBOU/CHUNK-AUTH/LEAF || ChunkID || stored_size)
```

The root, chunk count, and authorized byte total are committed by a finalized
RootPublication. The hash function, canonical leaf encoding, odd-node rule,
and proof size limits require published vectors before cutover.

## Admission

Providers expose content-addressed operations only:

```text
PutChunk(ChunkID, stored_bytes, finalized_publication, admission_proof)
GetChunk(ChunkID)
HasChunk(ChunkID)     # optional hint
```

Before storing, a provider verifies the full BLAKE3 ChunkID, stored size,
Merkle inclusion against the publication, and the publication's PoA-finalized
chain proof. Invalid or not-yet-finalized chunks are rejected. Storage is
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