# CYBOU P2P transport

CYBOU P2P is one uniform Full Node baseline: finalized block serving and sync,
announcements, discovery, candidate operation relay,
transport and encrypted storage. No compatibility
negotiation, capability bitmap or network-role announcement exists.

## Transport and HELLO

TLS 1.3 with X25519MLKEM768 is mandatory; no plaintext or classical fallback.
The TLS exporter label is `EXPORTER-CYBOU-P2P`. Ephemeral certificates
are transport protection, not global peer identities. A compiled bootstrap
locator additionally requires its compiled TLS SPKI pin.

Frame header: four-byte magic `CYBP`, message-type byte, little-endian uint32
payload length (nine bytes total). Ordinary payloads remain bounded to 65,536 bytes. Larger content
uses the existing bounded chunk sequences.

HELLO is exactly 82 bytes:

| Field | Bytes |
|---|---:|
| NetworkBinding | 32 |
| finalized_height (little-endian) | 8 |
| finalized_tip | 32 |
| nonce (nonzero, little-endian) | 8 |
| listen_port (little-endian; 0 = not listening) | 2 |

NetworkBinding is SHA-256("CYBOU/NETWORK-ID" || NetworkID). HELLO contains
neither StorageId nor a PoA proof. Heights and peer tips are untrusted hints;
only independently executed, correctly PoA-signed blocks change canonical state.
No IP, endpoint, TLS session or peer declaration grants consensus authority.

Every Full Node, desktop included, listens (default port 29461, or any free port
when it is taken) and announces that port in HELLO (DEC-287). The receiver takes
the IP from the connection itself and records `IP:listen_port` only as a
candidate: it connects back, and only after a successful handshake on the same
network does the address join its known peers and its `GET_PEERS` answers. An
unreachable node (NAT without port forwarding) is therefore never advertised.
The bootstrap is an ordinary node whose address everyone knows in advance; it
shares inbound peers like any other node. No node announces that it holds the
PoA key.

## On-demand storage proof

When storage placement/admission or retrieval needs a StorageId, the requester
sends `STORAGE_PROOF_REQUEST` (24) with a fresh random 32-byte challenge.
`STORAGE_PROOF` (23) returns the STORAGE public key (purpose 8) and hybrid
signature. The signature covers domain `CYBOU/STORAGE-PROOF`, the
32-byte TLS exporter, signer HELLO, verifier HELLO and challenge, in that order.
The proven StorageId is cached only for this live storage relationship.
Reconnection requires a new proof; invalid proofs fail closed.

StorageId = BLAKE3("CYBOU/STORAGE-ID" || Ed25519 public key || ML-DSA public key).
It distinguishes remote replica identities, never nodes or PoA authority.
Two endpoints proving the same StorageId count as one independent replica.
Every Full Node implements storage; quota zero/full returns CAPACITY_EXCEEDED
for otherwise valid admissions and does not impair its other protocol functions.
Remote PUT still requires finalized RootPublication and Merkle authorization.

## Operations and sync

Every node executes candidates against its own finalized state before staging
or relaying. There are no Validation attestations (DEC-284).

All peers can relay correctly signed finalized blocks. No session identifies
the PoA key holder. Local signer activation affects block production only and
never reconnects peers. Known-peer sync completion is a liveness/UX signal,
not proof of global freshness or a security gate for Identity creation.

Production/DEV inbound and outbound admission remains France-only, with local
Geo data failing closed. Development uses the same policy. Local
rate limits protect connections, operation execution and storage proof signing.

## Compact message assignments

| ID | Message | ID | Message |
|---:|---|---:|---|
| 1 | HELLO | 14 | GET_PEERS |
| 2 | PING | 15 | PEERS |
| 3 | PONG | 16 | PUT_AUTHORIZED_CHUNK |
| 4 | GET_BLOCKS | 17 | AUTHORIZED_CHUNK_DATA |
| 5 | BLOCK_META | 18 | CHUNK_ADMISSION_RESULT |
| 6 | BLOCK_DATA | 19 | GET_CHUNK_BY_ID |
| 7 | BLOCKS_END | 20 | CHUNK_DATA |
| 8 | BLOCK_ANNOUNCE | 21 | GET_CHUNK_AUTHORIZATION_PROOF |
| 9 | BLOCK_RESULT | 22 | CHUNK_AUTHORIZATION_PROOF |
| 10 | OP_POLL | 23 | STORAGE_PROOF |
| 11 | OP_META | 24 | STORAGE_PROOF_REQUEST |
| 12 | OP_DATA | 25 | STORAGE_AUDIT_CHALLENGE |
| 13 | OP_RESULT | 26 | STORAGE_AUDIT_RESPONSE |

GET_BLOCKS requests a first height (u64 LE) and count (u8, 1–32).
For each consecutive block the responder sends BLOCK_META (height u64 LE,
encoded size u32 LE), then BLOCK_DATA frames, followed by BLOCKS_END (actual
count u8). Each block is independently executed before commit. No inventory
exchange or alternate single-block transfer exists.

Operation submission and OP_POLL share OP_META (size u32 LE, relay-PoW nonce
u64 LE; DEC-273), OP_DATA and OP_RESULT (status u8, OperationID 32). A zero
OP_META size answers an empty poll. A receiver checks the nonce against the
author's finalized tier before candidate execution.
The receiver applies ingress limits before payload allocation and independently
executes the exact signed operation. Successful OP_RESULT acknowledges the
sender's FIFO item; unsuccessful delivery preserves it for retry.

Configured peers are one ordered list of endpoint and optional TLS SPKI pin.
Compiled rendezvous locators populate that list first; discovered peers use a
separate bounded cache, filled from `GET_PEERS` answers and from inbound peers
verified by connecting back. No endpoint represents the Central Authority.
