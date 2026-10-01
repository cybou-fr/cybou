# Network bootstrap and genesis lifecycle

Status: **revised target architecture; implementation and DEV cutover pending**.
This document defines the approved node-capability, genesis-bootstrap, and
France-only public P2P admission model. The current executable and DEV
deployment have not completed that migration. There is no production or Beta
network; the running VPS chain is a development testnet. Keep it available for
routine development until the acceptance matrix and planned new-genesis DEV
cutover are complete. That cutover replaces the testnet, not a production
network.

## Roles and deployment

Every participant runs the same full-node software. Bootstrap, storage,
advisory Validation, and PoA finalization are optional local capabilities, not
protocol node classes or separate consensus roles. Operation relay is a
baseline full-node capability, not a bootstrap role. A network genesis
authorizes one to four bootstrap Identities. Each bootstrap is an ordinary
full node that may provide rendezvous, peer discovery, and finalized-history
relay/cache. Every full node may relay operations; bootstrap capability does
not provide PoA or storage authority by itself.

The Central Authority is the ordinary desktop Identity whose role-specific
`POA_FINALIZER` public key is committed by the network definition. After that
Identity is unlocked and the local full node has verified the current chain,
that desktop may run the existing single-operator PoA finalizer. Every desktop
continues to validate blocks and state transitions independently. Finality is
unavailable while the Central Authority desktop is offline; the bootstrap does
not take over signing.

Bootstrap-capable nodes may retain the network definition and finalized
history for relay and sync. They accept application chunks only when they
separately run the storage capability and satisfy the same finalized
RootPublication rules as any provider.

```text
TARGET ARCHITECTURE: ordinary CybouNode full node + optional bootstrap capability
CURRENT IMPLEMENTATION PROTOTYPE: cybou-bootstrap executable (transitional prototype only)

1–4 genesis-authorized Identities: optional bootstrap capability
Central Authority desktop: full node + unlocked Identity + PoA finalizer
any node: optional storage and advisory Validation capabilities
```

Headless finalizer/provider processes remain supported for isolated LAB and
test topologies. They are not the official DEV deployment architecture.

## Bootstrap trust and state

Bootstrap has two durable states:

- `EMPTY`: no network has been claimed.
- `BOUND`: one current network binding and monotonically increasing generation.

The initial-locator list contains one to four numeric IP:port endpoints and
TLS SPKI pins. It is used only to find candidate peers and authenticate the
pre-genesis exchange. Neither an address nor a pin grants a post-genesis role.
The target locator and pins are not configured yet. The legacy DEV P2P seed is
separate and must not be reused as an initial locator.

Before the network exists, each candidate proves control of a proposed stable
`AccountID` and its Recovery key over the pinned TLS session. The signed
message binds the protocol domain, fresh client nonce, TLS exporter,
`AccountID`, and Recovery public key. The Central Authority verifies the proof
and records both `AccountID` and `RecoveryKeyID` in genesis. Recovery private
material remains inside the candidate's unlocked vault boundary.

Bootstrap grants are consensus state keyed by stable `AccountID`, each storing
the expected `RecoveryKeyID` and a claimed flag. A matching `AccountCreate`
claims the grant only when both values match. A mismatch leaves it unclaimed.
The genesis roster has between one and four distinct AccountIDs and distinct
RecoveryKeyIDs. Later `IdentityRotate` operations change the current
Authorization key used for session proofs while the bootstrap capability
remains with the same AccountID. No address, TLS pin, process identity, or
NodeID is stored in genesis. Changing the roster requires a signed network
replacement; there is no BootstrapAdd/BootstrapRemove operation.

The signed binding contains at least the generation, display name, NetworkID,
hash of the exact network file, and genesis-bound finalizer public key. The
Central Authority signs each binding. It contains no Authority IP address,
hostname, endpoint, or persistent node identifier. Bootstrap authentication
protects the service connection; it does not replace the Authority signature.
Clients that have previously accepted a binding persist the highest generation
and reject rollback or a different binding at that generation.

