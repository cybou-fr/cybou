# Implementation status

## Active protocol target

`main` implements an experimental substrate for genesis-bound hybrid-PQ PoA,
generic RootPublication, and encrypted chunk DAGs. It is not an active DEV
protocol. The deployed DEV chain remains pre-cutover and must not be reset
until all integration and clean-machine recovery gates pass together. See
`AGENTS.md`, the authority documents in `docs/cybou/`, and
`spec/poa_chunk_dag.yaml`.

## Implemented in source

- Full 256-bit ChunkID using pinned BLAKE3 C 1.8.1 and published known-answer
  vectors.
- Bounded RFC 8949 core-deterministic canonical CBOR codec.
- Versioned encrypted chunk envelope using HKDF-SHA256 and
  ChaCha20-Poly1305, random per-node salts/nonces/padding, and network-bound
  associated data.
- Local encrypted graph builder and reader with bounded depth, fan-out, chunk
  count, and reconstructed bytes; duplicate and cyclic references are rejected.
- Canonical RootPublication CBOR body, strict resource limits, identity-bound
  recipient capsules, and size-aware deterministic integer fee calculation.
- BLAKE3 chunk authorization commitments with sorted opaque ChunkID/size sets,
  proof generation, and verification.

These components are substrate code. Their integration with the canonical
Identity operation path, state transition, block finality, and provider network
is incomplete. Do not infer network readiness from the presence of local
serialization or cryptography code.

## Cutover gates still open

- Correct mandatory BLAKE3 integration in Depends and verify Windows/vcpkg,
  Linux normal, and Linux Depends builds.
- Cross-implementation vectors for canonical CBOR, encrypted chunks/graphs,
  hybrid capsules, RootPublication authorization, and chunk admission.
- RootPublication Identity authorization, replay protection, deterministic
  state execution, and genesis/state-root integration.
- Genesis-bound PoA signing, hybrid signature verification, anti-equivocation
  journal durability, fork handling, and operator recovery.
- Finalized-publication ChunkStore admission, independent chunk placement,
  durability, retry, retention, repair, and provider-loss handling.
- Publication scanning, recursive retrieval, and clean-machine Identity,
  Mail, and Files recovery without an existing client database.
- Gmail-familiar Mail and Google Drive-familiar Files UI/UX acceptance.

The protocol target is not active on DEV. A single coordinated cutover is
permitted only after every format, finality, execution, storage, Identity/name,
and recovery gate passes. Cutover discards obsolete DEV state and vaults; do not
add runtime compatibility or automatic import.
