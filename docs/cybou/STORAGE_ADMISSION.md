# Finalized chunk storage admission and durability

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Recorded status: active admission substrate plus frozen durability architecture target.


## Storage implementation evidence boundary

The Beta target remains two independent remote full replicas; the current
placement algorithm deduplicates proven StorageIds, which does not establish
independent hosts, operators or failure domains. The 1:3 reciprocal baseline
is a capacity/service objective, not measured proof of contribution: automatic
local capacity is an explicit operator choice (`V >= 15 GiB`), not proof of service.
Storage is paid by finalized leases; only PoA-signed settlements record service.

Canonical state currently records publications, roots and recipient capsules,
not provider placements or audit reliability. Mutual-audit transport, PoA
notarization and canonical reliability coefficients are unimplemented target
work requiring an evidence/privacy/accounting design before implementation.
Off-chain storage evidence (DEC-276) consists of provider-signed receipts,
random-offset audits and periodic full GET plus ChunkID verification; none of
it is consensus state or a canonical proof.

Finalized revocation stops admission and closes the author's lease after the current period.
Compliant providers journal purge of unshared chunks, retaining physical byte
accounting until unlink succeeds or absence is confirmed; maintenance/restart
retry failures. This does not prove deletion of hidden copies or crypto-erasure.
See [`DATA_ASSURANCE_AND_ERASURE.md`](DATA_ASSURANCE_AND_ERASURE.md) for scoped regression evidence.

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

1. publication is active in the latest canonical PoA-finalized state and its
   StorageLease covers the current settlement period (DEC-279);
2. Merkle proof authorizes ChunkID under that publication;
3. BLAKE3(bytes) equals ChunkID;
4. local capacity/policy allows storage.

Remote GET is content-addressed by ChunkID.

There is no provisional storage admission.

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

StorageService selects eligible remote providers with distinct proven StorageIds using secure
random selection over proven StorageIds.
Providers return stored authorization proofs for audit and state rebuild.
Placement records live in the Identity's encrypted rebuildable Application DB.

StorageId is proved on demand with STORAGE_PROOF_REQUEST (26) and
STORAGE_PROOF (23), bound to both HELLOs, TLS exporter and a fresh challenge.
The STORAGE key retains purpose value 8 and its original derivation domain.
No role is advertised. PublicationService retains ordered authorization leaves
in the encrypted Application DB; StorageService builds transient Merkle levels
and generates each proof on demand. Production nodes use an explicit local capacity `V >= 15 GiB` (default
15 GiB; headless `--capacity`; DEC-275). The whole ChunkBlobStore (own staging,
cache and provider replicas) is bounded by `V`; finalized provider obligations
are bounded by `floor(2V/3)`. Values below 15 GiB and zero are limited to
memory-only unit tests. Admission also preserves a disk reserve of
max(1 GiB, 5% of filesystem size) and fails closed on unavailable disk-space
information. Existing replicas survive capacity reductions.
Block sync and candidate relay remain operational.

## Notarial object register and deterministic quotas

The blockchain acts as the canonical Notarial Register for application content:
- Tracks `RootPublication` metadata, Merkle roots, recipient capsules. Provider placements remain local encrypted metadata.
- Remote storage allowances and admission rights are computed deterministically strictly from PoA-finalized state.
- Local configuration declarations and off-chain vouchers convey zero authority. The network only respects what is notarized and finalized by PoA.

## Mutual proof of storage and uptime auditing

Frozen target, not implemented network behavior: peers would perform periodic mutual cryptographic audits (DEC-270):
1. **Challenge**: Storing peer A sends a randomized challenge (byte offset, length, salt/nonce) to peer B holding its chunk.
2. **Response**: Peer B computes a deterministic cryptographic proof over the exact stored chunk bytes and returns it (`StorageAuditChallenge`, `CreateStorageAuditProof`, `VerifyStorageAuditProof` in `src/cybou/storage_audit.h`).
3. **Notarization & Reliability** (*Target architecture / Unimplemented*): Verified challenge proofs and uptime attestations are planned to be notarized in PoA blocks, feeding peer reliability coefficients and maintaining active storage allowances. (Currently, only the cryptographic audit primitive is implemented; P2P challenge relay, PoA notarization, and reliability state are not yet implemented).

## State synthesis and object pruning

Block finalization synthesizes transaction history into active state (`CybouState`) (DEC-271):
- **Object deletion**: Application deletion changes the encrypted catalog; eligible unreferenced own publications may subsequently be author-revoked and finalized.
- **State compaction**: The active publication entry is removed; historical blocks and their capsules remain.
- **Local garbage collection**: Compliant peers journal managed purge of chunks no other admitted publication authorizes. Failed unlink retains provider byte accounting and is retried at reopen and maintenance. Revocation frees canonical author quota before physical removal; no hidden-copy erasure is proved.

## Storage economy (implemented in M5; active from the M7 genesis)

DEC-274–DEC-283 replaced the former AUTH storage quota, the onboarding credit
and automatic allocation; DEC-284 removed AUTH entirely:

- **Local capacity** (implemented, M2): explicit `V >= 15 GiB`; the
  ChunkBlobStore is bounded by `V` and finalized provider obligations by
  `floor(2V/3)`. Nothing is physically partitioned and `V` is not consensus state.
- **Admission** (implemented): finality-first admission additionally requires an
  active funded StorageLease. Assignment attestation is PoA's off-chain duty.
- **Assignment** (implemented): providers are grouped by verified payout account
  (a StorageId without binding is its own group); groups are taken in uniformly
  random order and a chunk's replicas go to distinct groups, so many StorageIds of
  one account count once. No scores, top-k, capacity weighting or storage-node
  role. Network-side attestation that the payer did not choose remains PoA's
  off-chain duty.
- **Evidence** (implemented, M3): the provider association record is the
  durable obligation; signed StorageReceipt; random-offset audits and one full
  GET in eight with ChunkID recomputation; bounded in-memory rolling evidence,
  off-chain only. Since M4, evidence persists in the encrypted Application DB
  and credits shadow billing-unit-seconds between consecutive successful
  checks (gap capped at 24 h); estimated rent and provider reward move no CYBOU.
- **Settlement** (consensus implemented): daily PoA-signed StorageSettlement
  pays providers from escrow, at most one period's rent and `replicas` payouts per
  lease, never the payer. Failed audit -> no payment, replica degraded, repair.
  Off-chain evidence aggregation into settlements is not implemented yet.
- **Protected**: active publication + active funded lease + two remote
  obligations at distinct storage/economic identities + fresh evidence;
  otherwise `Securing`, `Needs renewal` or `Degraded`.
- **Revocation**: lease CLOSING, final settlement, refund to System Balance,
  managed purge. No quota exists to free.
- **Release gates**: concentration simulation (top-1/top-10 share, effective
  provider count) and Sybil simulation must pass before Beta.
