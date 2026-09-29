# 83 — CYBOU Files UI/UX

Status: canonical Beta Files/Storage product UX contract. The Qt Files page
implements this UX over the desktop product model and is fully usable with
deterministic UI fixtures (`CYBOU_UI_FIXTURE`). The encrypted Files catalog,
its finalized RootPublication and the Beta durability contract are not
connected yet; in live mode the page says so and never shows local-only
content as `Protected`.

This document defines how CYBOU exposes encrypted file storage to ordinary
users. Google Drive is the interaction reference for familiar file
management patterns; CYBOU does not copy Google branding, provider account
semantics, or centralized trust assumptions.

Files is the Beta file-management product surface for these familiar workflows;
there is no separate post-Beta Drive product. Private catalogs and content
publication use the shared encrypted chunk substrate described in
`ROOT_PUBLICATION.md` and `ENCRYPTED_CHUNK_TREE.md`.

The user manages files and folders. The user does not manage chunks, provider
nodes, repair queues, proofs, leases, or replication topology.

## 0. Content lifecycle (finality first)

Files uses the same finality-first lifecycle as Mail (`82_MAIL_UI_UX.md` §0):

```text
file selected
-> chunk/encrypt locally
-> private Files catalog/root prepared locally
-> RootPublication finalized by PoA
-> finalized-authorized chunks uploaded to storage providers
-> durability threshold reached
-> Files item becomes Protected
```

There is no remote upload of unfinalized content. Until the RootPublication is
finalized, ciphertext chunks remain local staging only; providers accept only
chunks with a valid finalized-publication admission proof.

Status terms are distinct and must never share one indicator:

```text
Finalized    = the RootPublication is part of canonical PoA history
Authorized   = its ChunkIDs are admitted for storage by that finalized publication
Available    = the chunks can actually be retrieved from the network
Protected    = the product durability threshold has been reached
Retrievable  = this client has fetched and verified the content
```

`Finalized` is not `Protected`; finality only authorizes storage admission.

See also:

- `81_BETA_PRODUCT_SCOPE.md` — Beta product boundary;
- `STORAGE_ADMISSION.md` — finalized-content availability gates;
- `ENCRYPTED_CHUNK_TREE.md` — encrypted content substrate;
- `82_MAIL_UI_UX.md` — Mail attachment integration;
- `84_PRODUCT_DESIGN_SYSTEM.md` — shared visual and interaction rules.

## 1. Product objective

A Beta user should be able to:

```text
open Files
-> drag a file/folder into CYBOU
-> see Preparing / Waiting for confirmation / Securing / Protected
-> close/restart CYBOU
-> see the file still present
-> download and open it
-> organize it with normal file-management actions
```

For Mail integration:

`Save to Files` creates an independent Files ownership and retention reference
to the protected content. Removing or expiring its source Mail message must not
remove that Files item.

```text
receive encrypted attachment
-> Save to Files
-> attachment appears immediately as a Files item
```

reusing existing protected content/chunks when authorization and retention
semantics permit.

## 2. Interaction reference

Use Google Drive as an ergonomic reference for:

- persistent left navigation;
- strong global/local search;
- List/Grid views;
- drag & drop;
- contextual actions;
- upload progress;
- familiar folder navigation;
- Recent / Starred / Trash concepts;
- clear storage usage.

CYBOU retains its own visual identity and security language.

## 3. Beta information architecture

Required normal navigation:

```text
My files
Recent
Starred
Trash
Storage
```

Capability-gated Beta navigation:

```text
Shared with me
Shared by me
```

The target share principal is a `.cybou` AccountID. A grant wraps the content key
to that account's current Identity KEM capability and binds READ or WRITE
permission. READ is the first share mode; WRITE requires a separately reviewed
Identity authorization and encrypted grant flow. No
public-link bearer token is the default access mechanism. Do not show sharing
controls until the relevant grant flow is implemented.

## 4. Main Files screen

List view:

```text
+--------------------------------------------------------------------------------+
| CYBOU Files      Search files...                                  stan.cybou   |
+----------------+---------------------------------------------------------------+
| + New          | My files                                    List | Grid       |
|                |                                                               |
| My files       | Name                Size       Modified       Status          |
| Recent         | Documents           —          Today          Protected       |
| Starred        | report.pdf          4.2 MB     Today          Protected       |
| Trash          | photo.jpg           8.1 MB     Yesterday      Protected       |
|                |                                                               |
| Storage        |                                                               |
| 12 / 100 GB    |                                                               |
+----------------+---------------------------------------------------------------+
```

