# Post-quantum cryptography profile

Identity key roles are derived from the 24-word recovery entropy with separate domain-separated derivation labels. Recovery uses Ed25519 + ML-DSA-65. Account authorization uses Ed25519 + ML-DSA-44. Recipient key agreement uses a separate X-Wing seed (ML-KEM-768 + X25519). Signing and KEM keys are never reused across roles.

The DEV X-Wing publication profile pins draft-05 and is bound to AccountID and key_epoch by the finalized Identity state commitment. Beta and Mainnet remain disabled until separately approved. Mail must remain fail-closed until the vetted HPKE backend, exact envelope transcript, evidence verification, and desktop send/receive flow are integrated.

No protocol-level device identity exists. An Identity record has one current Recovery key, one current Authorization key, one current KEM package commitment, one account-wide nonce, and one key_epoch. IdentityRotate atomically replaces all public roles and the package commitment. It requires the old Recovery signature and new Recovery and Authorization proofs of possession.

| Role | Algorithm | Purpose |
| --- | --- | --- |
| Recovery | Ed25519 + ML-DSA-65 | Restore identity and authorize full key rotation |
| Authorization | Ed25519 + ML-DSA-44 | Sign account-level service operations |
| Recipient KEM | X-Wing (ML-KEM-768 + X25519) | Establish or wrap content keys; separate from signing |
| Validator/operator/release/treasury | Hybrid PQ policy | Separate key custody and domain-specific authorization |

Use standard cryptographic libraries and pinned vectors. No classical-only production fallback and no custom cryptographic primitives.