Central Authority identity is possession of the genesis-bound PoA private key;
its network location is irrelevant to authorization. It must still pass the
same France-only public peer admission as every other node. After the ordinary bootstrap
service is authenticated, the candidate proves possession by signing a fresh
bootstrap challenge bound to that CYP2 session (including the TLS exporter and
both HELLO transcripts). Bootstrap verifies against the finalizer public key
in the already-bound network definition. Success marks only that live session
as finalizer-authenticated; it does not create a durable Authority record or
address mapping. On disconnect, bootstrap drops the route. A later session
from a new address must prove possession again. IP, DNS name, and source
address are routing data only and never authorize a role.

A peer claiming `CAP_BOOTSTRAP` proves a claimed genesis-granted AccountID on
that session. The proof binds NetworkID, TLS exporter, both HELLO messages,
and AccountID, and is checked against that AccountID's current Authorization
key. The route and authenticated AccountID are discarded when the session
ends. Bootstrap nodes do not vote, form a quorum, or finalize blocks.

## Sovereign peer admission

Production and DEV public P2P admission is restricted to French IP space for
inbound and outbound connections. The same local admission policy applies to
bootstrap candidates, providers, ordinary peers, and the Central Authority.
DNS names are resolved first; every resulting numeric IPv4/IPv6 address is
classified locally. Country suffixes and remote GeoIP services are not
evidence. An unavailable or corrupt mandatory Geo dataset fails closed for
public P2P. LAB loopback/private traffic requires an explicit LAB bypass.

Optional known VPN/proxy/Tor filtering uses local data and can only reject
addresses classified by that data. It is not a consensus rule or a guarantee
against unknown relays, tunnels, or reclassification errors. Missing optional
filter data blocks new public peers only when that setting is enabled.

Direct node-to-node connections follow the same cryptographic rule: a peer
that claims the finalizer role proves the genesis key for that session. A
any full-node relay is a convenient route, not the identity anchor.

A first claim requires all of:

1. an authenticated `EMPTY` response from the pinned bootstrap;
2. an unguessable, one-use activation code provisioned by the bootstrap owner;
3. proof of possession of the proposed genesis finalizer key; and
4. each selected pinned endpoint's Recovery-key proof bound to its proposed
   AccountID, with the AccountID and RecoveryKeyID included in genesis; and
5. an atomic bootstrap transition from `EMPTY` to `BOUND`.

The activation code is consumed only when the binding is durably committed and
is never a PoA or Identity key. Empty-state authentication and this claim flow
must be tested against races, replay, interruption, and bootstrap database
rollback before DEV cutover.

For replacement of a bound network, the current Authority key signs the
replacement request and the new finalizer key proves possession. If the key
changes, both old and new keys sign. Bootstrap archives the old binding and
atomically advances the generation. A new installation that has no previously
trusted binding needs a trust path for the current Authority key (for example,
a release-pinned key or an owner-signed binding); a self-asserted key included
in a bootstrap response is not enough to protect that installation if the
bootstrap itself is compromised.

## Desktop network creation and startup

Production/DEV startup must not silently install a network definition bundled
in the executable. The desktop authenticates its configured bootstrap peers
and checks their state first:

- On `BOUND`, it downloads the exact bound network file, verifies its hash,
  definition structure, NetworkID, genesis state root/block ID, finalizer key,
  and Authority signature, then atomically stores it before starting the full
  node. It does not offer network creation.
- On authenticated `EMPTY`, and only after the user supplies the required
  one-use activation codes, the desktop may create/unlock its local
  Identity/vault, derive its role-specific finalizer public key, obtain and
  verify one to four initial bootstrap Identity claims, create genesis with
  those AccountID/RecoveryKeyID grants, create the network file locally, and
  submit the same signed initial binding to every selected bootstrap node.
  The PoA secret never leaves the desktop. Bootstrap nodes receive only public
  network data, signatures, and their own one-use activation code.
