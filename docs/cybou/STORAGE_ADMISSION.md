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

## Storage admission (finality-first)

A provider accepts:

```text
PutChunk(publication_reference, ChunkID, bytes, admission_proof)
```

only after verifying:

1. publication exists in canonical PoA-finalized history;
2. Merkle proof authorizes ChunkID under that publication;
3. BLAKE3(bytes) equals ChunkID;
4. local capacity/policy allows storage.

Remote GET is content-addressed by ChunkID.

Validation signatures never authorize remote chunk admission. There is no
provisional storage admission.

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
random selection over proven StorageIds.
Providers return stored authorization proofs for audit and state rebuild.
Placement records live in the Identity's encrypted rebuildable Application DB.

StorageId is proved on demand with STORAGE_PROOF_REQUEST (26) and
STORAGE_PROOF (23), bound to both HELLOs, TLS exporter and a fresh challenge.
The STORAGE key retains purpose value 8 and its original derivation domain.
No role is advertised. PublicationService retains ordered authorization leaves
in the encrypted Application DB; StorageService builds transient Merkle levels
and generates each proof on demand. Production nodes allocate a positive quota automatically (or accept an explicit
positive quota). Zero is limited to memory-only unit tests. Automatic allocation
uses 10% of space remaining after a reserve of max(1 GiB, 5% of filesystem size),
clamped to 64 MiB–20 GiB. Admission preserves the same reserve and fails closed
on unavailable disk-space information. Existing replicas survive quota reductions.
Block sync, candidate relay and Validation transport remain operational.

## Notarial object register and deterministic quotas

The blockchain acts as the canonical Notarial Register for application content:
- Tracks `RootPublication` metadata, Merkle roots, recipient capsules, and verified placements.
- Remote storage allowances and admission rights are computed deterministically strictly from PoA-finalized state.
- Local configuration declarations and off-chain vouchers convey zero authority. The network only respects what is notarized and finalized by PoA.

## Onboarding trust credit and 1:3 reciprocal ratio

Every newly registered Identity receives an immediate **Onboarding Trust Credit of 5 GB** of remote storage in the network (DEC-269).
- **Physical ratio (1:3)**: 1 GB of stored user data requires 2 remote replicas plus 1 local copy = 3 physical copies total.
- **Reciprocal baseline**: Each Full Node allocates ~10–15 GB of local storage to house reciprocal chunks for other network peers.
- Frictionless onboarding: new users immediately store files and send mail without waiting to accumulate reputation.

## AUTH resource ladder

Remote storage allowances scale according to finalized Identity Authority (DEC-268, DEC-272).
Block execution refuses a RootPublication whose `chunk_count` exceeds the author's largest
file or whose author's register total would exceed the quota (512 KiB per chunk):

| Tier | AUTH | Ops / block | Ops / epoch (1024 blocks, ~17 min) | Network storage | Largest file | Relay PoW |
|---|---|---|---|---|---|---|
| T0 | < 10,000 | 1 | 30 | 5 GiB | 1 GiB | 22 bits |
| T1 | >= 10,000 | 5 | 150 | 25 GiB | 4 GiB | 21 bits |
| T2 | >= 100,000 | 25 | 750 | 100 GiB | 16 GiB | 20 bits |
| T3 | >= 1,000,000 | 100 | 3,000 | 500 GiB | 64 GiB | 19 bits |
| Validator | > 10,000,000 | 1,000 | 30,000 | 2 TiB | 256 GiB | 18 bits |

`RevokePublication` removes the record from the register and frees its chunks from the
quota. A revoked publication no longer authorizes admission, and providers purge every
chunk no other admitted publication still authorizes.

## Mutual proof of storage and uptime auditing

Peers storing chunks for each other perform periodic mutual cryptographic audits (DEC-270):
1. **Challenge**: Storing peer A sends a randomized challenge (byte offset, length, salt/nonce) to peer B holding its chunk.
2. **Response**: Peer B computes a deterministic cryptographic proof over the exact stored chunk bytes and returns it (`StorageAuditChallenge`, `CreateStorageAuditProof`, `VerifyStorageAuditProof` in `src/cybou/storage_audit.h`).
3. **Notarization & Reliability** (*Target architecture / Unimplemented*): Verified challenge proofs and uptime attestations are planned to be notarized in PoA blocks, feeding peer reliability coefficients and maintaining active storage allowances. (Currently, only the cryptographic audit primitive is implemented; P2P challenge relay, PoA notarization, and reliability state are not yet implemented).

## State synthesis and object pruning

Block finalization synthesizes transaction history into active state (`CybouState`) (DEC-271):
- **Object deletion**: When a user deletes a file or mail, an authenticated `RevokePublication` operation is published and finalized.
- **State compaction**: The active publication record is tombstoned and pruned from active state.
- **Local garbage collection**: Storing peers observing publication revocation immediately purge the associated chunks from their local `ChunkStore`, freeing disk space and preventing dead storage accumulation.
