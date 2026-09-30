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

The desktop model may expose:

```text
Local
Preparing
WaitingForConfirmation
Securing
Protected
TemporarilyUnavailable
NeedsAttention
```

Mail maps `Protected` durability to user-facing `Sent`.
Incoming Mail and its attachments are `Received`: finalized, end-to-end
encrypted and opened by this Identity. The recipient has not proved the
sender's remote durability, so incoming content is never shown as
`Protected`; the reader says "Network confirmed", not "Stored on the network".

`WaitingForConfirmation` means RootPublication is not yet PoA-finalized.

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

When Authority is implemented, the GUI may show read-only:

```text
Authority
Authority tier
current generic service allowances
network contribution status
```

Qt never computes or edits Authority.

Do not present Authority as a social score or PoA voting power.

## Performance

Network, crypto, block scanning, chunk transfer, recovery and storage health
work must stay off the Qt event loop.
