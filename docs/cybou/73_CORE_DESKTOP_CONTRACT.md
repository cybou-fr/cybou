# 73 — Core → Desktop contract

The desktop renders verified core/application state. It does not invent
protocol truth.

```text
core/application services → CybouDesktopModel → Qt pages
```

## Network startup and cutover

The core selects compiled `OfficialNetwork` public constants: Network Public
Key (`NetworkID`), signed immutable `NetworkGenesis`, initial genesis state,
and bootstrap locators. It verifies the compiled signature and initial state
root. No external official network/genesis file is loaded. Only then may it
open network-bound Identity and application state or connect to peers in the direct mesh.
DEVNET is the enabled profile; MAINNET has no provisioned data or bootstrap and
remains disabled in the GUI until provisioning is complete.
Bootstrap is an ordinary peer whose hints are untrusted until normal CYBOU P2P and
France-only admission checks pass.

On adopting a new official network definition (with a new Network Public Key, new
NetworkID, and new immutable genesis), the core stops services and pending operations,
prepares the new network domain, atomically activates it and destroys every old
network-bound item: chain/state, genesis, Identity, vault, AccountID, signing
and KEM keys, Wallet, Names, Mail, Files, Application DB, peer DB, storage
metadata. The GUI neither approves nor migrates old
Identity data; it reports the completed transition, for example « Le réseau
CYBOU a changé de réseau ». Theme, language, other
application-global preferences and validated Geo cache live outside the network domain.

## Data and request boundaries

Canonical core provides node/finality status, AccountID/Identity, Balance,
System Balance, Names and Authority. Private application services
provide Mail and Files projections, publication/storage progress and local
search. Each unlocked Identity uses its own encrypted rebuildable Application
DB. Pages and `CybouDesktopModel` never browse the common ChunkStore,
provider DB or foreign hosted chunks.

```text
Qt page → desktop controller → ApplicationService / PublicationService /
StorageService → NodeRuntime
```

Pages never allocate nonces, sign operations, choose KEM suites, build Merkle
proofs, select providers, classify IP addresses or infer network authenticity.
Network, crypto, scanning, transfer, recovery and health work stay off the Qt
event loop.

## Operation and content states

The operation axis is:

```text
Local → Preparing → Submitted → Finalized, or Failed
```

- **Submitted**: locally executed as valid and held in the volatile candidate pool, awaiting finality.
- **Finalized**: included in a valid block signed by the PoA key and independently verified locally. Finalized never regresses.
- **Failed**: terminal for an exact OperationID unless verified PoA finality includes it.

When a new finalized block arrives, the node re-executes held candidates
against it; one that is no longer valid becomes Failed. There is no
provisional state to roll back.

The separate outgoing content axis includes:
`Local → Securing → Protected`, or `Temporarily unavailable` / `Needs attention`.
Preparing belongs to operation/local preparation progress, not a distinct
content enum. Received is an incoming Mail state, not a successor to Protected.
Outgoing content remains Local until finality.
Finalized content enters Securing while remote replicas are placed/repaired.
Development requires 1 remote full replica; Beta requires 2 independent remote full replicas
(plus local copy = 3 physical copies total). Local encrypted cache does not count toward remote durability.
Retrieval has its own progress: Idle / Downloading / Verifying / Decrypting /
Ready. Local offline availability is independent of remote protection.
Mail maps Protected to Sent. Incoming Mail is Received after verified finality and decryption;
the recipient does not claim proof of sender durability.

## Feature availability, policy and account values

A UI feature becomes available only when its backend path is live. Mail needs
publication, scanning, retrieval and mailbox projection. Files needs private
catalog publication, retrieval and durability. Storage is intrinsic to each Full Node with explicit local capacity, not a canonical storage quota. PoA finalization
requires the private key matching genesis. Bootstrap is an ordinary CYBOU full peer
with a known locator. France-only public P2P admission is mandatory in DEV and production;
optional VPN/proxy/Tor filtering is local policy. Qt displays core decisions.

The account panel shows, from the latest finalized `AccountState`:

```text
Balance: X CYBOU
System Balance: Y CYBOU
```

The controller shows no AUTH, Age, Activity, System contribution or
scanned-height fields.

## Local command acknowledgements and stable projections (delivery target)

Archive, mailbox trash and draft save are local encrypted Application DB
commands; they do not wait for PoA finality. Their UI success requires a
successful durable local commit. Files catalog mutations and publication
revocations have a separate network lifecycle. Never reuse a local success
toast as evidence of finality, remote replication or purge.

SaveDraft, send preparation and mailbox moves now accept queued GUI progress
callbacks; the desktop model correlates them with opaque local command IDs and
the current Identity session generation. Other command paths still require
equivalent acknowledgement work. Use a correlated command identifier,
affected semantic item identifiers, session generation and queued/running/
committed/failed results. A UI/task identifier is not an OperationID and never
enters the wire or canonical state. Item identifiers must survive pending-to-
indexed reconciliation through an explicit mapping. Serialize dependent edits
and Undo; retain failed draft content and expose retry without duplicate send.

Draft send handoff keeps an encrypted local draft-to-message binding before
publication. A retry uses the bound message/job ID and resumes an existing
durable job. Successful handoff removes draft content while retaining its
binding to prevent stale compose replay from creating a second publication;
explicit draft discard removes the binding. This is local application state,
not a protocol object or canonical pending state.

Progress and evidence DTOs distinguish unknown, checking, measured, stale and
failed observations, including observation time and scope. Replica count is
the minimum over required chunks; absence of a measurement is not zero. Proven
StorageId/payout identity diversity is not proof of independent failure domains.
Expose safe blocker reasons rather than interpreting elapsed time as failure.

Unchanged projections must not emit wholesale replacement notifications.
Pages preserve selection, expanded Advanced, focus, scroll and input across
updates; lock clears private state. Manual Refresh requests a bounded snapshot
refresh; it does not implicitly run full audits, repair or history rebuilds.
Long worker tasks, shutdown drain and join must be measured for GUI stalls and
kept visibly asynchronous without violating existing service ownership.
