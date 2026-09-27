# 82 — CYBOU Mail UI/UX

Status: canonical Beta Mail product UX contract.

This document defines the normal desktop experience for CYBOU Mail. The
interaction benchmark is the familiarity and efficiency of mature webmail
products such as Gmail; CYBOU does not copy Google branding, visual identity,
or provider-centric account semantics. The goal is that a user who already
understands mainstream email can use CYBOU Mail immediately while the protocol
remains hybrid-PQ, E2E encrypted, consensus-registered, and decentralized.

The normal user operates mail, not a blockchain, key exchange, validator set,
or storage protocol. Technical evidence remains inspectable through Security
Details and Advanced diagnostics.

See also:

- `81_BETA_PRODUCT_SCOPE.md` — Beta product boundary;
- `49_EMAIL_E2EE_HPKE_PQ.md` — Mail confidentiality protocol;
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
-> wait until the attachment is protected
-> press Send
-> close CYBOU

Alice may be offline.

Alice later opens CYBOU
-> sees the finalized message
-> opens it
-> downloads/decrypts the attachment
-> replies
```

No step requires the user to understand:

```text
AccountID
OperationID
BFT quorum
block height
nonce
ML-KEM
X25519
storage shard IDs
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
```

Optional labels/categories may be added only when the corresponding local data
model exists. Do not add decorative navigation that does not function.

Unread count appears only where meaningful, primarily Inbox.

## 5. Search

The top search field is a normal product feature, not a protocol query editor.
Beta search should support local indexed search over data that the client can
legitimately decrypt and index, including:

```text
sender / recipient .cybou name
subject
body text
attachment filename
local labels/folders if implemented
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

Do not show OperationID, block height, MailTx size, discovery tag, or fee in the
normal list.

Useful states:

```text
Unread
Read
Starred
Draft
Sending
Waiting for confirmation
Sent
Needs attention
```

A pending outgoing message should remain visually distinct from a finalized
Sent message.

## 7. Reader

The reader shows:

```text
sender identity
recipient identity
subject
message content
attachments
reply / forward actions
compact security status
```

Example security line:

```text
Protected end to end  •  Post-quantum protected  •  Network-confirmed
```

This line is secondary. It must not compete with the message itself.

Clicking Security Details may reveal:

```text
resolved AccountID
sender key/evidence status
hybrid signature status
Mail confidentiality suite
finalized height
OperationID
historical authorization evidence
```

## 8. Compose

Compose should feel familiar:

```text
New message

To:      alice.cybou
Subject: Project files

Hello Alice,
...

report.pdf      4.2 MB      Protected
photo.jpg       8.1 MB      Securing 2/3

                              [Send]
```

Required fields:

```text
To
Subject
Body
Attachments
```

The initial Beta protocol remains one-recipient. The UI must not present CC,
BCC, or multiple recipients until the protocol supports them.

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

Attachments are normal Mail UI objects backed by CYBOU Object Storage.

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
-> encrypt locally
-> chunk
-> upload opaque ciphertext
-> reach minimum Storage durability
-> build encrypted private attachment manifest
-> enable Mail submission
```

Normal UI should compress this into understandable phases:

```text
Preparing...
Uploading 34%
Securing 2/3
Protected
```

The exact replica/audit information belongs in details.

### Send gate

A message containing attachments must not enter the normal Send submission path
until every required attachment has reached the protocol-defined minimum
Storage durability state.

Bad:

```text
Mail finalized
attachment upload failed afterwards
```

Required:

```text
attachment protected
-> Mail can be submitted
-> finality
```

If the user closes Compose during upload, the client must either preserve the
draft and resumable attachment state or clearly cancel it. Never leave an
orphaned UI state that says Sent.

## 10. Mail/Files integration

The reader offers:

```text
Download
Save to Files
```

`Save to Files` should reuse the existing protected Storage object/reference
when ownership, retention, and privacy rules permit. It should not download and
re-upload identical ciphertext merely to move an attachment into the user's
Files view.

The implementation may create or update the user's private encrypted Files
manifest while keeping provider-visible metadata opaque.

## 11. Sending state machine

Normal product states:

```text
Draft
Preparing
Securing attachments
Sending
Waiting for confirmation
Sent
Needs attention
```

Core/network distinctions must be mapped honestly:

```text
operation accepted/pending
    -> Waiting for confirmation

delivery uncertain
    -> Checking delivery status...
       Do not call this rejected.

explicit protocol rejection
    -> Needs attention + actionable reason

finalized
    -> Sent
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

Do not expose storage-node addresses or shard IDs in the normal message view.

## 14. Drafts

Draft text and attachment metadata are local/private user data. If future
cross-device draft synchronization is introduced, it must be explicitly
encrypted and specified; do not accidentally place plaintext drafts in
consensus or provider-visible storage.

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
finality evidence
```

The UI must fail closed if the required hybrid profile is unavailable.

## 18. Error model

Map technical failures into actionable product categories:

```text
Recipient unavailable for protected Mail
Attachment could not be secured
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
[ ] understand attachment preparation/protection progress
[ ] understand that Send is waiting when durability is not ready
[ ] distinguish Sending / Checking / Sent / Needs attention
[ ] go offline and later recover verified Inbox state
[ ] download/decrypt an attachment after being offline during send
[ ] save a received attachment to Files
[ ] search local mail by sender/subject/body/attachment filename
[ ] discover Security Details without being forced to understand them
```

The same flow must remain usable at 1040, 1280, 1600, and 1920 pixel desktop
widths and at common Windows DPI scaling values.
