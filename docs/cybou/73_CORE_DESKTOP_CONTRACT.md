# 73 — Core → Desktop contract

The desktop renders verified core/application state. It does not invent
protocol truth.

## Direction

```text
core/application services
-> CybouDesktopModel
-> Qt pages
```

## Data sources

Canonical core provides:

```text
node/finality status
AccountID / Identity state
Balance
System Balance
Names
Authority when implemented
```

Private application layer provides:

```text
Mail projection
Files projection
publication/storage progress
local search/index state
```

## Local data boundary

Qt pages and `CybouDesktopModel` never browse:

```text
common ChunkStore
provider DB
foreign hosted chunks
```

They consume the encrypted per-Identity Application DB through domain services.

Target request path:

```text
Qt page
-> desktop controller
-> MailService / FilesService
-> ApplicationService / PublicationService / StorageService
-> NodeRuntime
```

Pages never allocate protocol nonces, sign operations, choose KEM suites, build
Merkle proofs or select providers.

## Content states

The content axis exposes Local, Securing, Protected, Received,
TemporarilyUnavailable and NeedsAttention. Preparing and Submitted belong to
the separate operation axis; Submitted is displayed as Waiting for confirmation.
Outgoing content stays Local until PoA finality.

Mail maps `Protected` durability to user-facing `Sent`.
Incoming Mail and its attachments are `Received`: finalized, end-to-end
encrypted and opened by this Identity. The recipient has not proved the
sender's remote durability, so incoming content is never shown as
`Protected`; the reader says "Network confirmed", not "Stored on the network".

Waiting for confirmation means the operation is Submitted and not yet PoA-finalized.

`Securing` means finalized content is being placed/repaired toward the remote
durability target.

## Durability

Development target: 1 remote replica.
Beta target: 2 independent remote replicas.

The local encrypted copy is separate `Available offline`/cache state and does
not count toward `Protected`.

## Capabilities

A UI capability becomes true only when the corresponding real backend path is
live, not because a fixture demonstrates the intended UX.

Mail capability requires publication, scan, retrieval and mailbox projection.

Files capability requires private catalog/mutation publication, retrieval and
durability.

## Authority UI

The GUI may show the read-only Authority value and its finalized-history
inputs:

```text
Authority
Age
qualifying Identity activity
System Balance contribution
optional validator-qualification label
```

Qt never computes or edits Authority.

Do not present Authority as a social score or PoA voting power.

## Performance

Network, crypto, block scanning, chunk transfer, recovery and storage health
work must stay off the Qt event loop.

## Operation state, validation and Identity Authority

The desktop keeps two separate axes:

- Operation (`CybouOperationState`): Local → Preparing → Submitted →
  Validated → Finalized, or Failed. It applies to payments, name claims,
  rotation and Mail/Files publications.
- Content (`CybouContentState`): Local, Securing, Protected, Received,
  Temporarily unavailable, Needs attention. Outgoing content stays Local until
  PoA finality; its progress before that is the operation axis (Preparing,
  Waiting for confirmation, Validated). There is no pre-finality content
  state.

`Validated` means pre-finalized. It is never canonical: it does not change
balances, does not start remote storage and is never `Protected`. PoA
`Finalized` is the only canonical truth. Incoming Mail is discovered after
finality, so it never shows `Validated`. Items carry an `operation_id`; the
model keeps one `CybouOperationStatus` per operation, and
`displayedOperationState()` applies the rules above. `Validated` is shown only
while `capabilities.validation` is true (false until core validation exists)
and the user keeps "Show validation status" on. There is no trust mode.

Identity Authority (`CybouAuthoritySummary`) is an informational metric, not
social trust, a resource tier, or PoA power. The controller syncs the
read-only `AuthorityIndex` on each network refresh. It is shown only on the
Identity page and in Diagnostics, never on contacts or Mail.


`Failed` is terminal for the exact OperationID: informational updates cannot
return it to Preparing, Submitted or Validated. Retryable or uncertain delivery
remains Submitted. Independently verified PoA finality may supersede a local
failure; Finalized can never regress. CybouOperationStatus is a UI projection,
not an attestation or evidence store.
