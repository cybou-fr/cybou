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
Bootstrap is an ordinary peer whose hints are untrusted until normal CYP2 and
France-only admission checks pass.

On adopting a new official network definition (with a new Network Public Key, new
NetworkID, and new immutable genesis), the core stops services and pending operations,
prepares the new network domain, atomically activates it and destroys every old
network-bound item: chain/state, genesis, Identity, vault, AccountID, signing
and KEM keys, Wallet, Names, Mail, Files, Application DB, peer DB, storage
metadata. The GUI neither approves nor migrates old
Identity data; it reports the completed transition, for example « Le réseau
CYBOU a été réinitialisé pour une nouvelle version ». Theme, language, other
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
Local → Preparing → Submitted → Validated (optional) → Finalized, or Failed
```

- **Submitted**: locally executed as valid and held in the volatile candidate pool, awaiting finality.
- **Validated**: locally valid and holding at least one valid Validation signature from an Identity with finalized AUTH > 1,000,000, displayed as `Validated · N signatures`. Informational only; it changes no Balance, System Balance, AUTH or other state.
- **Finalized**: included in a valid block signed by the PoA key and independently verified locally. Finalized never regresses.
- **Failed**: terminal for an exact OperationID unless verified PoA finality includes it.

When a new finalized block arrives, the node re-executes held candidates
against it; one that is no longer valid becomes Failed and its Validation
signatures are dropped. There is no provisional state to roll back.

The separate content axis includes:
`Local → Preparing → Securing → Protected → Received`, or `Temporarily unavailable` / `Needs attention`.
Outgoing content remains Local until finality.
Finalized content enters Securing while remote replicas are placed/repaired.
Development requires 1 remote full replica; Beta requires 2 independent remote full replicas
(plus local copy = 3 physical copies total). Local encrypted cache does not count toward remote durability.
Mail maps Protected to Sent. Incoming Mail is Received after verified finality and decryption;
the recipient does not claim proof of sender durability.

## Capabilities, policy and Authority

A UI capability becomes true only when its backend path is live. Mail needs
publication, scanning, retrieval and mailbox projection. Files needs private
catalog publication, retrieval and durability. Storage and PoA finalization
are optional operational capabilities. Bootstrap is an ordinary CYBOU full peer
with a known locator. France-only public P2P admission is mandatory in DEV and production;
optional VPN/proxy/Tor filtering is local policy. Qt displays core decisions.

The account panel shows, from the latest finalized `AccountState`:

```text
Balance: X CYBOU
System Balance: Y CYBOU
Authority: Z AUTH
Validation eligible: Yes / No   (Yes iff Z > 1,000,000)
```

The controller does not maintain an Authority index and shows no Age,
Activity, System contribution or scanned-height fields. AUTH grants no PoA power.
