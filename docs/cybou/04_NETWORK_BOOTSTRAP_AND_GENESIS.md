# Network bootstrap and genesis lifecycle

Status: **frozen target architecture; implementation and DEV cutover pending**.
This document defines the approved deployment and network-creation model. It
does not claim that the current executable or DEV deployment implements it.
Consensus format changes are not implied.

## Roles and deployment

The official VPS runs one `cybou-bootstrap` service. It provides rendezvous,
peer discovery, operation relay, and finalized-history relay/cache. It is not a
finalizer, storage provider, user Identity, or consensus authority. It has no
PoA private key and cannot create, replace, or finalize a network.

The Central Authority is the ordinary desktop Identity whose role-specific
`POA_FINALIZER` public key is committed by the network definition. After that
Identity is unlocked and the local full node has verified the current chain,
that desktop may run the existing single-operator PoA finalizer. Every desktop
continues to validate blocks and state transitions independently. Finality is
unavailable while the Central Authority desktop is offline; the bootstrap does
not take over signing.

The bootstrap may retain the network definition and finalized history for
relay and sync. It does not accept application chunks as a storage provider.
Independent nodes may run providers under their owners' local policies.

```text
official VPS: one bootstrap service, no PoA or provider role
Central Authority desktop: full node + unlocked Identity + PoA finalizer
other desktops: full nodes; optional provider role is independently operated
```

Headless finalizer/provider processes remain supported for isolated LAB and
test topologies. They are not the official DEV deployment architecture.

## Bootstrap trust and state

Bootstrap has two durable states:

- `EMPTY`: no network has been claimed.
- `BOUND`: one current network binding and monotonically increasing generation.

The initial-locator format is an address plus a TLS SPKI pin. It is used only
to find the first peer and authenticate the pre-genesis exchange. The address
grants no role; the TLS pin authenticates only that first endpoint. The
approved future endpoint and pin have not been configured yet.
After genesis, bootstrap authority belongs to the ordinary CYBOU Identity
named by a genesis grant, and the service proves that role on each live CYP2
session with its current Authorization key. Its IP and TLS certificate may
change without changing the grant. The legacy DEV P2P seed is separate from
this future initial locator and must not be reused as one.

Before the network exists, the bootstrap Identity proves possession of its
Recovery key over the pinned TLS session. The proof binds the TLS exporter, a
fresh challenge, and the Recovery public key. The Central Authority includes
the resulting RecoveryKeyID in genesis as an unclaimed bootstrap grant. The
Recovery private key stays with the bootstrap owner.

Bootstrap grants are consensus state keyed by RecoveryKeyID. A matching
`AccountCreate` claims the grant for its stable AccountID exactly once. Later
`IdentityRotate` operations change the Authorization key used for session
proofs while the bootstrap role remains with the same AccountID. Multiple
genesis grants are permitted; each grants the same service role. No address,
TLS pin, process identity, or NodeID is stored in genesis.

The signed binding contains at least the generation, display name, NetworkID,
hash of the exact network file, and genesis-bound finalizer public key. The
Central Authority signs each binding. It contains no Authority IP address,
hostname, endpoint, or persistent node identifier. Bootstrap authentication
protects the service connection; it does not replace the Authority signature.
Clients that have previously accepted a binding persist the highest generation
and reject rollback or a different binding at that generation.

Central Authority identity is possession of the genesis-bound PoA private key;
its network location is irrelevant. Any full node may connect from any address
and request the finalizer role for a network. After the ordinary bootstrap
service is authenticated, the candidate proves possession by signing a fresh
bootstrap challenge bound to that CYP2 session (including the TLS exporter and
both HELLO transcripts). Bootstrap verifies against the finalizer public key
in the already-bound network definition. Success marks only that live session
as finalizer-authenticated; it does not create a durable Authority record or
address mapping. On disconnect, bootstrap drops the route. A later session
from a new address must prove possession again. IP, DNS name, and source
address are routing data only and never authorize a role.

A peer claiming `CAP_BOOTSTRAP` proves a genesis-granted AccountID on that
session. The proof binds NetworkID, TLS exporter, both HELLO messages, and the
AccountID, and is checked against that AccountID's current Authorization key.
The route and authenticated AccountID are discarded when the session ends.

Direct node-to-node connections follow the same cryptographic rule: a peer
that claims the finalizer role proves the genesis key for that session. A
bootstrap relay is a convenient route, not the identity anchor.

A first claim requires all of:

1. an authenticated `EMPTY` response from the pinned bootstrap;
2. an unguessable, one-use activation code provisioned by the bootstrap owner;
3. proof of possession of the proposed genesis finalizer key; and
4. the pinned endpoint's Recovery-key proof, whose RecoveryKeyID is included
   in the proposed genesis bootstrap grants; and
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
in the executable. The desktop authenticates the official bootstrap and fetches
its state first:

- On `BOUND`, it downloads the exact bound network file, verifies its hash,
  definition structure, NetworkID, genesis state root/block ID, finalizer key,
  and Authority signature, then atomically stores it before starting the full
  node. It does not offer network creation.
