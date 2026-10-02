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
blob survives restart and provider-metadata reset. Provider capacity
counts admitted bytes, while the common store counts each physical blob once.

## Storage admission policies

### 1. Default policy (Finality-first)

Under default policy, a provider accepts:

```text
PutChunk(publication_reference, ChunkID, bytes, admission_proof)
```

only after verifying:

1. publication exists in canonical PoA-finalized history;
2. Merkle proof authorizes ChunkID under that publication;
3. BLAKE3(bytes) equals ChunkID;
4. local capacity/policy allows storage.

Remote GET is content-addressed by ChunkID.

### 2. Optional policy (Provisional validation)

Nodes or storage providers enabling provisional validation policy locally may
optionally admit and stage chunks upon receiving sufficient eligible Validation
signatures (`Authority > 1,000,000` in latest finalized state) authorizing the
candidate RootPublication:

```text
sufficient Validation
-> PROVISIONAL remote admission / cache
-> PoA finality agrees   -> promote provisional admission to canonical finalized
-> PoA conflict / drop  -> purge provisional chunk admission and rollback
```

Provisional admission never counts toward `Protected` durability.

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
normally exists as one more physical copy (Beta: local + 2 remote = 3 physical copies total).

`Protected` in Beta means all required chunks satisfy the 2 independent remote full replicas
target (plus local copy = 3 physical copies total).

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
random selection over proven ProviderIDs.
Providers return stored authorization proofs for audit and state rebuild.
Placement records live in the Identity's encrypted rebuildable Application DB.
