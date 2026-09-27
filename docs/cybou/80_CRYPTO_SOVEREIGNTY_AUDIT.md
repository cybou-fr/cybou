# Crypto sovereignty audit

This audit records the repository state and implementation gates for reducing
CYBOU's dependency on inherited Bitcoin cryptography. It does not authorize a
consensus hash change or a new cryptographic protocol.

## Repository state

CYBOU sources still depend on inherited `uint256`, `CSipHasher`, and some
Bitcoin utility APIs.
The Bitcoin cryptography dependency therefore remains; migrating SHA-256 does
not by itself remove it.

The remaining inherited utility use has multiple distinct owners, so the
final `bitcoin_util` link closure cannot be removed by changing one CMake link
line:

- `cybou_base` exports the inherited `uint256` type and builds its object with
  standard-library code; its `bitcoin_util` link edge has been removed. The
  header still uses inherited compile-time endian, span, and hex helpers.
- `cybou_node` uses inherited `DataStream`/`SpanReader` serialization for its
  local LevelDB records. Those bytes form the existing local database format.
- The GCS filter uses inherited `CSipHasher`; its FastRange64,
  CompactSize, bitstream, and Golomb-Rice codec are CYBOU-owned and checked
  against the inherited implementation across element counts including the
  CompactSize 252/253 boundary.

The actual MinGW link command confirms `bitcoin_util` and `bitcoin_crypto` are
still in the final CYBOU test executable's link closure. A true removal needs
separate CYBOU-owned hash-value and serialization APIs with explicit byte
compatibility coverage; SipHash remains an additional crypto blocker.

## Progress

- A `cybou_base` target now owns the `uint256.cpp` object, and
  `bitcoin_consensus` consumes that target instead of compiling a duplicate.
- `cybou_core` now links `cybou_base` directly and no longer links
  `bitcoin_consensus`.
- `cybou_core` no longer links `bitcoin_crypto`; the remaining inherited
  `CSipHasher` consumer is `cybou_node`'s GCS filter, so that target now owns
  the direct dependency. This makes direct source ownership accurate but does
  not remove the library from the final link closure.
- `uint256.cpp` now formats the existing reversed-byte hex representation
  directly with standard-library code. A CYBOU test covers exact round-trip
  output. `cybou_base` no longer links `bitcoin_util`; inherited utility
  helpers remain in the shared `uint256.h` for compile-time and serialization
  interfaces, and other targets still require Bitcoin utility code.
- CYBOU secret-cleansing call sites now use
  `cybou/crypto/cleanse.h`, backed by OpenSSL `OPENSSL_cleanse`; direct
  `support/cleanse.h` use has been removed from `src/cybou`.
- CYBOU SHA-256 call sites now use `cybou/crypto/sha256.h`, backed by OpenSSL
  EVP. A new test checks the NIST empty and `abc` vectors and compares EVP
  output byte-for-byte with the inherited implementation across SHA-256
  padding boundaries and multi-chunk input. The inherited API remains in the
  test as a compatibility oracle. BIP-39 recovery phrase checksums also use
  this wrapper and retain the canonical zero-entropy phrase vector. All
  domain-separated protocol/state/identity SHA-256 call sites now use the
  same interface through a nonthrowing multipart helper; EVP digest calls are
  confined to the wrapper, and multipart output is checked against the `abc`
  vector.
- CYBOU HKDF-SHA256 derivations now share `cybou/crypto/hkdf_sha256.h`, backed
  by OpenSSL EVP_KDF. Identity key derivation, the local mail-key derivation,
  and the existing prototype Mail payload use this wrapper without changing
  their salt/info bytes. Tests include RFC 5869 test case 1, compare the Mail
  derivation byte-for-byte with the inherited HKDF implementation, and check
  the RFC output-length bound. The inherited HKDF implementation remains only
  as a test oracle.
- The Mail prototype's ChaCha20-Poly1305 calls now use
  `cybou/crypto/chacha20_poly1305.h`, backed by OpenSSL EVP. The helper preserves
  RFC 8439 ciphertext-plus-tag layout and rejects malformed buffer sizes; failed
  authentication clears the plaintext output. Tests check the RFC 8439 vector,
  byte-for-byte ciphertext/tag parity with the inherited implementation, bad
  tags, malformed lengths, and zero plaintext after authentication failure.
  The inherited AEAD API remains only as a test oracle; the prototype's X25519-
  only Mail profile remains disabled and is not the target PQ Mail suite.
