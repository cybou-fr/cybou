# Cryptographic baseline

CYBOU uses a hybrid post-quantum profile across protocol authorization and confidentiality. There is no classical-only production fallback. Do not invent primitives, combiners, or unauthenticated suite negotiation.

## Signing

- Recovery Root: Ed25519 **and** ML-DSA-65.
- Device operations: Ed25519 **and** ML-DSA-44.
- Validator votes, operator actions, release signing, and treasury actions: separate keys and domains under the PQ key policy. Their integration into the running node remains work in progress.
- Both components must verify over the same canonical, domain-separated message. A missing or failed component rejects the operation.

Key purpose, suite identifier, NetworkID, operation kind, account or validator identity, nonce, and canonical payload commitment must be bound wherever applicable. Reusing a key across domains is prohibited.

## Mail confidentiality

Mail signing keys are separate from encryption keys. Identity publishes
authorized recipient-device encryption capabilities; Mail does not maintain a
parallel recipient-key registry. The target device key-agreement profile is
X25519 + ML-KEM-768, subject to an interoperable standardized hybrid profile.
A recipient that requires the hybrid suite cannot be downgraded to
classical-only encryption. Plaintext and key material must never enter
consensus state.

## Identity capability key domains

| Purpose | Target keys | Status |
|---|---|---|
| Recovery Root authorization | Ed25519 + ML-DSA-65 | Protocol/vault target; implemented in the current identity path |
| Device authorization | Ed25519 + ML-DSA-44 | Implemented for current device authorization |
| Device key agreement / wrapping | X25519 + ML-KEM-768 | Target; not published in the identity record yet |
| Validator authorization | Ed25519 + ML-DSA-65 | Policy target; production consensus wiring remains incomplete |

Hybrid signatures protect authorization. Hybrid KEM protects key establishment
and wrapping. A standard symmetric AEAD protects bulk content. Device signing
keys must never be converted into or reused as Mail or Storage encryption keys.

Use distinct, authenticated contexts for these purposes:

```text
CYBOU/DEVICE/SIGN
CYBOU/MAIL/KEM
CYBOU/STORAGE/KEYWRAP
CYBOU/STORAGE/OBJECT
```

These are design-domain labels, not a custom cryptographic construction. Exact
transcript encoding, suite identifiers, hybrid combination, and AEAD parameters
remain subject to the relevant protocol freeze and test vectors.

## Implementation gate

The running BFT and desktop paths must be moved to this baseline before the DEV reset. Benchmarks, provider validation, fixed test vectors, canonical serialization, and malformed-input tests are required before activation.
