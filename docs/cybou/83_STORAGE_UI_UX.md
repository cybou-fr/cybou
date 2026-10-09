# 83 — CYBOU Files UI/UX

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Recorded status: canonical Beta Files/Storage product UX contract. The Qt Files page
implements a live encrypted catalog, publication, retrieval and durability
projection through CybouCoreApplicationAdapter as well as deterministic UI
fixtures (`CYBOU_UI_FIXTURE`). Connection is not full Beta acceptance: see
`26_IMPLEMENTATION_STATUS.md` and `DESKTOP_BETA_ACCEPTANCE_PLAN.md` for remaining
interaction and evidence gaps. Local-only content is never `Protected`.

The shell's contextual header search drives the current Files view's filter,
including its existing name/status matching. Clearing it removes that filter;
folder navigation clears the same query. Switching products retains each query;
Identity lock clears private search state. The embedded page has no second
visible search control.

Selection actions appear for one or multiple items and carry across list/grid
views. Star toggles all selected items according to whether all are already
starred. Trash offers Restore instead of Star/Move to Trash. Single selection
also offers Details; Clear selection remains available. Utility icons have
accessible names, tooltips and keyboard focus, and mutation actions require an
active Identity and available Files backend. Permanent deletion keeps its
explicit existing review. These actions do not imply finality or protection.

F2 and Delete act on the selection only while the Files list or grid has
keyboard focus. Retaining a selection while focusing navigation, search or
another control does not make those controls file-mutation shortcut targets.
Trash excludes both selection shortcuts; Restore and permanent deletion retain
their visible actions and permanent-deletion review.

Keyboard context-menu requests in list/grid use the current row/tile rather
than pointer position. The current item is scrolled into view; an existing
selection containing it remains, otherwise that item becomes the selection.
Move and Rename use their existing reviewed/acknowledged paths. The Move folder
selector exposes its existing "Move to" label as an accessible name.

Folder creation, rename, move, copy, Trash and Restore acknowledge the locally
saved publication intent. They report a failed save rather than announcing a
completed change. Move/Trash batches show pending feedback, count saved/failed
items and offer Undo only for saved items. Undo queues the inverse move to each
original folder even if the forward snapshot has not arrived; restoring from
Trash this way retains the original folder. Lock/session replacement invalidates
late replies and retained Undo actions. Network finalization and confirmed remote
durability remain separate from this local acknowledgment.

List, grid, breadcrumbs and navigation drops validate the current Identity,
known item IDs and a usable destination before accepting and again on drop.
Folders cannot enter themselves or descendants; missing/trashed destinations
are refused. Duplicate dragged IDs issue one mutation per item. Internal Files
IDs take precedence over any downloaded-copy URL: a rejected internal move never
silently becomes a new upload. Blank My files space targets the current folder.
External local files still import; non-local URLs alone are not an import.

This document defines how CYBOU exposes encrypted file storage to ordinary
users. Google Drive is the interaction reference for familiar file
management patterns; CYBOU does not copy Google branding, provider account
semantics, or centralized trust assumptions.

Files is the Beta file-management product surface for these familiar workflows;
there is no separate post-Beta Drive product. Private catalogs and content
publication use the shared encrypted chunk substrate described in
`ROOT_PUBLICATION.md` and `ENCRYPTED_CHUNK_TREE.md`.

The normal user manages files and folders. Advanced may inspect owned content
and scoped evidence; StorageService still manages assignment and repair.
Pages never enumerate foreign hosted chunks or the provider DB.

## Stable details and actionable protection (delivery target)

Keep the selected semantic file, details scroll and Advanced expansion stable
across snapshot refresh and pending-to-indexed item replacement. Lock clears
private state. An actual removed/inaccessible item closes details with a reason.
New uploads, folders, copies and Mail attachment saves allocate one random private
item ID before entering the adapter. Pending projection and indexed catalog use
that same ID; no temporary Files ID or replacement alias is retained.
List rows and grid items are reconciled by this ID. Status/metadata changes
update existing items and preserve selection, keyboard current item and scroll.
Sorting or insertion moves retained items rather than recreating the catalog;
removed/filtered items lose their selection. View-mode switches carry the visible
selection. Lock clears private rows, navigation and interaction state. Folder
child counts are aggregated in one catalog pass. Status cells use a viewport
delegate rather than one QWidget per file. Plain semantic status text remains
in display/accessibility roles and tooltips; selection/focus uses the standard
item-view style. Protected uses the lock marker; pending/error states retain
their distinct markers. No status is conveyed by colour alone.
Folder pickers use unique IDs with breadcrumb labels; duplicate names must not
choose the wrong destination. Recursive folder enumeration runs off the UI thread.
Folder import first discovers a bounded tree without creating publications (up to
10,000 entries and 64 directory levels), skipping symbolic links. A read failure
or exceeded limit rejects that discovery before staging. Progress and Cancel
remain available. Successful discovery feeds the existing upload/create commands
in small GUI batches. Cancellation during staging stops remaining commands;
already queued publications keep their normal lifecycle and are not rolled back.
Identity lock/change, Files unavailability or page destruction stops the import.
Discovery progress is an entry count, not an invented completion percentage;
queued counts never imply finality or remote protection.

