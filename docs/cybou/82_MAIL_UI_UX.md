# 82 — CYBOU Mail UI/UX

Status: canonical Beta Mail product UX contract.

This document defines the normal desktop experience for CYBOU Mail. The
interaction benchmark is the familiarity and efficiency of mature webmail
products such as Gmail; CYBOU does not copy Google branding, visual identity,
or provider-centric account semantics. The goal is that a user who already
understands mainstream email can use CYBOU Mail immediately. Messages and
attachments remain end-to-end encrypted, and the product explains finality and
availability without exposing protocol machinery in ordinary workflows.

The normal user operates mail, not a blockchain, key exchange, validator set,
or storage protocol. Technical evidence remains inspectable through Security
Details and Advanced diagnostics.

## 0. Content lifecycle (finality first)

Every outgoing message, with or without attachments, follows one
finality-first lifecycle shared with Files (`83_STORAGE_UI_UX.md`):

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
durability threshold
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
Protected    = the product durability threshold has been reached
Retrievable  = this client has fetched and verified the content
```

`Finalized` does not mean `Sent`. Finality only authorizes storage admission;
the message is `Sent` when the required durability/availability is reached.

The Beta interaction target includes conversation threads, unread/read state,
local labels, reply/reply-all/forward, blocked senders, local search, and
`.cybou` contact autocomplete. Keyboard shortcuts follow after the primary
accessible mouse and keyboard flows are complete.

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
model. The current Mail page is capability-gated; this is a target contract,
not an implemented storage guarantee.

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

Search must not send plaintext query terms to validators or storage providers.
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

Before finality, attachment chunks exist only in local staging. The client
never uploads ciphertext to providers ahead of a finalized RootPublication, and
providers reject chunks without a finalized-publication admission proof.

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

Autosave should not block the UI.

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
