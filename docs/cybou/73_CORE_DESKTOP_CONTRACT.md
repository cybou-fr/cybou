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
metadata and Authority indexes. The GUI neither approves nor migrates old
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
Local → Preparing → Submitted → Validated (optional, provisional) → Finalized, or Failed
```

- **Submitted**: staged in volatile relay memory, awaiting confirmation.
- **Validated**: received at least one valid attestation from an Identity with finalized Authority > 1,000,000 under active local validation policy. Displayed as informational provisional confirmation.
- **Finalized**: included in a valid block signed by the PoA key and independently verified locally. Finalized never regresses.
- **Failed**: terminal for an exact OperationID unless verified PoA finality includes it.

### Provisional rollback lifecycle

If a conflicting PoA block finalizes, or a new finalized state renders a provisionally
`Validated` operation invalid:
```text
VALIDATED
    ↓ conflicting/new PoA finalization
ROLLBACK
    ↓
re-evaluate original operation against new FINALIZED state
    ├─ still valid -> PENDING / SUBMITTED again
    └─ invalid     -> DROP / FAILED
```

The separate content axis includes:
`Local → Preparing → Securing → Protected → Received`, or `Temporarily unavailable` / `Needs attention`.
Outgoing content remains Local until finality (or provisional admission under local provider policy).
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

Authority is a deterministic property derived from finalized history.
Finalized Authority > 1,000,000 qualifies an Identity for provisional Validation.
The controller displays Authority on Identity and Diagnostics, never as a contact
trust label, resource tier, or PoA power.
