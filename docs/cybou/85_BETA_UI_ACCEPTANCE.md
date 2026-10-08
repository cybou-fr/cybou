# 85 — CYBOU Beta UI/UX acceptance

Status: Beta product acceptance checklist.

This document converts the product contracts in docs 81–84 into end-to-end
acceptance scenarios. It does not replace protocol/security tests; a Beta build
must satisfy both.

Presentation evidence from 2026-10-08 is recorded in
`26_IMPLEMENTATION_STATUS.md`: 14 native Windows fixture profiles cover FR/EN,
light/dark, three window sizes and Qt scaling at 125%, with 79 passing Qt tests.
These do not mark the live scenarios below passed. Physical mixed-monitor DPI,
screen-reader use, clean-machine flow and independent remote failure domains
still require their own acceptance evidence.

## 1. Clean-machine onboarding

```text
[ ] install/start CYBOU without developer environment variables
[ ] choose Create identity or Restore identity
[ ] create path explains password vs recovery phrase correctly
[ ] restore path works on a clean machine
[ ] register a .cybou name without showing commit/work/reveal mechanics
[ ] Home reaches Online/Syncing/Offline states honestly
[ ] no raw AccountID/NetworkID/nonce/quorum knowledge is required
```

## 2. Mail baseline

```text
[ ] Inbox/Compose are immediately discoverable
[ ] recipient entered as alice.cybou
[ ] unknown recipient produces a human error
[ ] missing acceptable hybrid encryption key fails closed
[ ] text Mail can be composed, encrypted, stored and delivered through Object Storage
[ ] delivery_uncertain appears as Checking delivery status, not Rejected
[ ] recipient can be offline during send and receive after later sync
[ ] persisted mailbox plaintext/metadata indexes are encrypted at rest
[ ] Search finds locally indexed sender/subject/body data without remote plaintext query
```

## 3. Mail attachment

```text
[ ] drag/drop PDF or photo into Compose
[ ] UI shows Preparing / Uploading / Securing / Protected
[ ] Send can begin after local preparation; Sent is never shown before finality and required remote durability
[ ] attachment bytes are encrypted before Object Storage upload and never enter consensus content
[ ] recipient offline during send can later retrieve/decrypt attachment
[ ] corrupted ciphertext/integrity failure never opens plaintext
[ ] Save to Files produces an independent-retention Files item without unnecessary re-upload when reusable
[ ] deleting/expiring the Mail message leaves the saved Files item available
```

## 4. Files

```text
[ ] My files opens without Storage protocol terminology
[ ] create folder
[ ] upload file and folder
[ ] progress remains responsive during large transfer
[ ] Protected appears only after required durability
[ ] restart preserves/reloads file view correctly
[ ] download verifies then decrypts locally
[ ] rename / move / star / trash / restore work
[ ] normal UI exposes logical storage usage, not replication overhead
[ ] advanced details expose durability/audit/object information on demand
```

## 5. Wallet

```text
[ ] recipient entered primarily as name.cybou
[ ] amount and deterministic fee are clear before confirmation
[ ] Send never blocks the Qt event loop
[ ] pending/uncertain/finalized states remain distinct
[ ] finalized ledger survives restart
[ ] System Balance language remains consistent with docs 52 and 72
[ ] concurrent Name and Wallet requests serialize through the shared coordinator
[ ] no second account operation is issued while Identity has an unresolved operation
[ ] uncertain operations survive restart and reconcile/retry with the same bytes and OperationID
```

## 6. Network interruption

```text
[ ] disconnect network while Inbox is open: app remains usable
[ ] reconnect: sync resumes automatically
[ ] disconnect during file upload: state is honest and recoverable
[ ] disconnect after operation bytes were sent but before acknowledgment: status becomes uncertain/checking
[ ] a bad peer does not turn remote failure into fake local-state corruption
```

## 7. Responsive/DPI

Validate at least:

```text
1040x720
1280x860
1600x900
1920x1080
Windows 125% DPI
Windows 150% DPI
```

```text
[ ] no whole-page horizontal scrollbar
[ ] Home keeps service cards readable and Recent Activity secondary
[ ] Mail transitions correctly between 3/2/1-pane modes
[ ] Files hides low-priority columns before layout breaks
[ ] compose/dialogs stay on-screen
[ ] no clipped folder/list rows
```

## 8. Accessibility/productivity

```text
[ ] keyboard can reach all primary actions
[ ] visible focus
[ ] accessible names for icon-only controls
[ ] error/success not communicated by color alone
[ ] text remains readable at supported DPI
[ ] common Mail/File shortcuts do not conflict with global application shortcuts
```

## 9. Security disclosure

Normal flow shows concise, capability-specific assurances only after the
corresponding capability and key package are verified:

```text
Identity signatures: post-quantum protected
Mail confidentiality: hybrid post-quantum encrypted (only for messages actually using the approved profile)
End-to-end encrypted
Verified identity
Network-confirmed
Protected
```

Advanced details expose exact algorithms/evidence.

```text
[ ] user is not forced to select crypto suite
[ ] downgrade is never silently offered
[ ] provider-visible Storage details exclude plaintext filename/MIME/path/key
[ ] technical evidence remains inspectable through Advanced
```

## 10. Account values

