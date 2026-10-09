# 82 — CYBOU Mail UI/UX

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Recorded status: canonical Beta Mail product UX contract.

The desktop shell exposes one contextual header search. Typing and clearing use
the existing current-mailbox filter; choosing a suggestion opens its semantic
item. Mail retains its query when visiting Files and restores it on return.
Ctrl+K and the Mail `/` shortcut focus the visible header field. Identity lock
clears the query and private suggestions. Standalone page fixtures retain their
local search control, hidden when embedded in the shell.

This document defines the normal desktop experience for CYBOU Mail. The
interaction benchmark is the familiarity and efficiency of mature webmail
products such as Gmail; CYBOU does not copy Google branding, visual identity,
or provider-centric account semantics. The goal is that a user who already
understands mainstream email can use CYBOU Mail immediately. Messages and
attachments remain end-to-end encrypted, and the product explains finality and
availability without exposing protocol machinery in ordinary workflows.

The normal user operates mail, not a blockchain, key exchange, finality mechanism,
or storage protocol. Technical evidence remains inspectable through Security
Details and Advanced diagnostics.

The message list uses one viewport delegate over stable ID-keyed Qt items.
Rows do not create per-message QWidget trees; only visible rows are painted.
Sender/recipient direction, avatar, unread marker, star, attachments, date,
protection/confirmation state and the support-rate warning remain semantic data.
Subject and preview are drawn as plain text. Escaped row tooltips preserve
protection and rate explanations; accessible text/description expose these
facts without relying on icons or color. Selection, current item, scroll anchor,
search, drag IDs and draft-to-finalized ID handoff remain the existing paths.
Lock removes the items and private rendering cache.

Large-mailbox profiling records construction, first completed Qt viewport
render, ten synchronous scroll renders and ten one-message update/render samples
for 2,000 and 10,000 synthetic messages. Timings are environment observations,
not product SLAs, monitor presentation latency or live sync/storage throughput.
Compare like-for-like native platform, theme initialization, DPR and geometry;
see the dated implementation evidence for the current before/after results.

## 0. Content lifecycle (finality first)

Every outgoing message, with or without attachments, follows one
shared lifecycle with Files (`83_STORAGE_UI_UX.md`):

```text
local draft
   ↓
local encrypt/chunk
   ↓
RootPublication submission
   ↓
PoA finality
   ↓
finalized-authorized chunks → storage providers
   ↓
durability threshold (2 remote replicas)
   ↓
Sent / Protected
```

Message text, recipients, attachment references, filenames, and the Mail schema
are encrypted payload data inside one RootPublication. There is no Mail-specific
consensus operation. No content is stored remotely before finality: until the
RootPublication is finalized, all ciphertext chunks remain local staging only.

Status terms are distinct and must never share one indicator:

```text
Finalized    = the RootPublication is part of canonical PoA history
Authorized   = its ChunkIDs are admitted for storage by that finalized publication
Available    = the chunks can actually be retrieved from the network
Protected    = the product durability threshold has been reached (2 remote replicas)
Retrievable  = this client has fetched and verified the content
```

`Finalized` does not mean `Sent`. Finality authorizes storage admission;
the message is `Sent` when the required remote durability/availability is reached.

The Beta interaction target includes conversation threads, unread/read state,
local labels, reply/reply-all/forward, blocked senders, local search, and
`.cybou` contact autocomplete. Keyboard shortcuts follow after the primary
accessible mouse and keyboard flows are complete.

Compose accepts protected Files drags by internal item ID, including drops over
the message editor. It adds one reusable reference per file without a local
source path; duplicate IDs do not add duplicate attachments. Internal IDs take
precedence over downloaded URLs, so refused references never silently become
new local uploads. All items in a dropped batch must be eligible: pending,
missing, folders and content in Trash are refused with an explanation. Drop
revalidates the source and active Mail session; handoff/close blocks attachment
changes. Local file drops remain available. Dropped batches respect the current
32-attachment Mail bound.

An acknowledged draft persists the reference ID, not a new retention guarantee
for its Files source. Sending must still resolve that source at preparation.
If it was removed before durable outgoing handoff, preparation fails and keeps
the draft; it does not substitute a downloaded copy. Finalization and remote
durability retain their existing separate delivery states.

See also:

