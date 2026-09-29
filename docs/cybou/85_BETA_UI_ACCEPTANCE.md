# 85 — CYBOU Beta UI/UX acceptance

Status: Beta product acceptance checklist.

This document converts the product contracts in docs 81–84 into end-to-end
acceptance scenarios. It does not replace protocol/security tests; a Beta build
must satisfy both.

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
[ ] Send is not treated as ready while required attachment durability is missing
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
[ ] Service Balance language remains consistent with docs 52 and 72
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

## 10. Beta wow-flow

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
