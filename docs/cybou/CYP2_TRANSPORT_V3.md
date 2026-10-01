# CYP2 transport version 3

CYP2 v3 carries node synchronization, operation submission, and encrypted
chunk transfer over TCP protected by TLS 1.3.

## Connection protection

- TLS 1.3 is required before CYP2 HELLO or any application frame; plaintext
  fallback is rejected.
- The configured key exchange is `X25519MLKEM768`; a build that cannot provide
  it fails the handshake.
- TLS protects record confidentiality, integrity, and ordering. Endpoint
  addresses, timing, and traffic sizes remain visible.
- The TLS certificate is ephemeral and self-signed. Special service roles are
  authenticated by proofs bound to both CYP2 HELLOs and the TLS exporter.
- A bootstrap service may use a persistent TLS certificate and private key.
  Bootstrap clients must pin the SHA-256 digest of the certificate's DER
  SubjectPublicKeyInfo from a trusted CYBOU release or another approved
  out-of-band source, and verify it immediately after TLS completes and before
  sending CYP2 HELLO. A pin learned from the same unauthenticated connection
  is not a trust anchor. Ordinary peer sessions continue to use ephemeral
  certificates.

A storage peer advertising `CAP_STORAGE` proves its stable hybrid
`STORAGE_PROVIDER` key. Peers verify the proof and derive its ProviderID. A
peer advertising `CAP_ACCEPT_OPERATIONS` proves the genesis-bound PoA
finalizer key. A missing or invalid role proof fails the handshake.

The role proofs identify providers and the canonical finalizer over this TLS
session; they do not establish a global identity for ordinary peers. CYBOU
operation signatures, PoA certificate checks, publication proofs, and ChunkID
checks remain independent and mandatory.

## CYP2 scope

The active wire profile carries finalized blocks, signed operations, peer
discovery, and finalized-publication-authorized encrypted chunks. Validation
attestations, ResourceTickets, canonical resource reservations, and per-I/O
resource accounting are not CYP2 messages.
