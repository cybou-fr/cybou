# Cryptographic baseline

CYBOU uses a hybrid post-quantum profile across protocol authorization and confidentiality. There is no classical-only production fallback. Do not invent primitives, combiners, or unauthenticated suite negotiation.

## Signing

- Recovery Root: Ed25519 **and** ML-DSA-65.
- Device operations: Ed25519 **and** ML-DSA-44.
- Validator votes, operator actions, release signing, and treasury actions: separate keys and domains under the PQ key policy. Their integration into the running node remains work in progress.
- Both components must verify over the same canonical, domain-separated message. A missing or failed component rejects the operation.

Key purpose, suite identifier, NetworkID, operation kind, account or validator identity, nonce, and canonical payload commitment must be bound wherever applicable. Reusing a key across domains is prohibited.

## Mail confidentiality

Mail signing keys are separate from encryption keys. The target recipient profile combines X25519 and ML-KEM-768 with authenticated suite choice and a standard AEAD. A recipient that requires the hybrid suite cannot be downgraded to classical-only encryption. Plaintext and key material must never enter consensus state.

## Implementation gate

The running BFT and desktop paths must be moved to this baseline before the DEV reset. Benchmarks, provider validation, fixed test vectors, canonical serialization, and malformed-input tests are required before activation.
