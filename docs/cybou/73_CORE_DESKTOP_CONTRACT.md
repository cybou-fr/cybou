# 73 — Core → Desktop contract

The desktop renders verified core/application state. It does not invent
protocol truth.

```text
core/application services → CybouDesktopModel → Qt pages
```

## Network startup and replacement

The core receives the `OfficialNetworkProfile`, contacts a pinned bootstrap,
and verifies the root-signed `OfficialNetworkBinding`, exact network definition
and genesis. Only then may it open network-bound Identity and application state
or connect to peers. Bootstrap peer hints are untrusted until normal CYP2 and
France-only admission checks pass.

On a verified newer `generation`, core stops services and pending operations,
prepares the new network domain, atomically activates it and destroys every old
network-bound item: chain/state, genesis, Identity, vault, AccountID, signing
and KEM keys, Wallet, Names, Mail, Files, Application DB, peer DB, storage
metadata and Authority indexes. The GUI neither approves nor migrates old
Identity data; it reports the completed transition, for example « Le réseau
CYBOU a été mis à jour ». A preparation failure leaves the old generation
usable. Theme, language, other application-global preferences and validated
Geo cache live outside the network domain.

An `authority_epoch` change updates the verified PoA assignment without wiping
Identity or application state. Pages never select a finalizer key.

## Data and request boundaries

Canonical core provides node/finality status, AccountID/Identity, Balance,
System Balance, Names and read-only Authority. Private application services
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

The operation axis is `Local → Preparing → Submitted → Finalized`, or `Failed`.
It applies to payment, naming, rotation and publication operations. Submitted
is displayed as waiting for confirmation. `Failed` is terminal for an exact
OperationID unless independently verified PoA finality supersedes a local
failure. Uncertain delivery remains Submitted; Finalized never regresses.

The separate content axis includes Local, Securing, Protected, Received,
Temporarily unavailable and Needs attention. Outgoing content remains Local
until finality. Finalized content enters Securing while remote replicas are
placed/repaired. Development requires one remote full replica; Beta requires
two independent remote full replicas. Local encrypted cache does not count.
Mail maps Protected to Sent. Incoming Mail is Received after verified finality
and decryption; the recipient does not claim proof of sender durability.

## Capabilities, policy and Authority

A UI capability becomes true only when its backend path is live. Mail needs
publication, scanning, retrieval and mailbox projection. Files needs private
catalog publication, retrieval and durability. Storage and PoA finalization
are optional full-node capabilities. Bootstrap is an external rendezvous
service. France-only public P2P admission is mandatory in DEV and production;
optional VPN/proxy/Tor filtering is local policy. Qt displays core decisions.

Authority is informational and read-only. The controller syncs its derived
index from finalized history and shows it only on Identity and Diagnostics,
never as a contact trust label, resource tier or PoA power.