Grid view uses the same data/actions, not a different feature set.

## 5. Normal actions

Beta Files should support ordinary file-manager actions where backend semantics
exist:

```text
New folder
Upload file
Upload folder
Download
Rename
Move
Star / Unstar
Move to Trash
Restore from Trash
Delete according to retention rules
Open details
View version history
Share with a `.cybou` identity (capability-gated)
Send by CYBOU Mail
```

Sharing follows the AccountID-based grant model frozen in DEC-194. The target
permission is visible in user language; a grantee receives the content key via
their current Identity KEM capability. Anonymous links are a later optional
feature and are not the security foundation.

## 6. Folder model

Folders are a user-facing organization abstraction. They must not force a
central plaintext directory into consensus or provider-visible metadata.

Names and hierarchy live in the encrypted Files catalog, published as
encrypted payload of a RootPublication, which maps human names to private
content references.

Provider-visible storage data must not reveal ordinary filenames or local
folder paths. The owner's client must decrypt and display those names and
folders; privacy from providers does not mean hiding a user's own metadata
from the Files page.

## 7. Upload flow

User action:

```text
drag report.pdf into Files
```

Internal work:

```text
read locally
-> generate content key material
-> encrypt/chunk locally (local staging only)
-> prepare the updated private Files catalog/root locally
-> submit one RootPublication and obtain PoA finality
-> upload the finalized-authorized chunks
-> reach the durability threshold
-> Files item becomes Protected
```

Normal UI states (same model as Mail):

```text
Local                     = present only on this device
Preparing                 = local encryption/chunking
Waiting for confirmation  = RootPublication submitted, awaiting PoA finality
Securing                  = finalized; authorized chunks spreading to storage (e.g. 2/3)
Protected                 = durability threshold reached
Temporarily unavailable   = Protected content currently cannot be retrieved; retrying
Needs attention           = user action or explicit rejection
```

`2/3` is an example presentation of a protocol-defined durability target, not
a frozen replication parameter. If the protocol later uses erasure coding or a
different durability model, the normal wording should remain outcome-oriented.

The client retains local ciphertext until the item is Protected and resumes
Waiting for confirmation or Securing after restart.

## 8. Durability semantics

The normal success state is:

```text
Protected
```

Advanced details may expose:

```text
RootChunkID
ciphertext size
placement count / coding status
last audit
repair state
lease/retention
integrity commitment
```

Do not call a file Protected until it is finalized and the Storage core reports
the required minimum durability state.

## 9. Download flow

Normal states:

```text
Downloading 42%
Verifying
Decrypting
Ready
```

Integrity verification precedes plaintext release to the destination path.

If the content is temporarily unavailable:

```text
Temporarily unavailable — retrying
```

If durability repair is underway but retrieval is still possible, the user may
continue to download; repair status belongs in details unless user action is
required.

## 10. File metadata and privacy

Normal Files metadata includes:

```text
filename
folder
size
modified time
starred state
local/user-facing type icon
```

Provider-visible data should be limited to what the Storage protocol requires
for opaque chunk admission, placement, integrity, lease, audit, and repair.

Do not intentionally expose:

```text
plaintext filename
folder path
MIME type
owner contact graph
mail subject
content key
plaintext hash usable for cross-user confirmation
```

to Storage providers unless a future protocol explicitly requires and justifies
it.

## 11. Content identity

The UI never asks users to work with content or chunk identifiers.

Normal:

```text
report.pdf
```

Advanced:

```text
RootChunkID: ...
Integrity: verified
```

ChunkIDs are BLAKE3 hashes of encrypted bytes, so predictable plaintext does not
create an obvious provider-side confirmation oracle. The UI contract is built
around files and protected content, not around RootChunkID.

## 12. Storage capacity

Normal presentation:

```text
Storage
12.4 GB used of 100 GB
```

A secondary details panel may explain contribution/entitlement policy.

Do not mix the user-facing logical capacity display with physical replication
or coding overhead. `3:1 contribution : entitlement` is an economics/accounting
policy and is not the same thing as three physical replicas.

## 13. Mail integration

Mail and Files are one product surface over the same finality-first encrypted
content lifecycle.

Compose attachments are prepared locally and published together with the
message in one RootPublication (`82_MAIL_UI_UX.md` §9); there is no separate
attachment upload before Mail finality.

