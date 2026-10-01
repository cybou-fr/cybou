# CYP2 transport version 3

CYP2 v3 carries node synchronization, operation submission, and encrypted
chunk transfer over TCP protected by TLS 1.3.

## Connection protection

- TLS 1.3 is required before CYP2 HELLO or any application frame; plaintext
  fallback is rejected. The bootstrap exchange is the explicit exception to
  HELLO: it sends its bootstrap request only after pinned TLS, since an empty
  network has no NetworkID for HELLO.
- The configured key exchange is `X25519MLKEM768`; a build that cannot provide
  it fails the handshake.
- TLS protects record confidentiality, integrity, and ordering. Endpoint
  addresses, timing, and traffic sizes remain visible.
- The TLS certificate is ephemeral and self-signed. Special service roles are
  authenticated by proofs bound to both CYP2 HELLOs and the TLS exporter.
- The compiled initial locator carries an SPKI SHA-256 pin for pre-genesis
  first contact. That pin authenticates only that initial endpoint; it does
  not grant a network role. The pinned session challenges the bootstrap
  Recovery key and verifies its proof against the same TLS exporter. After
  genesis, peers authenticate bootstrap service through its genesis grant and
  session proof. Ordinary peer sessions continue to use ephemeral certificates.
- Bootstrap request/response frames have a 16 MiB plus 16 KiB payload bound
  for the signed network definition. All ordinary CYP2 frames retain the 4096
  byte limit.

A storage peer advertising `CAP_STORAGE` proves its stable hybrid
`STORAGE_PROVIDER` key. Peers verify the proof and derive its ProviderID. A
peer advertising `CAP_ACCEPT_OPERATIONS` proves the genesis-bound PoA
finalizer key. A missing or invalid role proof fails the handshake.

A peer advertising `CAP_BOOTSTRAP` proves its stable AccountID with the current
Identity Authorization key. The receiver resolves that key only when finalized
state contains a claimed genesis bootstrap grant for the AccountID. The
signature binds NetworkID, TLS exporter, both HELLO transcripts, and AccountID.

The role proofs identify providers and the canonical finalizer over this TLS
session; they do not establish a global identity for ordinary peers. CYBOU
operation signatures, PoA certificate checks, publication proofs, and ChunkID
checks remain independent and mandatory.

## CYP2 scope

The active wire profile carries finalized blocks, signed operations, peer
discovery, and finalized-publication-authorized encrypted chunks. Validation
attestations, ResourceTickets, canonical resource reservations, and per-I/O
resource accounting are not CYP2 messages.
