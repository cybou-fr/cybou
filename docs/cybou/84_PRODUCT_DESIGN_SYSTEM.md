# 84 — CYBOU product design system

Status: canonical shared desktop product design and interaction contract.

This document defines the cross-service UX language for Home, Mail, Files,
Wallet, Identity/Security, and Settings. Page-specific behavior is owned by the
page documents, especially `82_MAIL_UI_UX.md` and `83_STORAGE_UI_UX.md`.

The product benchmark is familiarity comparable to mainstream productivity
software such as Gmail and Google Drive while preserving CYBOU's own visual
identity, decentralized architecture, user-owned identity, and hybrid-PQ
security model.

## 1. Design principle

CYBOU should feel simple because the infrastructure is strong, not because the
infrastructure is hidden dishonestly.

```text
complex protocol
-> small number of human states
-> familiar actions
-> technical evidence on demand
```

Primary hierarchy:

```text
1. Person / file / message / amount
2. Action
3. Outcome / progress
4. Security assurance
5. Technical evidence on demand
```

## 2. One product, not mini-apps

The active identity follows the user everywhere:

```text
stan.cybou
  Home
  Mail
  Files
  Wallet
  Identity & Security
```

Do not ask the user to choose keys, accounts, networks, or crypto suites again
when switching services.

## 3. Main application shell

Recommended shell:

```text
+-------------------------------------------------------------------------------+
| CYBOU      contextual/global search                              stan.cybou   |
+-------------+-----------------------------------------------------------------+
| Home        |                                                                 |
| Mail        |                      active page                                |
| Files       |                                                                 |
| Wallet      |                                                                 |
|             |                                                                 |
| Settings    |                                                                 |
+-------------+-----------------------------------------------------------------+
```

Home composition is flexible: keep service entry points readable, Identity
readiness clear and Recent Activity secondary. Vertical cards, a compact row
or a responsive grid are acceptable when task discovery, density and DPI
acceptance pass. Difference from a previous layout is not itself a defect.

## 4. Navigation

Primary navigation is stable and predictable:

```text
Home
Mail
Files
Wallet
Network (when its bounded diagnostics surface is connected)
```

Capability-gated services must not look operational before backend support is
live. Backup remains post-Beta and should not occupy primary Beta navigation
unless implemented.

Identity/security is accessed through the identity chip/menu and Settings.

## 5. Search

Search follows context:

- global search routes to available Mail/Files results with explicit scope;
- Mail search operates over private local Mail indexes;
- Files search operates over private local Files metadata;
- search terms must not be leaked to peers/providers merely for UX parity.

Search fields should be prominent where users expect them, especially Mail and
Files.

## 6. Visual language

Use CYBOU-owned theme tokens rather than hardcoded page colors.

Semantic roles:

```text
surface
surface-raised
text-primary
text-secondary
border-subtle
accent
success
warning
error
info
selection
focus
```

Exact color values are owned by `CybouTheme`/brand assets. Page code should not
invent independent color systems.

## 7. Typography

Typography should optimize long desktop sessions:

- clear page titles;
- compact but readable list rows;
- strong subject/file-name hierarchy;
- secondary metadata visually quieter;
- no tiny security/diagnostic text required for normal operation.

Use a small, consistent token set rather than page-specific point sizes.

## 8. Spacing and density

CYBOU should support productive information density without looking like an
operator console.

Guidelines:

- predictable outer margins;
- consistent 8/12/16/24-style spacing rhythm through theme tokens;
- dense lists for Mail/Files;
- comfortable cards for Home/Security;
- avoid gratuitous giant empty panels;
- avoid nested boxes around every piece of information.

## 9. Buttons and actions

Primary action examples:

```text
Compose
Send
Upload
Create identity
Confirm
```

Secondary action examples:

```text
Cancel
Save to Files
Advanced details
```

Destructive actions require clear wording and appropriate confirmation:

```text
Rotate Identity keys
Delete permanently
Remove local vault
```

Do not style routine operations as destructive merely because they produce a
protocol operation.

## 10. Lists

Mail and Files share list behavior:

- hover and keyboard focus;
- single/multi selection where supported;
- row context menu;
- selection toolbar for bulk actions;
- stable row height;
- ellipsis rather than layout breakage for long names;
- no horizontal page scrolling at supported desktop widths.

## 11. Context menus

Context menus should contain familiar user actions, not protocol actions.

Mail example:

```text
Reply
Forward
Star
Archive
Move to Trash
Security details
```

Files example:

```text
Open / Download
Rename
Move
Star
Save/Copy action if relevant
Move to Trash
Details
```

## 12. Status vocabulary

Shared normal vocabulary:

```text
Online
Syncing
Offline
Preparing
Uploading
Downloading
Securing
Sending
Waiting for confirmation
Checking status
Protected
Confirmed
Repairing protection
Needs attention
```

Avoid normal-surface terms such as:

```text
mempool
broadcast
quorum
prevote
precommit
nonce
replica placement
proof challenge
KEM encapsulation
```

## 13. Progress

Use determinate progress when meaningful bytes/items are known:

```text
Uploading 34%
Downloading 58%
```

Use phase/status progress where the exact duration is unknown:

```text
Securing attachment...
Waiting for network confirmation...
Repairing protection...
```

Never fake a percentage for consensus/finality or cryptographic work if no
meaningful progress metric exists.

## 14. Success and confirmation

Final words such as:

```text
Sent
Confirmed
Protected
Name ready
Identity keys rotated
```

are reserved for states backed by the core's verified/finalized truth.

