# CYBOU application data plane

Status: CURRENT
Scope: Accepted Local/Network boundary and application storage requirements; implementation evidence is recorded separately.

Recorded status: architecture target for Mail, Files, local indexing and content access.

This document defines the boundary between the network's encrypted physical
storage and the decrypted virtual user experience.

## 1. Independent Local and Network execution

```text
Qt GUI -> LocalApplicationService -> encrypted local.db
                 | LocalContentStager -> common encrypted ChunkBlobStore
                 | immutable durable Outbox / confirmed semantic import
                 v
           NetworkSyncService -> encrypted app.db
                 | ApplicationService / PublicationService / StorageService
                 v
           NodeRuntime: finalized state, PoA, P2P
```

LocalApplicationService owns local Mail folders/read/star flags, drafts, desired
Files state and immutable Outbox intents. Its executor never calls transport,
publication or finality. A local command acknowledges after its local durable
transaction; a queued intent is not a submitted or finalized operation.
LocalContentStager only encrypts, stores and retains local chunks. Content must
be staged before the atomic local acceptance of its immutable intent. Attachment
and upload preparation uses the helper's independent local executor, never the
short-command executor. The draft snapshot commits in short-command order before
preparation is queued. Mail without new local file sources remains on the short
executor. Journal locking is limited to ownership changes; a long content stream
cannot hold it across encryption. Sources remain streamed through
bounded encrypted chunks. Shutdown requests cancellation between source reads
and chunk writes, rolls back unaccepted staging pins and joins preparation before
destroying the local service. This cannot interrupt an OS read already in flight.

A draft edited during preparation survives acceptance of the earlier send payload.
Draft retirement compares the current payload with its durable send fingerprint
under the local-service mutex; retries cannot delete later edits. Content-executor
commits advance the local snapshot revision, rejecting older network snapshots.
Local Files catalog deltas also carry an ordering marker, so a late GUI callback
cannot erase a newer catalog delivered by the other local executor.

The durable active Outbox index excludes protected jobs without deleting their
immutable content or status. Status and active membership commit together.
An older local store builds this index once from retained history, failing closed
on invalid records. PendingOutbox(limit) bounds payload decoding in acceptance
order. The current network pass consumes all active jobs: a bounded scheduling
cursor must preserve submission order and avoid starvation behind securing jobs
before a per-pass budget is imposed.

NetworkSyncService independently advances publication jobs, history indexing,
content recovery and remote durability. It releases local locks before network
calls. Stable JobID and exact recorded OperationID survive retries/restarts.
Finalized Files ordering stays canonical; an older acknowledgement cannot clear
newer desired state. Another device is ordinary Identity use, not a protocol
Device registry; conflicts require an explicit product policy.

The Qt adapter bridges semantic snapshots and row changes with session generation
checks. The former shared SessionScheduler is removed. Shutdown drains accepted
local commands before destroying stores; queued network work does not initiate
new I/O during shutdown. Active transport calls still obey transport timeouts.

local.db is indispensable, not a rebuildable network cache. app.db contains exact
publication journals as well as rebuildable indexes: index recovery must preserve
those journals until verified restoration exists. Migration commits destination
records and completion marker atomically, leaves source intact, and fails closed
on missing required rows. Key mismatch never authorizes automatic deletion.
A prepared IdentityRotate durably wraps the same data key for the future keys;
reopening with promoted keys retires old access. Losing access preserves data.

These are accepted requirements, not a claim that all crash, conflict, large-file
and recovery scenarios have passed; see implementation status for scoped evidence.

The Full Node maintains a rebuildable local finalized-event index. It records
public operation coordinates, publication-bearing block heights and KEM package
coordinates by public AccountID/key epoch. It does not index capsule recipients,
decrypted content or private Mail/Files semantics. Coordinates never confer
admission or consensus authority: their source must be a canonical finalized
block. Index entries and the complete-head marker update in the same database
batch as a block commit, including removal of a losing canonical block's entries.
A missing/incomplete index is rebuilt from retained finalized history; absent or
invalid referenced source blocks leave lookup/recovery unavailable rather than advancing
application checkpoints. This changes no wire or canonical state encoding.

Application recovery walks publication-bearing heights through this shared
index, tries capsules locally and still checkpoints each relevant block together
with its private records. The scan budget counts relevant blocks (up to 256 per
indexed batch); it can advance across unrelated heights without decoding them.


### ApplicationService

Owns inbound discovery and private application indexing:

```text
finalized history
-> scan RootPublications
-> try Identity capsules
-> fetch/decrypt accessible roots
-> interpret private application schemas
-> update Identity Application DB
```

### PublicationService

Owns outbound private publication:

```text
private Mail/Files intent
-> encrypted local content trees
-> RootPublication
-> IdentityOperationCoordinator
-> PoA finality
```

It may use local helpers for bundle staging and proof construction, but those
helpers are not protocol entities.

The implemented `PublicationService` turns a prepared bundle into a
RootPublication with an optional recipient capsule and a mandatory self
capsule, persists the exact publication intent in the Application DB before
signing, and submits through `IdentityOperationCoordinator`. Resume never
builds a replacement operation while the recorded one is unresolved. Job
phases are WAITING_FINALITY, SECURING (finalized, awaiting remote durability),
PROTECTED, and NEEDS_ATTENTION.

### StorageService

Owns content transport and durability:

```text
PUT / GET
local encrypted cache
provider selection
remote replication
health checks
audit
repair
```

No separate user-visible provider layer exists.

`StorageService` places only finalized publications: the ordered chunk list
must reproduce the publication's chunk-authorization root, and per-chunk proofs
are rebuilt from it.
Each chunk goes to distinct CSPRNG-selected CYBOU P2P storage providers until the remote
target is met (1 in development, 2 in Beta; plus local copy = 3 physical copies total);
STORED and ALREADY_STORED both count, the local copy never does.
Providers are distinct by StorageId (the hash of the provider key proven in
the CYBOU P2P handshake), not by address:port, so one key answering on several
endpoints is one replica.
Placement records live in the Identity's encrypted Application DB as an
operational cache. `Audit` re-reads every recorded replica, drops missing or
BLAKE3-mismatching ones and repairs from any valid copy. `Fetch` returns the
local blob or the first valid provider copy and caches it. A replica counts only
with a provider-signed storage receipt; background audits use random-offset
challenges and periodic full GET/hash verification.

## 2. Common ChunkStore

The physical CYBOU ChunkStore is network infrastructure.

```text
ChunkID -> exact encrypted stored bytes
```

ChunkID is BLAKE3-256 of those exact bytes.

The store does not classify a blob as:

```text
mine
foreign
Mail
File
attachment
provider-only
```

The same physical blob may be useful to the local Identity and simultaneously
be retained because this node is a provider. The bytes remain one
content-addressed blob.

Provider/network metadata and local lifecycle pins are separate from the bytes.

### Retention and garbage collection

A node-local `ChunkRetentionRegistry` records why a local blob stays, without
any application semantics:

```text
pin:   (holder, reference) -> ChunkIDs   never evicted
cache: ChunkID -> last use               evictable, least recently used first
```

Holder and reference are opaque 32-byte tags (for example an Identity and one
of its publication jobs). Provider obligations remain FinalizedChunkStore
admission records.

- Staged publication chunks are pinned before the job records its leaves:
  until remote durability the local copy is the only copy.
- If the private leaf-order write fails after pinning, staging releases the
  pin before returning the error. A caller may cancel an abandoned queued job
  that has never been signed; an operation with pending or uncertain finality
  cannot be cancelled. Explicitly rejected work may be forgotten safely.
- When the job becomes PROTECTED the pin is released and the chunks become
  cache entries.
- Blobs fetched from providers, or read locally, are cache entries.

Garbage collection deletes a blob only if it is a cache entry, pinned by no
reference, not provider-admitted (checked atomically with admission) and
unused for the grace period (10 minutes). Eviction runs least recently used
first, bounded per pass, until the cache fits its budget (desktop default
2 GiB). A blob the registry has never recorded is never deleted.

The GUI has no API to enumerate the ChunkStore.

## 3. Identity Application DB

The implemented `PrivateApplicationStore` provides an encrypted per-record
LevelDB substrate at `identities/<AccountID>/app.db`. Record names are keyed
before persistence, values use authenticated encryption, and access requires
the original unlocked `CybouKeyStore`. It is a rebuildable projection; the
Mail/Files schemas and scanners still need to be layered on top.

Each unlocked Identity has a private encrypted rebuildable database, conceptually:

```text
identities/<AccountID>/app.db
```

It contains semantic private projections such as:

```text
Mail:
  Inbox / Sent
  message metadata
  attachment references
  read/archive/local-star state

Files:
  item tree
  names/folders
  logical sizes
  private content references
  versions/trash state

Application:
  accessible publication records
  publication/storage state
  local search/index data
```

Wallet balance, System Balance, `.cybou` names and Authority are canonical
state and need not become a second authoritative copy in this DB.

The Application DB is a cache/projection. Deleting it must not destroy protocol
Identity or the only recovery path for Mail/Files.

## 4. GUI view

The user sees decrypted semantic state after Identity unlock:

```text
Mail/
  Inbox/
    Alice — Project files

Files/
  Work/
    report.pdf
```

