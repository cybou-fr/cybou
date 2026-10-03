# CYP2 transport version 4

CYP2 v4 carries peer discovery, finalized block synchronization, bounded
operation relay, Validation attestation propagation, and authorized
encrypted chunk transfer over TCP/TLS 1.3.

## Connection protection and roles

TLS 1.3 is required before HELLO or application frames, with no plaintext
fallback. Session role proofs use the TLS exporter label
`EXPORTER-CYBOU-CYP2-V4`. The configured hybrid key exchange is `X25519MLKEM768`; a build
without it fails the handshake. The ephemeral self-signed TLS certificate is
not a global peer identity. Addresses, timing and traffic sizes remain visible.

Bootstrap is an ordinary CYBOU full peer with a known locator (`IP:port` and
TLS SPKI pin). Transport discovery connects to the bootstrap locator using standard
CYP2. The official release compiles the Network Public Key (`NetworkID`),
immutable signed `NetworkGenesis`, initial state, and bootstrap locators as public
constants. Nodes verify the compiled genesis signature and initial state root
before peer connections; no official network file is loaded. Bootstrap status itself grants no
consensus role, no authority, and no `CAP_BOOTSTRAP` flag.

A `CAP_STORAGE` peer proves its stable `STORAGE_PROVIDER` key in a session
proof bound to both HELLOs and the TLS exporter. The receiver derives
`ProviderID = BLAKE3(provider public key)`.

A peer advertising `CAP_FINALIZER_PROOF` operates the genesis-authorized PoA
key. Missing or invalid role proofs fail the handshake. These proofs identify
live services, not globally authenticated ordinary peers. Operation signatures,
PoA certificates, publication proofs, Validation signatures, and ChunkID checks
remain independent and mandatory.

Ordinary CYP2 frames retain the standard 4096-byte limit, while chunk transfers
use their bounded stream decoders.

## Frame version 4: candidate execution and Validation attestations

The frame header version byte is 4; frames of any other version are rejected,
with no compatibility path. Before staging or forwarding an operation
(`OP_META` or `OPERATION_RELAY_*`), every full node executes it on its own
finalized state and refuses it if invalid.

Between two `CAP_OPERATION_RELAY` peers, `VALIDATION_ATTESTATION_POLL` (49,
empty payload) asks for one `ValidationAttestation` the serving session has
not yet sent on its current finalized base; `VALIDATION_ATTESTATION` (50)
carries one serialized attestation (2,709 bytes) or an empty payload when none
is new. The receiver stores it, and offers it onward, only if it already holds
the operation as a candidate it executed itself, the base is its finalized tip,
the signer's finalized AUTH exceeds 1,000,000 and the Authorization signature
verifies. There is no validator capability bit.

`CAP_FINALIZER_PROOF` only announces the in-session PoA key proof; operations
never take a preferred route to the finalizer and travel the ordinary relay.
A connection to a compiled bootstrap locator additionally requires the
locator's TLS SPKI pin; the locator node serves a stable certificate.

The 32-byte network field of HELLO and of every signature domain is the
NetworkBinding, SHA-256("CYBOU/NETWORK-ID/V6" || NetworkID).

## Admission and scope

Production/DEV public inbound and outbound P2P admission is France-only before
connect/accept, using local Geo data for every resolved IPv4/IPv6 address.
Missing or corrupt data fails closed. LAB loopback/private traffic needs an
explicit bypass. Optional known VPN/proxy/Tor filtering is node-local policy.

The active wire profile carries peer hints, finalized blocks, signed
operations, Validation attestations, and authorized encrypted chunks.
No validator quorum, resource ticket, canonical reservation or per-I/O
accounting is part of active CYP2.