- The network is operational when genesis exists, the Central Authority full
  node is active, and at least one bootstrap grant is claimed by a finalized
  `AccountCreate`. Report the registered/available bootstrap count out of the
  genesis roster; one unavailable candidate does not block the network. Until
  a grant is claimed, the pinned initial locators are the only bootstrap trust
  path.
- On unavailable, unauthenticated, malformed, or inconsistent bootstrap
  status, the desktop fails closed and offers retry/recovery. It must not
  infer `EMPTY` from a timeout, an empty file, or an unverified peer.

The network display name belongs to the operational binding and is not added
to consensus serialization solely for presentation. NetworkID remains derived
from the existing immutable network definition.

The existing runtime accepts recovery entropy to sign PoA blocks. A future
signer interface may keep that material behind the unlocked vault boundary;
it must preserve durable journal-before-sign ordering, fail-closed
anti-equivocation, and must never send signing material to bootstrap.

## Relay and validation boundaries

The Central Authority full node may maintain authenticated outbound sessions
to any full nodes in the P2P mesh, allowing clients to submit operations
through whichever connected peer currently has a live Authority route. A relay
forwards exact operation bytes only while an authenticated finalizer session is
live; it may use a bounded in-memory queue for transient delivery, but has no
durable shared pending-operation pool. Bootstrap membership is not required.
If no route is live, clients retain exact signed operations and report
`FINALIZER_UNAVAILABLE` for retry. Relays/caches of finalized blocks do not
decide validity or finality. The Authority full node performs consensus checks
and signs; receiving full nodes independently verify every block, operation,
certificate, and state root.

Each relay's finalizer-authenticated session route is ephemeral memory state,
not part of the durable NetworkBinding. Session teardown removes that route.
The transport/session design does not make it safe to run independent
simultaneous finalizers with the same private key. One active signer and the
existing durable anti-equivocation journal remain required; key replication
and signer failover need a separately specified safety mechanism.

Bootstrap relay/cache data is not canonical merely because it came from the
official service. Only a verified PoA-finalized block advances local canonical
state. Bootstrap does not become a chunk provider and cannot authorize remote
chunk admission.

## Cutover requirements

The DEV VPS currently runs the legacy testnet topology described in
`AGENTS.md`. Keep it operational until the acceptance gates below pass and the
new-genesis cutover is coordinated. Desktop finalization and peer discovery
remain incomplete, and there is no production/Beta network migration. The
target network remains blocked on all of the following:

- authenticated bootstrap `EMPTY`/`BOUND` protocol, pinning, one-use claim,
  durable atomic binding, generation rollback protection, and replacement;
- initial-locator TLS pin and Recovery-key proof, genesis bootstrap grants,
  one-time AccountCreate claim, and current Authorization-key bootstrap proof;
- one-to-four bootstrap roster validation, France-only peer admission for all
  public P2P paths, local Geo data integrity/fail-closed behavior, and LAB
  bypass tests;
- desktop genesis creation and desktop finalizer lifecycle through the vault;
- ordinary full-node relay/discovery/history sync, with no bootstrap PoA or
  storage role;
- clean-install, compromised-bootstrap, replay/race, restart, offline-authority,
  and replacement acceptance tests;
- a coordinated plan that replaces the disposable DEV testnet with a new
  genesis created from the Central Authority desktop and retires the legacy
  VPS finalizer/provider services.

For the current implementation prototype (`cybou-bootstrap` utility, LevelDB store, v8 prototype state vs v9 target schema, and open cutover gates), see [`26_IMPLEMENTATION_STATUS.md`](26_IMPLEMENTATION_STATUS.md).

Until the cutover gates pass and coordinated cutover is executed:
- Keep the current DEV testnet operational during routine development; do not reset its state or replace its PoA key before the planned cutover.
- Do not describe the bootstrap prototype as a complete target network.
- Do not point CYP2 executables at the bootstrap protocol port as if it were a peer endpoint.
- The isolated LAB may continue using explicit network files and separate headless processes.
