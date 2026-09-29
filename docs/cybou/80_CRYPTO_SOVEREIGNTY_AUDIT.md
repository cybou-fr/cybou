# CYBOU dependency boundary

This page describes the current CYBOU source boundary. It is not a roadmap for
recreating inherited Bitcoin facilities that the active protocol does not use.

## Active cryptographic paths

- Protocol SHA-256 calls use `cybou/crypto/sha256.h`, backed by OpenSSL EVP.
- Identity derivation uses the CYBOU HKDF wrapper backed by OpenSSL EVP_KDF.
- Hybrid Identity and PoA signatures use the frozen Ed25519 + ML-DSA profiles.
- X-Wing draft-05 is enabled only for the DEV Identity KEM profile.
- Encrypted chunks use BLAKE3-256 identifiers and the protocol's approved
  authenticated-encryption wrapper.
- `cybou_core` links `cybou_base` and does not link `bitcoin_consensus` or
  `bitcoin_crypto` directly.
- `cybou_base` owns the shared `uint256.cpp` object and has no direct
  `bitcoin_util` link.
- `cybou_node` no longer contains the unused CYBOU GCS/compact-filter clone;
  block filtering is not part of the active CYP2 protocol. Its direct
  `bitcoin_crypto` link remains required by inherited secure serialization and
  secret-cleansing symbols used by the node and local database adapter.

## Remaining inherited boundary

The node's local LevelDB implementation uses inherited `DataStream` and
`SpanReader` helpers. These encode local database records; they are not CYP2
wire messages or consensus serialization. Replacing them is worthwhile only
when it removes a real dependency or clarifies local storage ownership, while
preserving the required database behavior.

The shared `uint256` interface also retains inherited compile-time endian,
span, and hex helpers. Do not change its byte order or the hash interpretation
as part of dependency cleanup. Protocol identifiers and consensus hashing
remain unchanged.

## Constraints

- Do not add a custom cryptographic primitive or hybrid KEM combiner.
- Do not change operation, block, state, NetworkID, commitment, or chunk-ID
  bytes as part of cleanup.
- Keep OpenSSL provider errors explicit at the CYBOU crypto wrapper boundary.
- Identity KEM capsules and full client publication flows remain integration
  work; see `26_IMPLEMENTATION_STATUS.md`.
- Core or node changes follow the DEV build, rollback, restart, and health
  verification procedure in `AGENTS.md`.
