# CYP2 transport version 5

CYP2 v5 is one uniform Full Node baseline: finalized block serving and sync,
inventory and announcements, discovery, candidate operation relay, Validation
transport and encrypted storage. Version 4 is rejected; no compatibility
negotiation, capability bitmap or network-role announcement exists.

## Transport and HELLO

TLS 1.3 with X25519MLKEM768 is mandatory; no plaintext or classical fallback.
The TLS exporter label is `EXPORTER-CYBOU-CYP2-V5`. Ephemeral certificates
are transport protection, not global peer identities. A compiled bootstrap
locator additionally requires its compiled TLS SPKI pin.

Frame header: `CYP2`, version byte 5, message-type byte, little-endian uint32
payload length. Ordinary payloads remain bounded to 4096 bytes. Larger content
uses the existing bounded chunk sequences.

HELLO is exactly 80 bytes:

| Field | Bytes |
|---|---:|
| NetworkBinding | 32 |
| finalized_height (little-endian) | 8 |
| finalized_tip | 32 |
| nonce (nonzero, little-endian) | 8 |

NetworkBinding is SHA-256("CYBOU/NETWORK-ID/V6" || NetworkID). HELLO contains
neither ProviderID nor a PoA proof. Heights and peer tips are untrusted hints;
only independently executed, correctly PoA-signed blocks change canonical state.
No IP, endpoint, TLS session or peer declaration grants consensus authority.

## On-demand storage proof

When storage placement/admission or retrieval needs a ProviderID, the requester
sends `GET_PROVIDER_PROOF` (51) with a fresh random 32-byte challenge.
`PROVIDER_PROOF` (36) returns the STORAGE_PROVIDER public key and hybrid
signature. The signature covers domain `CYBOU/CYP2/PROVIDER-PROOF/v5`, the
32-byte TLS exporter, signer HELLO, verifier HELLO and challenge, in that order.
The proven ProviderID is cached only for this live storage relationship.
Reconnection requires a new proof; invalid proofs fail closed.

ProviderID = BLAKE3("CYBOU/PROVIDER-ID/v1" || Ed25519 public key || ML-DSA public key).
It distinguishes remote replica identities, never nodes, AUTH or PoA authority.
Two endpoints proving the same ProviderID count as one independent replica.
Every Full Node implements storage; quota zero/full returns CAPACITY_EXCEEDED
for otherwise valid admissions and does not impair its other protocol functions.
Remote PUT still requires finalized RootPublication and Merkle authorization.

## Operations, Validation and sync

Every node executes candidates against its own finalized state before staging
or relaying; signatures from other nodes never substitute execution.
VALIDATION_ATTESTATION_POLL (49) and VALIDATION_ATTESTATION (50) retain their
existing encodings and rules: only locally held valid candidates, current
finalized base, eligible AUTH > 1,000,000 and valid Identity Authorization signature.

All peers can relay correctly signed finalized blocks. No session identifies
the PoA key holder. Local signer activation affects block production only and
never reconnects peers. Known-peer sync completion is a liveness/UX signal,
not proof of global freshness or a security gate for Identity creation.

Production/DEV inbound and outbound admission remains France-only, with local
Geo data failing closed; LAB private traffic needs an explicit bypass. Local
rate limits protect connections, operation execution and storage proof signing.
