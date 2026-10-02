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
- is used solely by the network owner to create, recreate, or edit signed genesis;
- makes the network owner the root authority of that network.

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
  Owner creates genesis (params, initial Authority assignments, PoA key P)
  Owner signs genesis with Network Private Key

Online:
  Ordinary bootstrap peer starts with signed genesis
  Clients connect to known bootstrap locator
  Verify signed genesis against compiled Network Public Key (NetworkID)
  Bootstrap seeds initial peers -> direct CYP2 mesh forms
```

Every full node independently checks the signed genesis, operational PoA
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

## Network replacement and full wipe

If the network owner issues a newer signed genesis for the network (or on network cutover):
The new genesis must carry a strictly greater `genesis_generation` (`generation > installed_generation`)
and a valid signature by the compiled Network Public Key (`NetworkID`). An older or equal generation
is rejected immediately to prevent rollback attacks.
Upon verifying the new signed genesis:
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