Reader:

```text
Download
Save to Files
```

```text
Mail attachment
    ↓
Save to Files
    ↓
new private Files catalog reference
    ↓
existing encrypted content may be reused
```

`Save to Files` creates an independent Files catalog reference to existing
encrypted content when authorization and retention semantics permit. No
download and re-upload of ciphertext is required, and removing the Mail message
does not remove the Files item. The UI must not expose the underlying
optimization.

```text
Files item
    ↓
Send by CYBOU Mail
    ↓
Mail private schema references existing protected content
```

`Send by CYBOU Mail` places a private content reference and content key inside
the encrypted Mail root. It does not copy ciphertext when authorization and
retention semantics permit reuse.

## 14. Recent

Recent is a local/private view over user activity such as:

```text
uploaded
opened/downloaded
saved from Mail
renamed/moved
```

Do not require global publication of access history.

## 15. Version history

A file revision is new immutable encrypted content. The encrypted Files
catalog links revisions and identifies the current version; consensus does not
store a row per revision. Restoring an earlier revision changes the catalog
pointer and retains the immutable content under Storage policy. Version history
controls stay hidden until the catalog and retention behavior are implemented.

## 16. Starred

Starred is local/private metadata in the current product scope. It is not
consensus state.

## 17. Trash and deletion

Trash should match familiar file-manager expectations while remaining honest
about distributed retention:

```text
Move to Trash
Restore
Delete
```

A normal Delete action must not claim immediate physical erasure from every
provider if leases/repair/retention make that untrue.

User-facing copy can say:

```text
Removed from your Files. CYBOU will release retained storage according to the
Storage retention policy.
```

Exact cryptographic erasure and provider-GC semantics are owned by the Storage
protocol documents.

## 17. Search

Search should be local over decryptable/private metadata where possible:

```text
filename
folder
local tags if implemented
```

Do not leak plaintext search terms to Storage providers.

## 18. Offline behavior

When offline, the Files page remains browsable from locally available metadata
and cache.

Actions that require network availability show an honest queued/offline state;
they do not pretend the file has been protected remotely.

Example:

```text
Offline — will be secured when CYBOU reconnects
```

Only introduce durable offline queues when the backend can persist and resume
them safely.

## 19. Storage health

Normal user vocabulary:

```text
Local
Preparing
Waiting for confirmation
Securing
Protected
Repairing protection
Temporarily unavailable
Needs attention
```

Advanced diagnostics may expose:

```text
provider count
placement set
lease expirations
audit results
repair queue
ChunkIDs / RootChunkID
transfer peers
```

## 20. Contribution/provider mode

Contributing disk capacity is an advanced/service setting, not part of normal
file browsing.

Suggested path:

```text
Settings
-> Storage contribution
```

It may show:

```text
Capacity contributed
Capacity verified
Current obligations
Audit health
Network service status
```

Normal users who only consume their entitlement should not have to understand
provider operation.

## 21. Performance contract

File list rendering, navigation, rename, star, selection, and menus must never
wait on network transfer or proof/audit work.

Uploads/downloads run asynchronously and report progress through model state.
Large directories should use incremental/lazy rendering where needed.

## 22. Responsive behavior

At wide widths the page uses a stable left navigation and full list/grid.
At narrower supported desktop widths:

- sidebar may collapse to compact icons or a drawer;
- columns with low value may hide before introducing horizontal page scroll;
- primary filename/status/actions remain visible;
- details open as a side panel or separate view.

Validate at 1040, 1280, 1600, and 1920 pixel widths plus common Windows DPI
scales.

## 23. Beta Files UX acceptance

A Beta candidate passes when a new user can, without Storage terminology:

```text
[ ] open Files and understand the page immediately
[ ] create a folder
[ ] drag/drop a file and understand progress
[ ] know when the file is Protected
[ ] restart CYBOU and still see the file
[ ] download, verify and decrypt the file
[ ] rename, move, and copy it
[ ] star/unstar it
[ ] preview a file and inspect version history when available
[ ] share a file with a `.cybou` identity when sharing is enabled
[ ] send a file by CYBOU Mail without re-uploading it
[ ] move it to Trash and restore it
[ ] save a received Mail attachment to Files
[ ] understand Storage used/available
[ ] find advanced durability details only when requested
```

The user must never need to manually select providers, chunks, replicas,
leases, chunk IDs, proofs, or cryptographic algorithms.
