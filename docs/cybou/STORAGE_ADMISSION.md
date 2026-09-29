# Finalized chunk storage admission and durability

Status: active admission substrate plus frozen durability architecture target.

## Physical store

Providers use one content-addressed encrypted ChunkStore:

```text
chunks/<2hex>/<2hex>/<full ChunkID>
```

ChunkID is full lower-case BLAKE3-256 of exact stored encrypted bytes.

The physical store does not classify content as:

```text
mine
foreign
Mail
File
```

Provider metadata is separate from blob bytes.
Local staging/cache and provider retention use the same physical blob. A local
blob survives restart and provider-metadata reset; remote GET still requires
provider admission metadata for a finalized publication. Provider capacity
counts admitted bytes, while the common store counts each physical blob once.

## Admission

Provider accepts:

```text
PutChunk(publication_reference, ChunkID, bytes, admission_proof)
```

only after verifying:

1. publication exists in canonical finalized history;
2. Merkle proof authorizes ChunkID under that publication;
3. BLAKE3(bytes) equals ChunkID;
4. local capacity/policy allows storage.

GET is content-addressed by ChunkID.

For placement recovery, providers may return the stored authorization proof
for a `(finalized OperationID, ChunkID)` pair only while the admitted blob is
present and its proof verifies against canonical finalized history. Clients
use verified leaf indices to reconstruct publication order; provider proof
metadata remains operational and is not consensus state.

Unfinalized chunks are rejected remotely.

## Durability targets

Development target:

```text
1 remote full replica per required chunk
```

Beta target:

```text
2 independent remote full replicas per required chunk
```

The local encrypted copy does not count toward the remote target, but it
normally exists as one more physical copy (Beta: local + 2 remote = 3).

`Protected` in Beta means all required chunks satisfy the three-remote-replica
target.

## Erasure coding

Reed-Solomon/erasure coding is disabled for Beta.

Full replication preserves simple:

```text
ChunkID -> exact encrypted bytes
GET -> exact encrypted bytes
BLAKE3 verification
repair by copying a verified chunk
```

Erasure coding may be researched only after measured Beta replication cost
justifies the extra protocol complexity.

## Placement

Placement is per ChunkID, not necessarily one provider set per file.

StorageService selects independent eligible remote providers using secure
random selection and diversity/health/capacity rules.

Do not freeze a deterministic provider-ranking algorithm that can be cheaply
gamed by ProviderID generation before a mature provider anti-Sybil model exists.

Providers may reject admission; StorageService tries other eligible peers until
the replica target is reached or reports a retryable failure.

Placement metadata is operational cache, not canonical recovery state.

## Retrieval after clean recovery

If old provider placement is unknown, query multiple discovered
storage-capable peers for the requested ChunkID until a valid response is found.

A DHT/global provider index is not required for the first Beta-scale network.

## Audit and repair

Initial durability may rely on successful PUT acknowledgments plus periodic
health/retrieval verification.

Signed receipts become necessary when canonical provider contribution/Authority
accounting is introduced, but are not a prerequisite for the first working
replication path.

When healthy remote copies drop below target:

```text
retrieve any valid copy
-> choose replacement provider
-> PUT authorized chunk
-> restore replica target
```

Temporary provider timeout is not automatically fraud.

## Local cache

The local encrypted copy is useful for:

```text
offline access
fast open
repair
re-upload
```

It may later be evicted after remote durability is healthy. Semantic Mail/Files
items remain in the Identity Application DB and content is re-fetched on
demand.
