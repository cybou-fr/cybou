# CYP2 transport version 3

CYP2 v3 carries peer discovery, finalized block synchronization, bounded
operation relay and authorized encrypted chunk transfer over TCP/TLS 1.3.

## Connection protection and roles

TLS 1.3 is required before HELLO or application frames, with no plaintext
fallback. The configured hybrid key exchange is `X25519MLKEM768`; a build
without it fails the handshake. The ephemeral self-signed TLS certificate is
not a global peer identity. Addresses, timing and traffic sizes remain visible.

The bootstrap request runs over separately pinned TLS before a NetworkID
exists; it is not a CYP2 peer role or capability. The profile's IP:port and
SPKI pin authenticate the rendezvous endpoint only. The core verifies the
returned `OfficialNetworkBinding` and Authority assignments against immutable
Network Root `R` as specified in `04_NETWORK_LIFECYCLE.md`.

A `CAP_STORAGE` peer proves its stable `STORAGE_PROVIDER` key in a session
proof bound to both HELLOs and the TLS exporter. The receiver derives
`ProviderID = BLAKE3(provider public key)`. A peer advertising
`CAP_ACCEPT_OPERATIONS` proves possession of current root-authorized PoA key
`P_epoch` for that height/epoch. Missing or invalid role proofs fail the
handshake. These proofs identify live services, not globally authenticated
ordinary peers. Operation signatures, PoA certificates, publication proofs
and ChunkID checks remain independent and mandatory.

Bootstrap response size may accommodate a signed network definition (16 MiB
plus 16 KiB); ordinary CYP2 frames retain the 4096-byte limit.

## Admission and scope

Production/DEV public inbound and outbound P2P admission is France-only before
connect/accept, using local Geo data for every resolved IPv4/IPv6 address.
Missing or corrupt data fails closed. LAB loopback/private traffic needs an
explicit bypass. Optional known VPN/proxy/Tor filtering is node-local policy.

The active wire profile carries peer hints, finalized blocks, signed
operations and finalized-publication-authorized encrypted chunks. No
validator opinion, resource ticket, canonical reservation or per-I/O
accounting is part of active CYP2.