- `81_BETA_PRODUCT_SCOPE.md` — Beta product boundary;
- `ROOT_PUBLICATION.md` and `ENCRYPTED_CHUNK_TREE.md` — private content substrate;
- `83_STORAGE_UI_UX.md` — Files/Storage experience;
- `84_PRODUCT_DESIGN_SYSTEM.md` — shared visual and interaction rules;
- `73_CORE_DESKTOP_CONTRACT.md` — core/UI truth boundary.

## 1. Product objective

A Beta user should be able to:

```text
open CYBOU
-> see Inbox
-> compose to alice.cybou
-> write text
-> drag a PDF/photo into the message
-> press Send
-> see Preparing -> Waiting for confirmation -> Securing -> Sent
-> close CYBOU after Sent

Alice may be offline.

Alice later opens CYBOU
-> sees the finalized message
-> opens it
-> downloads/decrypts the attachment
-> replies
```

No step requires the user to understand:

```text
account identifiers
publication identifiers
finality certificates
block numbers
protocol nonces
KEM internals
storage chunk IDs
replica placement
provider endpoints
```

## 2. Interaction reference

Use Gmail as an ergonomic reference for concepts users already know:

- Compose is prominent and always easy to reach;
- folders/mailboxes are stable in the left rail;
- search is first-class;
- message lists are dense but readable;
- reader and compose actions are predictable;
- attachments look like ordinary files;
- keyboard and mouse workflows both work;
- technical infrastructure is not the primary surface.

Do not reproduce Gmail pixel-for-pixel. CYBOU uses its own typography, spacing,
icons, colors, security language, and identity model.

## 3. Desktop information architecture

Wide desktop target:

```text
+--------------------------------------------------------------------------------+
| CYBOU     Search mail...                                      stan.cybou       |
+--------------+-------------------------------+---------------------------------+
| + Compose    | Inbox                         | Message                         |
|              |                               |                                 |
| Inbox     12 | Alice    Project       10:42 | Alice                           |
| Starred      | Bob      Photos         9:15 | Project documents               |
| Sent         | Company  Invoice       Tue   |                                 |
| Drafts       |                               | Hi Stan...                      |
| Archive      |                               |                                 |
| Trash        |                               | report.pdf  4.2 MB              |
|              |                               | [Download] [Save to Files]      |
|              |                               |                                 |
|              |                               | Reply  Forward                  |
+--------------+-------------------------------+---------------------------------+
```

Responsive behavior:

```text
>= 1400 px       folders + message list + reader
1040–1399 px     folders + message list; reader replaces list when opened
< 1040 px        one primary pane at a time with explicit Back navigation
```

Do not solve narrow layouts by adding horizontal scrolling to the full page.

## 4. Mail navigation

Required Beta navigation:

```text
Compose
Inbox
Starred
Sent
Drafts
Archive
Trash
Blocked senders
Labels
```

Labels, blocked senders, read/unread, stars, archive, and trash are local
mailbox state, never consensus state. A future Spam classifier must be local
or have a separately reviewed service/privacy contract.

Unread count appears only where meaningful, primarily Inbox.

## 5. Search

Inbox, Sent, Drafts, read state, and search indexes are local client data. Any
persisted mailbox content or index containing message plaintext or identifying
metadata MUST be encrypted at rest under the local identity/vault security
model. The live adapter uses the encrypted per-Identity Application DB; search
and local mailbox state remain capability-gated. This does not mean that drafts
or local organization are reconstructible from public finalized history.

The top search field is a normal product feature, not a protocol query editor.
Beta search should support local indexed search over data that the client can
legitimately decrypt and index, including:

```text
sender / recipient .cybou name
subject
body text
attachment filename
local labels
```

Search must not send plaintext query terms to peers or storage providers.
If the implementation is local-only, the UI should simply behave like normal
search; the privacy architecture belongs in Security Details, not in the
placeholder text.

## 6. Message list

Each row should prioritize:

```text
sender / recipient
subject
short preview
attachment indicator
unread/star state
time/date
```

Do not show protocol identifiers, block numbers, encrypted payload size,
discovery metadata, or protocol fees in the normal list.

Useful states:

```text
Unread
Read
Starred
Draft
Preparing
Waiting for confirmation
Securing
Sent
Needs attention
```

A pending outgoing message (Preparing, Waiting for confirmation, Securing)
must remain visually distinct from a Sent message. A finalized message that is
still Securing is not Sent.

## 7. Reader

The reader shows:

```text
sender identity
recipient identity
subject
message content
attachments
reply / reply-all / forward actions
compact security status
```

Example security line:

```text
Protected end to end  •  Post-quantum protected  •  Network-confirmed
```