Before then use pending vocabulary.

Local words such as Saved, Archived and Moved to Trash require acknowledgement
of the corresponding local durable commit. A queued request is not success.
Archive is local mailbox organization, not a PoA operation. Network Files
mutations retain their separate confirmation/protection lifecycle.

Short background actions use an inline pending indicator and a non-blocking
task panel. After roughly one second show that work continues; this is a UX
feedback target, not a promised completion time. Offer details and actionable
errors. Avoid a mandatory modal for every archive; use a modal only when the
user must decide or acknowledge a destructive action. Do not invent an ETA
or percentage for an unmeasured phase.

## 15. Delivery uncertainty

`delivery_uncertain` is a first-class product state.

Required UX:

```text
Checking delivery status...
```

Not:

```text
Rejected
Failed to send
```

until the core can prove rejection/non-acceptance under the operation
coordination policy.

This rule applies to Wallet, Name, Mail, and future Identity operations.

## 16. Security indicators

Normal summary:

```text
Identity signatures: post-quantum protected (only when verified for the active identity)
End-to-end encrypted
Verified identity
Network-confirmed
```

Do not flood every row with shields and crypto labels. Use security indicators
where they help a user make a trust decision.

Advanced Security Details may name exact algorithms and evidence.

## 17. Errors

Every user-visible error maps to:

```text
Needs user action
Temporary network problem
Still processing / waiting
Conflict with verified state
Local security/data problem
```

An error message answers:

```text
What happened?
Did my action happen?
What should I do next?
```

Raw core error text belongs in diagnostics/logs.

## 18. Offline UX

CYBOU is expected to encounter intermittent connectivity.

The app shell remains usable offline. Local data remains browsable according to
its security state.

Mutating actions either:

- fail before submission with a clear offline message; or
- enter a durable queued state only if a real persistent queue exists.

Do not invent queued success in the GUI.

## 19. Responsive desktop targets

Mandatory validation widths:

```text
1040 x 720
1280 x 860
1600 x 900
1920 x 1080
```

Also validate common Windows DPI scales such as 125% and 150%.

Rules:

- no whole-page horizontal scrollbar;
- important actions remain visible;
- sidebars collapse before content becomes unusable;
- Mail can transition 3-pane -> 2-pane -> 1-pane;
- Files can hide low-priority columns;
- dialogs fit the minimum supported viewport.

## 20. Accessibility

Required direction:

- full keyboard traversal;
- visible focus indication;
- meaningful accessible names for icons/actions;
- no status conveyed only by color;
- sufficient contrast;
- scalable text/DPI layout;
- predictable tab order;
- destructive confirmations readable without relying on icons.

## 21. Async UI contract

The Qt GUI thread must never perform:

```text
network requests
block/history scans
Storage upload/download
large file hashing/chunking
PQ KEM/signature generation if it can stall interaction
long DB migrations
recursive folder enumeration and large filesystem scans
```

Pages issue requests through controllers/services and consume model snapshots,
progress, and completion states.

A user click gets immediate local acknowledgement even if the real operation
continues asynchronously.

Keep widgets and item identity stable. Coalesce frequent signals, update only
changed rows/fields and preserve expanded details, selection, focus, scroll and
compose text. Reducing polling alone does not fix destructive widget rebuilds.
Activity and diagnostics show Last updated and a manual Refresh action;
unchanged snapshots cause no visible redraw. Important failures/completion
remain event-driven. See `DESKTOP_UX_DELIVERY_PLAN.md` for implementation order.

## 22. Home

Home is a calm dashboard, not a node console.

Recommended structure:

```text
Identity / readiness

Mail card
Files card
Identity & Security card

Recent Activity                  right/secondary column
```

Possible content (security claims must be capability-specific and verified):

```text
stan.cybou
Online
Identity signing: post-quantum protected
Mail confidentiality: unavailable until verified hybrid recipient keys are published

Mail       12 unread
Files      12.4 GB used / Protected
Recovery words  Confirmation completed on this installation

Recent activity
Alice sent "Project files"
report.pdf protected
100 CYBOU sent to bob.cybou
```

Phrase confirmation is not proof that an external backup remains available.
Normal Home uses concise summaries; Network provides scoped network statistics
and an optional schematic France map. Raw evidence remains in Advanced.

## 23. Settings structure

Suggested normal settings:

```text
General
Notifications
Privacy
Identity & Security
Storage
Advanced
```

Advanced contains:

```text
Network diagnostics
Cryptography
Storage diagnostics
Logs
Protocol and implementation details
```

## 24. Technical transparency

Progressive disclosure is mandatory, not secrecy.

A technical user must be able to inspect:

```text
AccountID
NetworkID
OperationID
finalized height
PoA finality status
peer connection health
crypto suites
application publication status
logs
```

without those values becoming required knowledge for ordinary workflows.

## 25. Product benchmark

The desired experience can be summarized as:

```text
Gmail-level familiarity for Mail
+
Google Drive-level familiarity for Files
+
CYBOU-owned identity
+
hybrid-PQ E2E protection
+
distributed storage
+
independent verification/finality
```

Familiar workflows preserve user-held Identity and encrypted content with
independent local verification. Canonical finality remains centralized under
the genesis-authorized PoA key; the UI must describe this trust boundary honestly.

Network, operator tools and the own-content console follow
[`NETWORK_AND_ADVANCED_UX.md`](NETWORK_AND_ADVANCED_UX.md). Security and deletion
labels follow scoped evidence, never an unconditional RGPD/certification badge.
