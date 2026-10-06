# CYBOU dependency boundary

This page describes the current CYBOU source and cryptographic boundary.

## Active cryptographic paths

- Protocol SHA-256 calls use `cybou/crypto/sha256.h`, backed by OpenSSL EVP.
- Identity derivation uses the CYBOU HKDF wrapper backed by OpenSSL EVP_KDF.
- Hybrid Identity and PoA signatures use the frozen Ed25519 + ML-DSA profiles.
- X-Wing draft-05 is enabled only for the DEV Identity KEM profile.
- Encrypted chunks use BLAKE3-256 identifiers and the protocol's approved
  authenticated-encryption wrapper.
- `cybou_base` owns the native `cybou/hash256.cpp` object using only the
  standard library; `cybou_core` and `cybou_node` build on it, OpenSSL and BLAKE3.
- PoA seed cleansing uses the CYBOU OpenSSL wrapper.
- `cybou-core-test` uses a CYBOU-owned temporary-directory fixture; crypto tests
  use published vectors and CYBOU-local test hex helpers.
- Signed-genesis behavior is tested directly.

## Local persistence

The node stores its records in vendored LevelDB through a CYBOU-owned record
codec (CompactSize, little-endian integers, fixed bytes). These bytes are local
persistence, not CYBOU P2P wire messages or consensus serialization.

Native Hash256 preserves protocol raw bytes and lexicographic ordering. Its
hex display follows canonical byte order, without reversal or numeric padding.
RootPublication and encrypted/private schemas have fixed-order binary layouts;
no generic CBOR decoder exists.

## Constraints

- Do not add a custom cryptographic primitive or hybrid KEM combiner.
- Do not change operation, block, state, NetworkID, commitment, or chunk-ID
  bytes as part of cleanup.
- Keep OpenSSL provider errors explicit at the CYBOU crypto wrapper boundary.
- Identity KEM capsules and full client publication flows remain integration
  work; see `26_IMPLEMENTATION_STATUS.md`.
- Core or node changes follow the DEV build, rollback, restart, and health
  verification procedure in `AGENTS.md`.
