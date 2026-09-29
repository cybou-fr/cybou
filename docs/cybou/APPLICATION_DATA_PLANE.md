# CYBOU application data plane

Status: architecture target for Mail, Files, local indexing and content access.

This document defines the boundary between the network's encrypted physical
storage and the decrypted virtual user experience.

## 1. Four layers

```text
Qt GUI
  |
  v
Identity Application DB / product model
  |
  v
ApplicationService / PublicationService / StorageService
  |
  v
NodeRuntime: finalized state, PoA, P2P
  |
  v
common encrypted ChunkStore
```

The number of architectural services is intentionally small.

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
phases are WAITING_FINALITY, SECURING (finalized, awaiting remote durability)
and NEEDS_ATTENTION.

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

The implemented `StorageService` places only finalized publications: the
ordered chunk list must reproduce the publication's chunk-authorization root,
and per-chunk proofs are rebuilt from it. Each chunk goes to distinct
CSPRNG-selected CYP2 storage peers until the remote target is met (1 in
development, 2 in Beta); STORED and ALREADY_STORED both count, the local copy never does.
Placement records live in the Identity's encrypted Application DB as an
operational cache. `Audit` re-reads every recorded replica, drops missing or
BLAKE3-mismatching ones and repairs from any valid copy. `Fetch` returns the
local blob or the first valid provider copy and caches it. No signed storage
receipts exist yet; ACK plus periodic GET/hash verification is the first
durability path.

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

The implemented v1 codecs use strict canonical CBOR arrays. The first two
fields are the private schema type and version; unknown types, versions and
extra fields are rejected. The three types are Mail (1), Files mutation batch
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

The implemented `PublicationBundleStager` streams each tree into the common
local ChunkBlobStore and appends its staged chunks to one durable authorization
proof index. It can reopen a completed child-tree index to append the main
tree. An interrupted tree marks that local index unusable until discard; it
never authorizes remote provider storage before publication finality.

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
later explicit sync feature is designed.

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
