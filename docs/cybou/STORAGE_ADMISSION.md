# Finalized chunk storage admission

Status: active DEV protocol substrate; a durable local provider admission store,
proof encoding, typed Identity-authorized RootPublication operation, canonical
finalized-history lookup, and CYP2 content-addressed PUT/GET messages are
implemented. Client provider selection, retry, durability, retention, repair,
and economics remain Beta readiness gates.

## Authorization commitment

The client builds a domain-separated Merkle tree over every stored chunk in
exact durable staging order (DATA in source order, INDEX nodes at the point
they are generated, ROOT last):

```text
leaf = H(CYBOU/CHUNK-AUTH/LEAF || ChunkID)
```

The root and chunk count are committed by a finalized RootPublication and
verified by each inclusion proof. ChunkID already commits to the exact stored
bytes, so the proof carries no redundant size, ChunkID, or chunk count: PUT_CHUNK provides
the ChunkID and the finalized RootPublication provides the count. Providers enforce local physical
capacity independently of publication. The provider records each leaf index as
it durably stages a chunk.

## Admission

Providers expose content-addressed operations only:

```text
PutChunk(publication_reference, ChunkID, stored_bytes, admission_proof)
GetChunk(ChunkID)
HasChunk(ChunkID)     # optional hint
```

Before storing, a provider verifies the full BLAKE3 ChunkID and Merkle inclusion
against the publication reference. Since a provider is a full node, it resolves
that reference in its own canonical finalized history; PUT does not carry a
separate finality proof. Providers enforce local capacity and reject invalid or
not-yet-finalized chunks. Storage is immutable and idempotent by ChunkID. They
retain the publication reference, inclusion proof, and lease/accounting metadata
beside the bytes for repair and revalidation.

Persistent providers keep opaque bytes in hash-named files, sharded as
`chunks/<first-two-hex>/<next-two-hex>/<full-64-character-lowercase-ChunkID>`.
The files have no extension. A separate local metadata database records each
chunk's exact stored size and per-publication proof association. Capacity is
rebuilt from metadata, so a missing blob remains reserved and can be repaired
without allowing over-admission; uncommitted orphan blobs and temporary files
are removed during startup reconciliation. Reads verify both the recorded size
and full ChunkID. The sharding directories have no protocol meaning. No public
manifest commit, indexed chunk address, or remote abort operation is required
by this target.

## Durability and privacy

Chunk placement operates independently per ChunkID. A RootPublication may
authorize a graph before every chunk reaches its required durability target;
the client must retain retryable ciphertext until acknowledgments meet the
frozen threshold. Storage metadata reveals opaque chunk IDs, actual stored sizes, provider
placement, and publication proofs only. Padding and traffic-analysis
risks remain explicit privacy limitations.

Exact capacity accounting, expiry/retention, provider loss, repair, retrieval
fallback, denial-of-service bounds, and Beta durability thresholds are not
defined by the hash-addressing decision and must pass the Storage readiness
gates before Beta.