This line is secondary. It must not compete with the message itself.

Clicking Security Details shows product-level status:

```text
Identity authorization       Valid
Network confirmation         Finalized
Content authorization        Valid
Content availability         Protected
```

Advanced details (not the ordinary UI) may additionally reveal:

```text
resolved AccountID
OperationID
finalized height
sender authorization evidence (historical Identity key_epoch)
PoA finality evidence
RootChunkID
Mail confidentiality suite
```

## 8. Compose

Compose should feel familiar:

```text
New message

To:      alice.cybou
Subject: Project files

Hello Alice,
...

report.pdf      4.2 MB      Encrypted locally
photo.jpg       8.1 MB      Preparing 60%

                              [Send]
```

Required fields:

```text
To
Subject
Body
Attachments
```

DEV/Alpha remains one-recipient. Beta targets Gmail-style To/Cc/Bcc and
Reply All with one current Identity KEM capsule per recipient AccountID/key
epoch. The Beta recipient limit and discovery/privacy encoding remain protocol
gates; do not show group-send controls until those gates are implemented.

### Recipient resolution

The primary input is `.cybou` identity:

```text
To: ali

Alice
alice.cybou
Verified identity
```

A resolved recipient may show a compact verified indicator. Raw AccountID is
available only through details.

If the recipient exists but lacks an acceptable Mail encryption key package:

```text
Alice cannot receive protected CYBOU Mail yet.
```

Do not silently downgrade to classical-only encryption.

## 9. Attachment flow

Attachments are ordinary Mail UI files backed by the shared encrypted chunk
tree.

User actions:

```text
Attach file
Drag & drop file(s)
Remove attachment before send
Open local source before send
```

Internally:

```text
read local file
-> encrypt/chunk locally (local staging only)
-> include the private attachment reference in the encrypted Mail root
-> build one RootPublication for the message
-> obtain PoA finality
-> upload the finalized-authorized chunks to storage providers
-> reach the durability threshold
```

Attachment chunks exist only in local staging before finality; the client
uploads ciphertext only after finalized RootPublication. Providers reject
unauthorized chunks.

In Compose, attachment chips show only local preparation (`Preparing`,
`Encrypted locally`). After Send, the whole message, including attachments,
moves through one lifecycle (see §11). Replica/audit information belongs in
details.

### Send semantics

Send is available once local preparation succeeds. The message and its
attachments are one publication, so they are finalized together and cannot
diverge:

```text
Preparing
-> Waiting for confirmation   (RootPublication submitted, awaiting PoA)
-> Securing                   (finalized; authorized chunks spreading to storage)
-> Sent                       (required durability reached)
```

Bad:

```text
Mail shown as Sent while attachment chunks are only finalized, not durable
```

The sending client must retain the local ciphertext until durability is
reached. If CYBOU is closed while Waiting for confirmation or Securing, the
client resumes on restart from persisted local state; it never shows Sent
before durability and never leaves an orphaned state that claims Sent.
Closing Compose before Send preserves the draft and locally staged attachments
or clearly discards them.

## 10. Mail/Files integration

The reader offers:

```text
Download
Save to Files
```

Target semantics:

```text
Mail attachment
    ↓
Save to Files
    ↓
new private Files catalog reference
    ↓
existing encrypted content may be reused
```

`Save to Files` adds an independent entry to the recipient's encrypted Files
catalog that references the existing protected content. It does not download and
re-upload ciphertext. Reuse is allowed only when authorization and retention
semantics permit: the recipient holds the content key from the Mail payload, and
the Files retention reference is independent of the Mail message, so deleting
or expiring the Mail message does not make the saved Files item unavailable.
Where reuse is not permitted, the client prepares new protected content through
the normal Files lifecycle.

In the other direction, `Send by CYBOU Mail` from Files places a private
reference to existing protected content inside the encrypted Mail root; no
second upload of identical ciphertext is required.

Content references, filenames, MIME types, and keys stay inside encrypted
payloads and are never visible to storage providers.

## 11. Sending state machine

Normal product states:

```text
Draft
Preparing
Waiting for confirmation
Securing
Sent
Needs attention
```

Meaning:

```text
Preparing                = local encryption/chunking; nothing published
Waiting for confirmation = RootPublication submitted, awaiting PoA finality
Securing                 = publication finalized; authorized chunks spreading to storage
Sent                     = required durability/availability threshold reached
```

Core/network distinctions must be mapped honestly:

```text
operation submitted/pending
    -> Waiting for confirmation

delivery uncertain
    -> Checking delivery status...
       Do not call this rejected.

explicit protocol rejection
    -> Needs attention + actionable reason

finalized
    -> Securing (not Sent)

durability threshold reached
    -> Sent

durability not reached after retries
    -> Needs attention (content is finalized; client keeps retrying from local ciphertext)
```

A retry after `delivery uncertain` must reconcile the original OperationID and
nonce reservation before constructing a replacement operation.

## 12. Receive/offline state

Recipient offline presence is normal.

When a user returns after being offline:

```text
Syncing securely...
Mail will appear as verified history is restored.
```

Do not show partially verified ciphertext as a normal Inbox message.

A message enters the normal Inbox only after the local client has enough
verified evidence to treat it as finalized and attributable under the active
Mail policy.

## 13. Attachment receive state

A finalized message may be visible before its attachment is locally downloaded.
The attachment chip can show:

```text
report.pdf   4.2 MB
Available securely
[Download]
```

During retrieval:

```text
Downloading 58%
Verifying
Decrypting
Ready
```

If some providers are unavailable but repair/retry is in progress:

```text
Temporarily unavailable — retrying
```

Do not expose storage-node addresses or chunk IDs in the normal message view.

## 14. Drafts

Draft text and attachment metadata are local/private user data. Do not place
plaintext drafts in consensus or provider-visible storage.

Autosave must be debounced and run off the UI thread, with Saving / Saved /
Save failed states backed by durable acknowledgement. The current composer
requires this delivery work; optimistic insertion is not successful persistence.
Closing must retain content until save succeeds or the user explicitly discards
it. A send failure during recipient resolution or publication preparation keeps
the draft recoverable. Remove the draft only after a durable outgoing job owns
the payload; uncertain delivery reconciles the exact outgoing operation.
Drafts are device-local and are not reconstructed on a clean machine from
finalized history. Lock/shutdown drains must preserve acknowledged commands
without freezing interaction or leaking private content.

## 15. Delete, archive, and trash

Mail UI semantics should match user expectations while respecting immutable
consensus history:

- Archive removes the message from the normal Inbox view locally;
- Trash marks the local mailbox item for user-visible deletion semantics;
- deleting a local view does not pretend to erase already-finalized historical
  protocol evidence from other nodes;
- deleting an attachment from Files follows Storage retention/lease rules and
  must not break another retained reference unexpectedly.

The UI should describe user-visible behavior, not make false global deletion
claims.

Archive/trash requests immediately show a pending row/task state. Show Archived
or Moved to Trash only after the local encrypted DB commit; no PoA progress is
needed for this command. Correlate failures with affected items and retain the
prior view on failure. Batch operations show completed/failed counts and retry
only failed items. Undo must serialize behind an unfinished move and remain
correct through refresh, lock and error reconciliation.

Context menu, keyboard, toolbar and drag/drop use the same move command path.
Mail ID drag/drop already exists in Qt; acceptance requires actual mouse drag
from populated row widgets to Archive/Trash, visible drop feedback and correct
selection at supported DPI. Invalid drops give feedback without changing state.
Use the common task panel for slow operations rather than a blocking modal for
every archive. Technical logs are optional, redacted Advanced details.

The header Activity panel is shared by Mail and Files. Identity-local commands
use correlated Queued/Running/Committed/Failed states and a Mail/Files scope.
Completed local saves leave this in-flight view; outgoing publication and remote
protection progress remain separate semantic rows. Command failures remain
visible with their affected item; reopening it offers the relevant workflow,
without automatic replay of a mutation whose intent may have changed. Retry is
provided only for the existing publication retry commands. Terminal local task
retention is bounded to 32 entries; active commands are never evicted for this
limit. The task journal is volatile and lock clears it; durable draft and
publication ownership remain the core application's responsibility.

The panel scrolls, retains stable rows and button focus across status updates,
and preserves its scroll position. It displays at most 100 tasks with an explicit
truncation notice; additional item progress remains in Mail and Files. Task text
is plain text, with bounded status/tooltip output. The popup clears cached private
labels on lock even while hidden. Console jobs uses the same local task journal
and identifies Mail/Files scope without changing command semantics.

## 16. Notifications

Desktop notifications may show:

```text
New mail from alice.cybou
Project files
```

only if the user permits message previews. A privacy setting must allow:

```text
New CYBOU Mail
```

without sender/subject disclosure on the OS lock screen.

## 17. Security presentation

Default security vocabulary:

```text
Protected end to end
Post-quantum protected
Verified identity
Network-confirmed
```

Do not make normal users choose algorithms.

Advanced details may name:

```text
Ed25519 + ML-DSA
X25519 + ML-KEM
AEAD profile
PoA finality evidence
```

The UI must fail closed if the required hybrid profile is unavailable.

## 18. Error model

Map technical failures into actionable product categories:

```text
Recipient unavailable for protected Mail
Attachment could not be prepared
Message could not be secured — retrying
CYBOU is offline
Delivery status is uncertain — checking
Waiting for network confirmation
Local encrypted mailbox needs attention
```

Avoid raw protocol error strings in the normal surface.

## 19. Keyboard and productivity

Where practical, support familiar desktop mail shortcuts without conflicting
with OS conventions:

```text
Ctrl+N / C          new message (platform-appropriate mapping)
Ctrl+Enter          send, with optional confirmation preference
R                   reply when message view has focus
F                   forward when message view has focus
/ or Ctrl+K         focus search if consistent with global app shortcuts
Delete              move selected message to Trash
```

All shortcuts must have menu/action equivalents and accessible labels.

Keyboard context-menu requests use the current message row, scroll it into view
and anchor the menu there. Pointer coordinates do not select another message.
An existing multiple selection containing that row is preserved; otherwise the
current row becomes the selection. Menu actions keep their existing command
and acknowledgment paths. The CYBOU Files attachment picker has an explicit
accessible name identifying its purpose.

Compose Send and Reader Security details retain explicit visible keyboard-focus
borders even with their local button styles. The border space is reserved in
both states so focus does not change their geometry. Component acceptance walks
Tab and Shift+Tab through Compose fields, dynamic attachment removal and visible
enabled buttons, and Reader actions, in FR/EN and light/dark themes. This is
programmatic Qt evidence; physical keyboard and screen-reader use remain separate.

## 20. Performance contract

The Qt event loop must never perform network, block scan, Storage upload,
cryptographic KEM, or long disk work synchronously.

Immediate actions should acknowledge visually without waiting for the network.
Long work reports progress through immutable/model-driven state.

Scrolling, selecting a message, opening a menu, and typing in Compose must stay
responsive while sync, encryption, upload, or finality work is running.

## 21. Beta Mail UX acceptance

A Beta candidate passes when a new user can, without technical guidance:

```text
[ ] find Inbox and Compose immediately
[ ] resolve another user by name.cybou
[ ] write and send a text message
[ ] drag a PDF/photo into Compose
[ ] understand attachment preparation progress
[ ] understand that a finalized message is still Securing until durability
[ ] distinguish Preparing / Waiting for confirmation / Securing / Sent / Needs attention
[ ] go offline and later recover verified Inbox state
[ ] download/decrypt an attachment after being offline during send
[ ] save a received attachment to Files
[ ] search local mail by sender/subject/body/attachment filename
[ ] discover Security Details without being forced to understand them
```

The same flow must remain usable at 1040, 1280, 1600, and 1920 pixel desktop
widths and at common Windows DPI scaling values.

## Stable mailbox presentation

Draft discard and permanent mailbox deletion follow the same correlated local
command acknowledgement as moves. A queued/running delete keeps the row and
editor content until the encrypted Application DB commits it. Failed deletion
retains content and exposes retry; partial batches count only committed items.
Discard queues after any in-flight autosave and ignores its obsolete editor
reply. Theme/language rebuild follows the pending delete by draft/task identity.
Lock or session replacement invalidates late UI acknowledgements. These are
local mailbox actions, not proof of network erasure or PoA finality. A durable
pending send cannot be cancelled merely by removing its projected row.

Reconcile visible messages by semantic ID. Retain unaffected row widgets,
selection, keyboard current item and scroll anchor; update only changed rows.
Folder targets and count labels remain stable during refresh/drag. Temporary
outgoing ID replacement transfers interaction context before removing the old
projection. Filtering/removal drops invisible selections; lock clears private
rows, search and interaction state. Outgoing mail shows its recipient even in
Archive/Trash. A row refresh does not commit a move or imply Sent/protection.

Reader refresh retains body text selection, scroll and unchanged attachment
controls when unrelated messages/status change. Switching messages resets reader
scroll/selection. Bodies and subjects render as plain text. Lock or removal
clears private reader labels/attachments and closes scoped Security Details;
unrelated updates keep that dialog open. Details remain an opening-time
inspection rather than a claim of fresh network evidence.
