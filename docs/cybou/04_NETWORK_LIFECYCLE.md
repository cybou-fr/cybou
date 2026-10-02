# 04 — Network lifecycle

Status: **Active architecture target**. This document defines official network
trust, creation, joining, Validation, and network replacement.

## Official networks

A standard CYBOU installation knows two official network profiles:
- **DEVNET**: enabled official profile, bootstrap locator `51.255.46.58:29461`.
- **MAINNET**: unprovisioned, no bootstrap locator, disabled in the GUI until
  its keys, genesis, and bootstrap exist.

Each `OfficialNetwork` consists of compiled public constants: its Network
Public Key, immutable signed `NetworkGenesis` object, initial genesis state,
and bootstrap locators. Runtime loads no external official network/genesis
file and has no separate genesis digest profile pin.

Network identity is immutable:
```text
NetworkID = Network Public Key
```

The corresponding **Network Private Key**:
- is generated before network launch;
- is strictly offline and **never used online** (including in DEVNET);
- is never stored on bootstrap or on the PoA finalizer;
- is used solely by the network owner at creation time to sign the immutable genesis specification once;
- never changes an existing network;
- is the creation-time root of trust for that network.

Provisioning creates the Network secret and the ordinary `cybou.cybou`
Identity secret once. Private material stays only under gitignored `/private/`
(`devnet/` for DEVNET; `mainnet/` does not yet exist). Only public keys,
public Identity data, and signed genesis constants enter Git.

## Bootstrap peer

Bootstrap is an **ordinary CYBOU full peer**:
- runs the exact same executable as all other nodes;
- communicates using the standard CYP2 protocol;
- has no `CAP_BOOTSTRAP` capability flag;
- has no `BootstrapNode` class or distinct role in consensus;
- has an IP:port and TLS SPKI pin known in advance for initial discovery;
- bootstrap status itself grants no authority and no AUTH.

The DEV locator is `51.255.46.58:29461`; its SPKI SHA-256 pin is compiled in
`src/cybou/official_networks.h` to authenticate initial transport discovery.

## Bootstrap Identity

The bootstrap node runs an ordinary CYBOU Identity with no special consensus
grant or wire structure. Whether that Identity holds AUTH is an ordinary
GenesisAllocation decision for the network, not a property of the bootstrap
role. Genesis may give `cybou.cybou` more than 1,000,000 AUTH so the network
starts with an eligible Validation Identity.

## Network creation and joining

```text
Offline:
  Owner creates immutable genesis (params, GenesisAllocation AUTH, PoA key P)
  Owner signs genesis once with Network Private Key
  Provisioning generates public C++ constants for the official network

Online:
  Client selects compiled OfficialNetwork and verifies its signed genesis and initial state root
  Initializes local consensus state
  Connects to known bootstrap locator as an ordinary CYP2 peer
  Bootstrap seeds initial peers -> direct CYP2 mesh forms
```

Every full node independently checks the compiled signed genesis, operational PoA
certificate, block transitions, and state roots.

`cybou.cybou` is an ordinary Identity with mnemonic, AccountID, Recovery,
Authorization, KEM, Mail/support, and a distinct PoA key role. Its finalization
right comes solely from the PoA public key authorized by genesis. Its name does
not confer consensus power, and there is no separate PoA Identity entity.

## Authority and Validation

Authority is non-transferable AUTH stored in each finalized AccountState and
committed by the state root; see `57_IDENTITY_AUTHORITY.md`.

Every full node independently executes every candidate. Validation is an
additional signature by an Identity with finalized AUTH > 1,000,000 after its
own node validated the operation; it is evidence only. See `VALIDATION.md`.

## Canonical PoA finality

The Central Authority PoA finalizer:
- is the sole canonical finalizer;
- operates from the Central Authority desktop;
- independently executes every candidate;
- trusts no validator, bootstrap, or peer state;
- valid -> signs block certificate;
- invalid -> drops candidate.

Canonical truth is always the latest valid PoA-finalized state.

## No alternative finality

Validation creates no state. Only a valid PoA-finalized block changes
canonical state. There is:
- NO voting against PoA;
- NO validator fork-choice;
- NO validator quorum finality;
- NO BFT consensus.

## Immutable genesis

A NetworkID has exactly one genesis.
Nodes use the immutable signed genesis compiled for the selected official
network and MUST reject invalid signatures or a mismatched initial state root.

There is NO `genesis_generation`, NO re-genesis, and NO in-place genesis replacement inside a NetworkID.

## Official network cutover and full wipe

If an official network is replaced:
```text
new Network Public Key
-> new NetworkID
-> new immutable Genesis
-> clean local network-domain reset
```

This is an entirely new network, not a version update of the old one.
Upon adopting a new official network definition:
The core stops network services and wipes all local network-bound state cleanly:
- chain/state
- network definition
- genesis
- Identity
- vault
- AccountID
- Recovery, Authorization, and KEM keys
- balances and System Balances
- names
- Mail and Files
- Application DB
- peer DB
- pending operations
- storage metadata

No cross-network migration exists. Application preferences outside the network
domain (e.g., UI theme, language) may be retained.

## Direct P2P mesh

Bootstrap provides initial peer hints. Ordinary full nodes then exchange
finalized blocks, operation relays, Validation signatures, and encrypted
chunks directly. A bootstrap outage does not stop an already formed mesh.
Production and DEV public inbound/outbound admission is France-only and fails
closed when local Geo data is unavailable or corrupt; see
`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`.