- On authenticated `EMPTY`, and only after the user supplies the one-use
  activation code, the desktop may create a local Identity/vault, derive its
  role-specific finalizer public key, obtain and verify the initial bootstrap
  Identity claim, create genesis with that RecoveryKeyID grant, create the
  network file locally, and submit the signed initial binding. The PoA secret
  never leaves the desktop. The bootstrap receives only public network data,
  signatures, and the one-use activation code.
- The network becomes ready for bootstrap service after the granted Identity's
  `AccountCreate` is PoA-finalized. Until then, the pinned initial locator is
  the only bootstrap trust path.
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

The Central Authority full node maintains an authenticated outbound session to
bootstrap, allowing nodes behind NAT to submit operations and receive finalized
history without publishing the Authority's address. Bootstrap relays exact
operation bytes only while an authenticated finalizer session is live; it may
use a bounded in-memory queue for transient delivery, but has no durable shared
pending-operation pool. If no such session is live, clients retain exact
signed operations and report `FINALIZER_UNAVAILABLE` for retry. Bootstrap
relays/caches finalized blocks but does not decide validity or finality. The
Authority full node performs consensus checks and signs; receiving full nodes
independently verify every block, operation, certificate, and state root.

Bootstrap's finalizer-authenticated session route is ephemeral memory state,
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

The current DEV deployment and executable still use the legacy topology and
must be treated as a migration state until all of the following exist and pass:

- authenticated bootstrap `EMPTY`/`BOUND` protocol, pinning, one-use claim,
  durable atomic binding, generation rollback protection, and replacement;
- initial-locator TLS pin and Recovery-key proof, genesis bootstrap grants,
  one-time AccountCreate claim, and current Authorization-key bootstrap proof;
- desktop genesis creation and desktop finalizer lifecycle through the vault;
- bootstrap relay/discovery/history sync, with no bootstrap PoA or storage role;
- clean-install, compromised-bootstrap, replay/race, restart, offline-authority,
  and replacement acceptance tests;
- a coordinated DEV network reset and deployment plan that creates the new
  network from the Central Authority desktop and retires legacy VPS
  finalizer/provider services.

The CYP2 direct-peer foundation is implemented: an operation-accepting peer
proves the genesis finalizer key over the live TLS-exporter-bound handshake,
and the peer manager exposes that role only on its current session. This does
not implement the bootstrap service, outbound Authority route, operation relay,
or any of the cutover requirements above.

The public network-file format has a canonical serializer, and a `CYBB1`
NetworkBinding envelope can be signed by and verified against the genesis PoA
key. Verification checks the exact file hash, NetworkID, genesis contents,
display name, and generation. The core now also has a durable LevelDB
`EMPTY`/`BOUND` store: provisioning generates a 256-bit one-use activation
code, stores only its domain-separated hash, and an initial claim atomically
stores generation 1 while consuming the code. Reopening validates the stored
binding and state, and concurrent claims serialize through the store lock.
The core now has a bounded STATUS/CLAIM/REPLACE protocol handler and a
`cybou-bootstrap provision|serve` executable. Bootstrap exchanges use a
persistent server TLS identity and require the client's SPKI SHA-256 pin for
initial contact; they deliberately do not send HELLO because an empty network
has no NetworkID to put in HELLO. The persistent certificate is only an
initial locator trust anchor, not bootstrap authority. Configuring the target
endpoint and distributing its pin through a trusted release or approved
out-of-band path remain deployment work. A client-side
durable binding pin is also implemented: first acceptance requires an
explicit trust write, later generations must increase, same-generation
responses must be byte-identical, and a changed genesis finalizer key is
rejected. Signed key
replacement now has a signed data format and durable-store primitive: the
current key authorizes the exact next binding, the new key signs that binding,
and one synchronous batch archives the prior binding and advances current
state. The service accepts that dual-signed replacement and returns the new
binding. Durable client-side acceptance in the bootstrap exchange and database
rollback/recovery remain unimplemented.

Genesis bootstrap grants now use RecoveryKeyID and are claimed by AccountCreate
into stable AccountID state. A state with no grants retains the exact prior v7
encoding and state root; v8 is emitted only when grants exist. CYP2 now defines
`CAP_BOOTSTRAP` and an exporter/HELLO/AccountID-bound proof checked against the
current Authorization key resolved from a claimed genesis grant. Runtime APIs
accept an external unlocked-Identity signer and resolve remote role keys from
canonical state. A pinned-TLS challenge/response primitive now verifies the
bootstrap Recovery key against the same TLS exporter and fresh challenge.
Vault integration, initial locator values, and the network-creation client
are still pending.

This endpoint is an implementation milestone only. Authority outbound
sessions, relays/history, client application flows, pin distribution, and the
acceptance tests and coordinated DEV cutover remain incomplete. The DEV VPS
continues to run its existing legacy finalizer and two providers until those
gates are completed.

Until that cutover is accepted, do not describe the deployed legacy DEV as the
new bootstrap architecture, and do not point a new executable at it as if it
were a bootstrap. Preserve the existing network and key during routine work.
The isolated LAB may continue using explicit network files and separate
headless finalizer/provider processes.