```text
[ ] a Submitted payment does not modify spendable Balance
[ ] Balance and System Balance display from finalized AccountState
[ ] no AUTH, Validation, Age, Activity or System contribution fields appear
```

## 11. Beta wow-flow

A candidate is not product-ready until this can be demonstrated live between
two clean desktop installations:

```text
Stan creates/restores stan.cybou
Alice creates/restores alice.cybou

Stan opens Mail
-> writes to alice.cybou
-> drags report.pdf
-> CYBOU protects the attachment
-> Stan sends
-> Alice is offline

Alice later opens CYBOU
-> syncs
-> sees verified Mail
-> downloads/decrypts report.pdf
-> presses Save to Files
-> report.pdf appears in Files
-> Alice replies

Stan opens Files
-> uploads another file by drag/drop
-> sees it reach Protected
-> restarts CYBOU
-> file remains available
```

The demonstration should feel like familiar productivity software. The
protocol, PQ cryptography, PoA finality and distributed storage should improve
trust and resilience without becoming mandatory UI concepts.

## 12. Durable local actions and draft loss prevention

```text
[ ] slow archive/trash shows immediate pending feedback and stays responsive
[ ] Archived/Saved appears only after the relevant local durable commit
[ ] injected DB/save failure retains content and does not show success
[ ] draft discard waits for durable deletion and keeps text on failure
[ ] autosave/discard ordering and theme rebuild cannot resurrect a discarded draft
[ ] permanent Mail deletion retains failed rows and counts only committed items
[ ] multi-item failure identifies completed/failed items and retry is bounded
[ ] Undo issued during a pending move produces the intended final folder
[ ] autosave/close/lock/restart retain acknowledged drafts without duplicate send
[ ] recipient-resolution/preparation failure retains the previous draft payload
[ ] native mouse drag from real rows to Archive/Trash works at supported DPI
[ ] toolbar/context menu/keyboard/drop share the same outcome and error handling
```

## 13. Stable views and complete Files evidence

```text
[ ] unchanged snapshots do not flicker or rebuild visible rows
[ ] Advanced, scroll, selection and focus survive refresh and indexed-ID replacement
[ ] lock clears private details, compose and any diagnostic output
[ ] 1/2, unknown, stale, repair and failure states remain distinct and actionable
[ ] absent replica measurement never renders as zero copies
[ ] local availability, finality, protection and retrieval remain separate
[ ] Protected Files → Mail opens Compose or shows a specific actionable failure
[ ] unsupported pending reference is explained rather than silently enabled
[ ] duplicate destination names cannot cause the wrong folder ID to be used
[ ] recursive folder upload enumeration does not block the GUI
[ ] failed destination replacement leaves an existing downloaded file intact
[ ] Last updated/manual Refresh requests bounded work without full audit/rescan
```

## 14. Network and Advanced extensions

Run these when the corresponding surface is delivered; mark unavailable target
features explicitly instead of counting them as passed Beta acceptance.

```text
[ ] map positions are labeled illustrative and only real observed peers appear
[ ] LAN/unknown peers are not invented French city locations
[ ] local peer count/availability is never called total network/global uptime
[ ] any top-N ranking has a window/sample scope and cannot affect paid placement
[ ] quiet network/block age alone does not show false outage
[ ] map has a readable list and keyboard-accessible non-color-only states
[ ] explorer separates verified finalized facts, local candidates and off-chain evidence
[ ] displayed name/IP cannot grant signer or settlement authority
[ ] own-content inspector/console cannot enumerate foreign chunks or expose keys
[ ] running jobs and private output clear safely on lock/session replacement
[ ] output size, traversal, history retention and exports are bounded/redacted
[ ] production UI cannot launch BUILD_TESTS loadgen/smoke/soak implicitly
```

## 15. Assurance and deletion scope

```text
[ ] security labels identify capability, evidence scope and freshness
[ ] mnemonic/vault recovery distinguishes published data from local drafts/preferences
[ ] two distinct storage identities are not claimed as independent failure domains
[ ] Beta acceptance records real independent remote domains, not colocated DEV peers
[ ] Trash, finalized deletion, revocation/lease closure and purge are distinguished
[ ] missing remote purge evidence remains unknown, never a fabricated receipt
[ ] shared-reference/unlink-failure/restart purge tests preserve byte accounting
[ ] historical capsules, recipient copies and backups are disclosed as erasure limits
[ ] no blanket RGPD/certification/crypto-erasure badge is inferred from encryption
```

## 16. Authority operator acceptance

```text
[ ] Administration appears only for the locally proven genesis-authorized key
[ ] Vault lock discloses the observed background finalizer without granting controls
[ ] pause explains pending-operation consequences and defaults to Cancel/Escape
[ ] resume and one-block finalization obey the current local signer state
[ ] settlement review shows the exact prepared period, entries, accounts and amount
[ ] payout details disclose any display truncation; reviewed vector remains exact
[ ] missing local evidence is not claimed as no service or independent durability
[ ] lock/account/proof/period/escrow changes cancel stale review without submission
[ ] submission stays pending until a verified PoA block records settlement
[ ] worker submission leaves the GUI responsive and signing safety fails closed
[ ] FR/EN review labels fit at supported physical DPI and are keyboard accessible
```
