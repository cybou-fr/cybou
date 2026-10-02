# CYP2 transport version 3

CYP2 v3 carries peer discovery, finalized block synchronization, bounded
operation relay, advisory Validation attestation propagation, and authorized
encrypted chunk transfer over TCP/TLS 1.3.

## Connection protection and roles

TLS 1.3 is required before HELLO or application frames, with no plaintext
fallback. The configured hybrid key exchange is `X25519MLKEM768`; a build
without it fails the handshake. The ephemeral self-signed TLS certificate is
not a global peer identity. Addresses, timing and traffic sizes remain visible.

Bootstrap is an ordinary CYBOU full peer with a known locator (`IP:port` and
TLS SPKI pin). Transport discovery connects to the bootstrap locator using standard
CYP2. The official release bundles the signed immutable genesis specification (`CYG1`);
nodes independently verify genesis locally against the compiled `NetworkID = Network Public Key`
and pinned `GenesisDigest` before peer connections. Bootstrap status itself grants no
consensus role, no authority, and no `CAP_BOOTSTRAP` flag.

A `CAP_STORAGE` peer proves its stable `STORAGE_PROVIDER` key in a session
proof bound to both HELLOs and the TLS exporter. The receiver derives
`ProviderID = BLAKE3(provider public key)`.

A peer advertising `CAP_ACCEPT_OPERATIONS` operates the genesis-authorized PoA
key. Missing or invalid role proofs fail the handshake. These proofs identify
live services, not globally authenticated ordinary peers. Operation signatures,
PoA certificates, publication proofs, Validation attestations, and ChunkID checks
remain independent and mandatory.

Ordinary CYP2 frames retain the standard 4096-byte limit, while chunk transfers
use their bounded stream decoders.

## Admission and scope

Production/DEV public inbound and outbound P2P admission is France-only before
connect/accept, using local Geo data for every resolved IPv4/IPv6 address.
Missing or corrupt data fails closed. LAB loopback/private traffic needs an
explicit bypass. Optional known VPN/proxy/Tor filtering is node-local policy.

The active wire profile carries peer hints, finalized blocks, signed
operations, advisory Validation attestations, and authorized encrypted chunks.
No validator quorum, resource ticket, canonical reservation or per-I/O
accounting is part of active CYP2.
