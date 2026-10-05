# 04 — Network lifecycle

## Uniform Full Node invariant

CYBOU defines exactly one network node type: Full Node. Every Full Node
implements the complete CYBOU P2P baseline: blocks, announcements,
discovery, operation relay and encrypted storage. There
is no capability bitmap and no network role announcement. Storage is intrinsic;
capacity is local policy. Bootstrap is only a known locator of an ordinary Full
Node. PoA is
possession of the private key matching the public key in genesis, with durable
signing safety. IP, endpoints, TLS sessions, StorageId and peer declarations
never confer consensus authority. StorageId is proven on demand only for a
storage relationship. Peer sync completion is a liveness/UX hint, never proof
of global freshness or a prerequisite for creating an Identity.

Status: **Active architecture target**. This document defines official network
trust, creation, joining, and network replacement.

## Official networks

Before MAINNET, an explicitly authorized destructive DEVNET exercise may reset
all participating nodes to height zero using the same exact genesis and keys.
Follow the coordinated stop/archive/clear/single-signer procedure in `AGENTS.md`.
This changes no NetworkID or signed genesis. Historical signed blocks remain valid;
their reintroduction can replay or conflict with the restarted exercise. It is
not production recovery and does not disable normal signing safety or conflict checks.

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

Provisioning creates the Network secret and the ordinary `cybou.cybou` and
`bootstrap` Identity secrets once. A new network may keep `cybou.cybou`'s
existing phrase and PoA key (`cybou-provision create-devnet ...
--keep-central-authority FILE`); AccountID is network-instance state created on
initial network onboarding. The Network key is always new, so the new
network always has a new NetworkID. Private material stays only under gitignored `/private/`
(`devnet/` for DEVNET; `mainnet/` does not yet exist). Only public keys,
public Identity data, and signed genesis constants enter Git.

The current DEVNET is the storage-economy network without AUTH or Validation,
provisioned on 2026-10-04 (DEC-283, DEC-284). Its NetworkBinding is
`eee26eca805d5a16b2f550d66b3355ecf50d90c43138bc1fefc34df92f7f3665`
and its signed genesis anchor is
`1abf2355b6597e53c6affb73d4c92bacc9b970f956a1a259bac8c3699f958cfc`.
Genesis allocations: `cybou` (Central Treasury; `cybou.cybou` phrase and PoA key
kept from the previous DEVNET) 100,000,000,000 CYBOU; `bootstrap` (ordinary
Identity of the bootstrap operator, phrase kept) 0 CYBOU. By operator decision
this DEVNET Network Root was generated on a networked machine: DEVNET is
disposable and re-provisioned often; MAINNET keys keep the strictly offline
ceremony. The previous DEVNETs (`6d202ccf…2d97`, anchor `e0d3d6ee…c25f`, with
AUTH; `846e8f22…b311`, anchor `5bd33c6c…a9f2`) are retired.
The preceding NetworkIDs are permanently retired. No prior genesis was re-signed
or replaced, and no prior network-bound state is imported.

## Bootstrap peer

Bootstrap is an **ordinary CYBOU full peer**:
- runs the exact same executable as all other nodes;
- communicates using the standard CYBOU P2P protocol;
- announces no network role;
- has no `BootstrapNode` class or distinct role in consensus;
- has an IP:port and TLS SPKI pin known in advance for initial discovery;
- bootstrap status itself grants no authority.

The DEV locator is `51.255.46.58:29461`; its SPKI SHA-256 pin is compiled in
`src/cybou/official_networks.cpp` to authenticate initial transport discovery.

## Bootstrap Identity

The bootstrap node runs an ordinary CYBOU Identity with no special consensus
grant or wire structure. Whatever CYBOU that Identity holds is an ordinary
GenesisAllocation decision for the network, not a property of the bootstrap
role.

## Network creation and joining

```text
Offline:
  Owner creates immutable genesis (params, GenesisAllocation CYBOU, PoA key P)
  Owner signs genesis once with Network Private Key
  Provisioning generates public C++ constants for the official network

Online:
  Client selects compiled OfficialNetwork and verifies its signed genesis and initial state root
  Initializes local consensus state
  Connects to known bootstrap locator as an ordinary CYBOU P2P peer
  Bootstrap seeds initial peers -> direct CYBOU P2P mesh forms
```

Every full node independently checks the compiled signed genesis, operational PoA
certificate, block transitions, and state roots.

`cybou.cybou` is an ordinary Identity with mnemonic, AccountID, Recovery,
Authorization, KEM, Mail/support, and a distinct PoA key role. Its finalization
right comes solely from the PoA public key authorized by genesis. Its name does
not confer consensus power, and there is no separate PoA Identity entity.

## Candidate execution

Every full node independently executes every candidate against its latest
finalized state and relays only locally valid ones.

## Canonical PoA finality

The Central Authority PoA finalizer:
- is the sole canonical finalizer;
- operates from the Central Authority desktop;
- independently executes every candidate;
- trusts no bootstrap or peer state;
- valid -> signs block certificate;
- invalid -> drops candidate.

Canonical truth is always the latest valid PoA-finalized state.

## No alternative finality

Only a valid PoA-finalized block changes canonical state. There is:
- NO voting against PoA;
- NO validators, Validation signatures or quorum finality;
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
finalized blocks, operation relays and encrypted
chunks directly. Every node listens and shares the peers it reaches and the
peers that reach it (verified by connecting back, DEC-287), so the mesh does not
depend on any node's configuration file. A bootstrap outage does not stop an already formed mesh.
Production and DEV public inbound/outbound admission is France-only and fails
closed when local Geo data is unavailable or corrupt; see
`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`.

## Simplified implementation boundary

The only node type is Full Node. Nodes announce no roles or capabilities.
PoA authority is possession of the genesis-authorized private key. Storage is
intrinsic; StorageId proves possession of a cryptographic storage key only. Rendezvous is a known
location of an ordinary Full Node. Runtime takes VerifiedNetworkGenesis and
uses its signed specification digest as the height-zero chain anchor.

PublicationService stages directly into pinned local encrypted chunks, stores
one encrypted ordered leaf list and generates Merkle proofs on demand in RAM.
RootPublication wire and encrypted/private schema are bounded binary
layouts with exact consumption. Hash256 hex follows its raw 32-byte order.
No legacy runtime, CBOR or reversed-hash decoder is retained. Provisioning and
network cutover require the previously established operator authorization.

Provisioning is offline-only in the separately built `cybou-provision` tool.
Production `cybou` has no provisioning command or Network Root derivation/signing
path. `verify-devnet PRIVATE_DIR` checks existing private material against compiled
public constants without signing. `create-devnet` accepts only fresh output paths;
it never replaces existing constants or secrets. The current current DEVNET NetworkID
is retained; the prior DEVNET is retired.