but CYBOU physical storage contains only encrypted chunks.

Plaintext exists only:

- in bounded process memory while used;
- in an explicit user-selected destination when the user downloads/exports it.

CYBOU never keeps a parallel plaintext file tree under its data directory.

## 5. Publication discovery

For every finalized `AuthorizedRootPublication`, ApplicationService uses outer
authorization as authoritative publisher identity and tries only relevant
recoverable KEM epochs.

```text
capsule unwrap fails -> ignore
capsule unwrap succeeds -> accessible publication
```

Do not create permanent `NOT_FOR_ME` records in the Identity Application DB.

Accessible roots are fetched independently of scan progress. A temporarily
unavailable root must not block scanning later finalized blocks.

The implemented `ApplicationService` persists `last_scanned_height` after each
block and records an accessible publication (publisher, sender nonce/epoch,
root and content key) before indexing it, so a crash resumes as a retry.
Processing is idempotent per OperationID. Capsules open inside
`CybouKeyStore`; KEM seeds never leave it. A root that cannot be fetched is
`TEMPORARILY_UNAVAILABLE` and retried on every scan; an opened root that is not
a valid private document for this Identity is `INVALID` and never retried.
Mail is Inbox when addressed to this Identity and Sent when published by it;
Archive, Trash, read and star are local mailbox state. Files mutations are
accepted only from the owner and applied per item by canonical order
`(finalized height, operation index, mutation index)`, so an out-of-order
retry never overrides a newer mutation.

## 6. Private application schemas

Private schema/type is encrypted application metadata, never a consensus
operation type.

Initial private schema families:

```text
MAIL_MESSAGE
FILES_MUTATION_BATCH
IDENTITY_RECOVERY_BRIDGE
```

The implemented codecs use fixed-order bounded binary schema. The first field are the private schema type; unknown types and
trailing bytes are rejected. The three types are Mail (1), Files mutation batch
(2), and Identity RecoveryBridge (3). They are placed only inside encrypted
application content, never in a consensus operation or public chunk metadata.

Mail carries a random message ID, optional reply ID, recipient AccountID,
client timestamp, subject, body and bounded attachment references. The sender
comes from the outer authorized publication. Files carries only full-item
upserts and item deletes; a null parent means root and the reserved all-FF
parent denotes trash. RecoveryBridge carries the AccountID, next key epoch and
ordered historical X-Wing seeds. Import must verify each recovered seed
against its historical canonical KEM commitment before use.

Generic file/attachment content does not need a `FILE_BLOB` application schema;
it is simply an encrypted ROOT/INDEX/DATA content tree referenced privately by
Mail or Files metadata.

## 7. Publication bundles

One RootPublication may authorize chunks from multiple local encrypted trees.

Example:

```text
attachment A tree
attachment B tree
Mail main root
      |
      v
one global durable staging order
one chunk_authorization_root
one RootPublication
```

`root_chunk_id` identifies the main private application root. Child
RootChunkIDs and child ContentKeys remain encrypted inside that main root.

`PublicationBundle` is an implementation abstraction only. Do not introduce a
BundleID, bundle wire format or bundle consensus registry.

PublicationService directly streams child trees and the main tree into the
common local ChunkBlobStore, pins their unique chunks and saves one ordered
leaf list in the encrypted Application DB. The staging-attempt marker enables
cleanup of interrupted work without deleting committed jobs or pending intents.
A transient Merkle tree generates proofs individually for upload. No persisted
proof index or intermediate staging service exists. Remote admission still
requires a finalized RootPublication.

## 8. Self capsule

Every publication required for clean owner recovery includes a self capsule.

One-recipient Mail:

```text
recipient capsule
self capsule
```

Files mutation publication:

```text
self capsule
```

The self capsule allows a restored Identity to rebuild Sent Mail and Files
without its old Application DB.

## 9. Files mutation model

Files uses canonical finalized publication order as its private event order.

Keep the persistent mutation grammar minimal:

```text
UPSERT_ITEM
DELETE_ITEM
```

`UPSERT_ITEM` contains the current private item state required to create or
update an item, including parent, name and optional content reference.

Thus:

```text
create      -> UPSERT_ITEM
rename      -> UPSERT_ITEM
move        -> UPSERT_ITEM
new version -> UPSERT_ITEM
copy        -> UPSERT_ITEM with new item_id
trash       -> UPSERT_ITEM with trash parent
restore     -> UPSERT_ITEM with restored parent
delete      -> DELETE_ITEM
```

Starred, Recent, list/grid preference and access history remain local unless a
later explicit sync feature is designed. Starred is persisted as encrypted
local state in the Identity Application DB (never published), so it survives
restarts but not a clean restore.