Show separate fields for confirmation, local availability, remote replicas and
retrieval/integrity. Examples: Waiting for confirmation; Protecting — 1 of 2
remote copies confirmed; Checking copies — no recent measurement; Repairing
protection; Needs attention — with a safe reason and retry. Unknown count is
never rendered as Not stored on network yet. Show measurement freshness and
whether the count is a local observation. Two distinct StorageIds/payout
accounts do not establish separate hosts or failure domains.

Folder creation, rename, move, copy, Trash and Restore contribute their actual
worker command states to the shared header Activity panel described in doc 82.
Queued or Running means local application work; Committed acknowledges saved
publication intent and never means finalized catalog state or protected copies.
Failed commands retain an item link, without a generic mutation retry button.
These volatile task rows coexist with semantic publication/download progress.

Current Files details expose the last local placement/audit attempt time, its
scope (placement, partial audit or full-publication audit), and a bounded typed
reason. These session observations are not canonical storage reliability or a
last-successful-full-copy verification timestamp. They retain at most 1,024
publication observations, are not persisted, and become unknown on restart or
eviction until another attempt occurs. Reading saved placement records never
refreshes the observation time or checks remote availability. Raw transport
errors and provider endpoints are not projected into these reasons.

An observation older than 24 hours receives a descriptive age label; age alone
never changes replica counts, protection state or verified local download access.
An unmet target does not imply that repair is currently running. Unknown counts
remain unknown, including Advanced assurance text. Details explain disabled
downloads separately for active retrieval and missing local/remote availability.

Download/Open/Send capabilities have explicit reasons, consistently across the
toolbar, details and context menu. The existing Files-to-Mail reference path
requires Protected; first make that path reliable and explain missing replicas.
A future compose-from-pending-file flow is separate gated work: define durable
content references, preparation/source lifetime and recovery before enabling it.
Do not bypass the current gate merely to make a disabled action clickable.
Local verified access may be considered separately from remote protection;
core availability, authorization and verification decide whether it is safe.

Deletion separates local Trash, finalized catalog deletion, publication
revocation/lease closure and managed purge of unshared provider chunks. Report
only evidence actually available. Retained recipient/shared copies, historical
capsules and backups prevent a promise of universal deletion or crypto-erasure.
See `DATA_ASSURANCE_AND_ERASURE.md` and `NETWORK_AND_ADVANCED_UX.md`.

## 0. Content lifecycle (finality first)

Files uses the same shared lifecycle as Mail (`82_MAIL_UI_UX.md` §0):

```text
file selected
-> chunk/encrypt locally
-> private Files catalog/root prepared locally
-> RootPublication finalized by PoA
-> finalized-authorized chunks uploaded to storage providers
-> durability threshold reached (2 independent remote replicas)
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
Protected    = the product durability threshold has been reached (2 remote replicas)
Retrievable  = this client has fetched and verified the content
```

`Finalized` is not `Protected`; finality only authorizes storage admission.
Durability (`Protected`) always requires verified PoA finality.

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
-> submit one RootPublication
-> PoA finality
-> upload finalized-authorized chunks
-> reach the durability threshold (2 independent remote replicas)
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
12.4 GB of logical files
System Balance: finalized CYBOU service budget
```

A secondary details panel shows current lease/rent assumptions and separately
the explicit local physical capacity and provider budget. Do not invent a
canonical GB entitlement from local capacity or System Balance. Show an upper
limit only when a real application policy defines and measures it.

Do not mix the user-facing logical capacity display with physical replication
or coding overhead. The reciprocal 1:3 baseline is a capacity/service objective,
not a measured proof of contribution, storage entitlement or three-replica
evidence. Paid publication leases govern service under the active economy.

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

## 20. Reciprocal storage and node health

Storage is intrinsic to every Full Node: each node has an explicit local CYBOU
capacity `V >= 15 GiB` (default 15 GiB), of which at most `floor(2V/3)` serves
provider obligations and the rest is a local reserve (DEC-275). Network storage
is paid by lease from System Balance (DEC-279). Inspecting local storage allocation,
uptime, and mutual audit health is an advanced setting, not part of normal file browsing.

Suggested path:

```text
Settings
-> Storage allocation & node health
```

It may show:

```text
Local storage capacity (e.g. 15 GiB, minimum)
Storage held for others (up to 2/3 of capacity)
Storage audit health & uptime
Storage lease cost and paid-until date
Estimated storage service value
```

Ordinary users who simply store files and send mail do not need to understand
internal chunk placement or audit mechanics.

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
