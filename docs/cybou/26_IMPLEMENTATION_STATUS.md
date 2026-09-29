# Implementation status

## Active protocol target

`main` defines genesis-bound hybrid-PQ PoA as the target and implements an
experimental substrate for generic RootPublication and a streaming encrypted
chunk tree. This target is not an active DEV protocol. The deployed DEV chain
remains pre-cutover and must not be reset until all integration and clean-machine
recovery gates pass together. See
`AGENTS.md`, the authority documents in `docs/cybou/`, and
`spec/poa_chunk_tree.yaml`.

## Implemented in source

- Full 256-bit ChunkID using pinned BLAKE3 C 1.8.1 and published known-answer
  vectors.
- Bounded RFC 8949 core-deterministic canonical CBOR codec.
- Versioned bytes-based encrypted chunk envelope using HKDF-SHA256 and
  ChaCha20-Poly1305, random per-chunk salts/nonces/padding, and network-bound
  associated data.
- Local streaming ROOT/INDEX/DATA tree builder and sink-based reader with bounded
  chunk buffers, fan-out, depth, and caller-supplied output limits.
- Canonical RootPublication CBOR body, strict resource limits, identity-bound
  recipient capsules, and size-aware deterministic integer fee calculation.
- BLAKE3 chunk authorization commitments in durable staging order, an O(log N)
  Merkle accumulator, bounded inclusion-proof generation/verification, and a
  compact RootPublication that does not reveal the complete ChunkID set.
- Durable content-addressed provider admission by ChunkID with finalized-
  publication lookup, per-publication proof association, provider capacity
  enforcement, and idempotent deduplication. Persistent providers keep opaque
  blobs in sharded hash-named files and admission metadata in LevelDB; startup
  reconciliation reserves bytes for missing blobs and removes orphan files.
  Provider capacity is local and is not published in RootPublication.
- `AuthorizedRootPublication` as a typed Identity-authorized operation,
  deterministic nonce/state execution, byte-and-chunk System Balance fee, and
  lookup from verified canonical finalized block history.
- A separate `POA_FINALIZER` Ed25519 + ML-DSA-65 key derivation purpose for the
  dedicated operator recovery phrase. PoA signing, genesis commitment, durable
  anti-equivocation, and finality acceptance remain cutover gates.

These components are substrate code. Their integration with the canonical
Identity operation path, state transition, block finality, and provider network
is incomplete. Do not infer network readiness from the presence of local
serialization or cryptography code.

## Cutover gates still open

- Verify pinned BLAKE3 integration in Windows/vcpkg, Linux normal, and Linux
  Depends builds.
- Cross-implementation vectors for canonical CBOR, encrypted chunks/trees,
  hybrid capsules, RootPublication authorization, and chunk admission.
- RootPublication client construction/submission, publication scanning, and
  clean-machine reconstruction of accessible roots.
- Genesis-bound PoA signing, hybrid signature verification, anti-equivocation
  journal durability, fork handling, operator recovery, and cross-implementation
  vectors for operator recovery phrase key derivation.
- Connecting provider admission to the PUT/GET peer wire; independent chunk
  placement, durability, retry, retention, repair, and provider-loss handling.
- Publication scanning, recursive retrieval, and clean-machine Identity,
  Mail, and Files recovery without an existing client database.
- Gmail-familiar Mail and Google Drive-familiar Files UI/UX acceptance.

The protocol target is not active on DEV. A single coordinated cutover is
permitted only after every format, finality, execution, storage, Identity/name,
and recovery gate passes. Cutover discards obsolete DEV state and vaults; do not
add runtime compatibility or automatic import.