"Available offline" is derived from real local chunks, never from past
actions: the file's content tree must enumerate from local ROOT/INDEX blobs
and every DATA chunk must be present in the local ChunkStore. It turns false
once cache eviction removes any of them.

### Freeing network storage (DEC-271, DEC-272)

`DELETE_ITEM` only changes the private catalog. Network storage is freed by
`RevokePublication` of an own publication that nothing needs any more. The
client decides this only from a complete Application DB index, with every own
publication job finalized, and keeps a publication while:

- any catalog record, live or deleted, came from it (a delete must keep
  replaying over the older upsert during a restore);
- a message it carries is not deleted forever;
- the content tree of any live file or attachment is among its authorization
  leaves (a file saved from Mail keeps the mail publication).

Recovery bridges are never revoked. One revocation is in flight at a time, and
the last operations of the window stay free for the user. Restores skip revoked
publications. Deleting an own sent message forever therefore also removes it
from the network for its recipient, who keeps only what was already downloaded.

## 10. Mail model

A minimal private Mail message contains:

```text
message_id
optional reply_to_message_id
recipient_account_id
timestamp
subject
body
attachments[]
```

Attachment reference contains private filename, logical size, RootChunkID and
ContentKey.

Sender truth comes from the outer authorized RootPublication AccountID, not
from a decrypted `from` string.

## 11. Storage and retrieval

When content is needed:

```text
need ChunkID
-> local ChunkStore?
   yes: verify/read
   no: query discovered storage peers
       -> verify BLAKE3
       -> store encrypted bytes locally
       -> continue tree reconstruction
```

Provider placement state is operational cache, not recovery-critical state.

After clean restore the client may know content RootChunkID/ContentKey but not
the previous provider set. It may query discovered providers by ChunkID,
retrieve any valid copy, then re-establish required durability.

A DHT/provider directory is not required for the first Beta-scale network.

## 12. Durability

Development target:

```text
1 remote full replica
```

Beta target:

```text
2 independent remote full replicas
```

The local encrypted copy does not count toward the remote target, but it
normally exists as one more physical copy (Beta: local + 2 remote = 3).

Erasure coding is disabled for Beta.

A local encrypted copy may later be evicted after remote durability is healthy;
the semantic file/message remains visible in the Application DB and is fetched
again on demand.

## 13. Recovery invariant

A clean installation with the current mnemonic and public network data must be
able to reconstruct:

```text
canonical Identity/Wallet/Names
historical recoverable KEM epochs
accessible RootPublications
Mail
Files
```

without the previous Application DB or previous provider-placement DB.

Across IdentityRotate this holds through the RecoveryBridge:

```text
PublishRecoveryBridge(new entropy)
  every known KEM seed, capsules for current key + next-epoch key
-> PoA finality -> PROTECTED (remote durability)
-> VerifyRecoveryBridge(new entropy) opens and decodes it
-> only then RotateIdentitySync
```

On restore, ApplicationService opens the bridge with the current key, accepts
each historical seed only if it reproduces that epoch's canonical KEM package,
imports it into the key store (memory only) and rescans once, so pre-rotation
publications open. `PublishRecoveryBridge` refuses to omit a published epoch
whose seed this device has not recovered.

## Typed private binary layouts (schema)

Every document begins `type:u8`; types are Mail=1, Files=2,
RecoveryBridge=3. IDs and keys use their fixed 32-byte representation. All
integers below are LE. `optional(T)` is a strict presence byte (0/1), then T
when present. `text` is u32 byte length followed by valid UTF-8.

```text
Mail:
  message_id:32, reply:optional(32), recipient_account_id:32, timestamp:u64
  subject:text<=1024, body:text<=131072, attachment_count:u16<=32
  each: attachment_id:32, filename:text<=255, logical_size:u64
        media_type:optional(text<=127), root_chunk_id:32, content_key:32
Files:
  mutation_count:u16 (1..512)
  each: kind:u8 (UPSERT=1, DELETE=2), item_id:32
  UPSERT adds: parent:optional(32), item_kind:u8 (FILE=1, FOLDER=2)
    name:text<=255, logical_size:u64, root:optional(32), key:optional(32)
    modified_ms:u64
RecoveryBridge:
  account_id:32, next_key_epoch:u64, seed_count:u16 (1..64)
  each: historical_epoch:u64, X-Wing seed:32
```

Existing semantic validation remains mandatory: unique IDs, valid filenames,
nonzero required fields, matched root/key presence, folders without content,
and ordered historical epochs below the next epoch. Decoders consume the
entire bounded input. No CBOR or legacy decoder remains.
