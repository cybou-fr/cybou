# 04 — Network lifecycle

Status: **Active architecture target**. This document defines official network
trust, creation, joining, Validation, and network replacement.

## Official networks

A standard CYBOU installation knows two official network profiles:
- **DEVNET**
- **MAINNET**

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

## Bootstrap peer

Bootstrap is an **ordinary CYBOU full peer**:
- runs the exact same executable as all other nodes;
- communicates using the standard CYP2 protocol;
- has no `CAP_BOOTSTRAP` capability flag;
- has no `BootstrapNode` class or distinct role in consensus;
- has an IP:port and TLS SPKI pin known in advance for initial discovery;
- bootstrap status itself grants no authority.

The DEV locator is `51.255.46.58:29461`; its SPKI SHA-256 pin is compiled in
`src/cybou/official_networks.h` to authenticate initial transport discovery.

## Bootstrap Identity

The bootstrap node runs an ordinary CYBOU Identity:
- no special consensus grant or wire structure;
- initial Authority is assigned directly by genesis;
- DEV bootstrap initial Authority = 1,000,001 (qualifying it for Validation).

## Network creation and joining

```text
Offline:
  Owner creates immutable genesis (params, initial Authority assignments, PoA key P)
  Owner signs genesis once with Network Private Key
  Official release packages signed genesis bundle (CYG1) with pinned NetworkID and GenesisDigest

Online:
  Client verifies bundled genesis against pinned Network Public Key (NetworkID) and GenesisDigest
  Initializes local consensus state
  Connects to known bootstrap locator as an ordinary CYP2 peer
  Bootstrap seeds initial peers -> direct CYP2 mesh forms
```

Every full node independently checks the signed genesis bundle, operational PoA
certificate, block transitions, and state roots.

## Authority and Validation

Authority is a deterministic Identity property derived exclusively from
PoA-finalized history.

Validation is optional pre-finalization:
- non-canonical evidence;
- peer chooses locally whether to trust it (`validation.enabled`);
- default minimum required signatures = 1;
- only signatures of eligible Identities count;
- eligibility requirement: `authority_from_latest_PoA_finalized_state(identity) > 1,000,000`.

## Canonical PoA finality

The Central Authority PoA finalizer:
- is the sole canonical finalizer;
- operates from the Central Authority desktop;
- independently executes every candidate;
- trusts no validator, bootstrap, or peer state;
- valid -> signs block certificate;
- invalid -> drops candidate.

Canonical truth is always the latest valid PoA-finalized state.

## Conflict resolution and unconditional rollback

If provisional Validation conflicts with PoA finality:
```text
discard provisional state
rollback provisional effects
adopt PoA-finalized state unconditionally
```

There is:
- NO voting against PoA;
- NO validator fork-choice;
- NO validator quorum finality;
- NO merge of conflicting provisional state;
- NO BFT consensus.

## Immutable genesis

A NetworkID has exactly one genesis.
Nodes MUST reject any genesis whose canonical digest differs
from the GenesisDigest pinned for that official network.

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
- Authority indexes

No cross-network migration exists. Application preferences outside the network
domain (e.g., UI theme, language) may be retained.

## Direct P2P mesh

Bootstrap provides initial peer hints. Ordinary full nodes then exchange
finalized blocks, operation relays, validation attestations, and encrypted
chunks directly. A bootstrap outage does not stop an already formed mesh.
Production and DEV public inbound/outbound admission is France-only and fails
closed when local Geo data is unavailable or corrupt; see
`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`.