- The GCS implementation now owns its CompactSize count encoding, bitstream,
  and Golomb-Rice codec instead of calling inherited stream/codec helpers.
  Compatibility tests compare complete filters against Bitcoin's GCS filter
  for empty and varied-length elements at counts 0, 1, 2, 17, 252, and 253;
  malformed and oversized CompactSize counts are rejected. SipHash remains
  inherited and unchanged. FastRange64 now lives in `cybou/fast_range.h`;
  deterministic boundary and sample tests compare its output against the
  inherited helper, and GCS no longer includes Bitcoin's FastRange header.
- SipHash remains on the inherited `CSipHasher` implementation in the GCS
  filter. A local MinGW benchmark on OpenSSL 3.5.2 compared 32-byte elements:
  `CSipHasher` measured 26.11 ns/hash and `EVP_MAC` SipHash measured
  54.78 ns/hash (one context reused, with per-message reinitialization and an
  explicit 8-byte output size). The EVP result matched `CSipHasher` for the
  benchmark vector; the existing Mail GCS test also checks encoded-filter
  parity against the inherited block-filter implementation. The roughly 2.1x
  slowdown is not acceptable for this hot path, so no SipHash migration was
  made. Revisit only with a faster provider/API path or a different approved
  performance constraint; do not replace it with a CYBOU-authored primitive.
- CYBOU secret-cleansing call sites now route through
  `cybou/crypto/cleanse.h`; direct `OPENSSL_cleanse` use is confined to the
  approved crypto wrappers. `CRYPTO_memcmp` remains for constant-time vault
  payload comparison.

The root `CMakeLists.txt` requires OpenSSL 3.5. The active PQ baseline in
`09_CRYPTO_PQ.md` names X25519 + ML-KEM-768 as the Mail target and explicitly
prohibits a custom hybrid KEM combiner. Do not change the target to ML-KEM-1024
or require OpenSSL 3.6 without a separate compatibility and provider review.

Mail confidentiality is not production-ready. `CybouMailService::SendMail`
fails closed because recipient encryption keys are not published in verified
identity state. The X25519-only helper in `mail_service.cpp` is marked as a
prototype; it converts an Ed25519 public key and must not be promoted to the
production path. See `16_MAIL_PROTOCOL.md` and `49_EMAIL_E2EE_HPKE_PQ.md`.

## Required order

1. Extract consensus-neutral byte, serialization, and hash-value types into a
   CYBOU-owned base target. Preserve all existing serialized bytes and hash
   interpretation. Remove `cybou_core -> bitcoin_consensus` only when no
   inherited consensus symbols remain in its interface or object files.
2. Introduce a small CYBOU crypto API backed by OpenSSL EVP/provider APIs.
   Keep provider selection and failure behavior explicit; do not expose OpenSSL
   types throughout protocol APIs.
3. Migrate SHA-256 first with byte-for-byte compatibility vectors for existing
   operation IDs, block IDs, state roots, network IDs, validator commitments,
   name commitments, and Mail commitments. Keep SHA-256 as the consensus
   function; a provider change is not a hash-suite change.
4. SipHash benchmarked on the active Windows/OpenSSL build; retain inherited
   `CSipHasher` for now due to EVP_MAC's approximately 2.1x cost. Revisit only
   with a faster provider/API path, preserving exact output. Review provider
   policy and failure behavior for the crypto wrappers before widening their
   use.
5. Secret cleansing now routes through one CYBOU-owned interface. Verify there
   are no direct inherited crypto or cleanse includes under `src/cybou` outside
   the approved wrappers.
6. The direct `bitcoin_crypto` edge was moved from `cybou_core` to
   `cybou_node`, where GCS SipHash uses it. FastRange64 is CYBOU-owned, and
   `cybou_base` no longer links `bitcoin_util`. Other legacy link paths remain
   until `uint256` headers, node serialization, and GCS no longer need
   inherited utility or crypto symbols. Verify the final link graph for
   `cybou_core`, `cybou_node`, and the desktop executable.
7. Treat Mail KEM and CYP2 transport confidentiality as separate protocol
   projects. Mail waits for a finalized interoperable PQ/T HPKE/key-package
   profile and authenticated recipient-key publication. CYP2 TLS needs its own
   downgrade, identity-binding, and deployment review.

## Compatibility and release gates

- No consensus hash, identifier, commitment, serialization, or wire-suite
  change is part of the implementation-only provider migration.
- Do not invent a hybrid KEM combiner or silently accept classical-only Mail.
- Keep the existing Ed25519 + ML-DSA signing profiles required by the active
  implementation authority. Any stronger profile is a separate protocol
  decision, not a local key-size adjustment.
- Build and run the relevant crypto, consensus, vault, and Mail tests before
  deployment. For core or `cybou-node` changes, follow the DEV deployment and
  rollback procedure in `AGENTS.md`.
