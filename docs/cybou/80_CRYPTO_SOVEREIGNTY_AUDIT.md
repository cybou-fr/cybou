# Crypto sovereignty audit

This audit records the repository state and implementation gates for reducing
CYBOU's dependency on inherited Bitcoin cryptography. It does not authorize a
consensus hash change or a new cryptographic protocol.

## Repository state

CYBOU sources still depend on inherited `uint256`, `CSipHasher`,
`CHKDF_HMAC_SHA256_L32`, `AEADChaCha20Poly1305`, and `memory_cleanse` APIs.
The Bitcoin cryptography dependency therefore remains; migrating SHA-256 does
not by itself remove it.

## Progress

- A `cybou_base` target now owns the `uint256.cpp` object, and
  `bitcoin_consensus` consumes that target instead of compiling a duplicate.
- `cybou_core` now links `cybou_base` directly and no longer links
  `bitcoin_consensus`.
- This is an initial target-boundary extraction only. The `uint256` header and
  implementation still use inherited Bitcoin utility APIs, and the target
  links `bitcoin_util`; this does not yet remove the `bitcoin_crypto` dependency.
- CYBOU secret-cleansing call sites now use
  `cybou/crypto/cleanse.h`, backed by OpenSSL `OPENSSL_cleanse`; direct
  `support/cleanse.h` use has been removed from `src/cybou`.
- CYBOU SHA-256 call sites now use `cybou/crypto/sha256.h`, backed by OpenSSL
  EVP. A new test checks the NIST empty and `abc` vectors and compares EVP
  output byte-for-byte with the inherited implementation across SHA-256
  padding boundaries and multi-chunk input. The inherited API remains in the
  test as a compatibility oracle; other inherited crypto migrations remain.

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
4. Migrate HKDF and AEAD with fixed vectors covering key, nonce, AAD, ciphertext,
   authentication failure, and malformed lengths. Benchmark SipHash before
   choosing EVP_MAC for the filter hot path; preserve its exact output.
5. Replace inherited secret-cleansing calls through one CYBOU-owned interface,
   then verify there are no direct inherited crypto or cleanse includes under
   `src/cybou` outside the approved wrapper.
6. Remove `bitcoin_crypto` from CYBOU link interfaces only after the remaining
   CYBOU sources and targets no longer require any of its symbols. Verify the
   final link graph for `cybou_core`, `cybou_node`, and the desktop executable.
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
