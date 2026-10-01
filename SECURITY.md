# Security policy — CYBOU

CYBOU is experimental software. It is not a production communication or
storage service. The active protocol target uses a genesis-bound,
single-operator PoA finalizer. This is centralized finality and does not provide
Byzantine fault tolerance.

## Reporting a vulnerability

Report security issues privately to `security@cybou.org` or the designated
project security contact. Include the affected component, impact, and steps to
reproduce. Do not include real user secrets or recovery phrases. Allow the
maintainers time to investigate and prepare a fix before public disclosure.

## Security boundaries

- Identity recovery, account authorization, Identity KEM, PoA finality,
  release signing, and treasury authority use separate key roles.
- Production signatures require the configured Ed25519 and post-quantum
  components. Classical-only fallback is not allowed.
- The PoA signing key is genesis-bound, held in memory only, and protected by a
  durable anti-equivocation journal. Journal rollback or conflicting signing
  must fail closed.
- RootPublication exposes generic publication accounting and opaque chunk IDs.
  Application schemas, recipients, filenames, file metadata, and graph edges
  remain inside encrypted content.
- Providers verify the full ChunkID against stored encrypted bytes and require
  finalized-publication admission proofs. Finality authorizes storage; it does
  not prove durability.
- The client retains encrypted content and must distinguish finalized,
  available, and retrievable states.
- CYP2 v3 uses TLS 1.3 with the configured `X25519MLKEM768` key exchange and
  no plaintext fallback. Provider and genesis-key finalizer proofs bind the
  role to the TLS exporter and both HELLOs. The ephemeral TLS certificate is
  not itself a peer identity; ordinary peers are not globally authenticated.
- Secret files require private owner-only permissions and reject links or
  reparse points. Event logs are private and omit sensitive identifiers by
  default; LAB mode is explicitly verbose.
- Use vetted BLAKE3, HKDF-SHA256, ChaCha20-Poly1305, and the frozen hybrid KEM
  profile. Do not create custom cryptographic primitives.

## Current limits

DEV is an experimental network. Do not treat its identities, balances,
ciphertext, or keys as production assets, and do not reset its state or replace
its PoA key during routine deployments. See `AGENTS.md` and
`docs/cybou/26_IMPLEMENTATION_STATUS.md`.
