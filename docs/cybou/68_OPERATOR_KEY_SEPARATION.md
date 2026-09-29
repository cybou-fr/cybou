# 68 — Operator key separation

Commercial ownership does not justify a shared master key. Production roles
remain cryptographically separate:

| Role | Authority |
|---|---|
| PoA finalizer | Signs the next canonical block under the genesis-bound key |
| Release Signing | Authenticates software and update artifacts |
| Treasury | Controls company-owned CYBOU funds |
| Identity recovery/authorization | User-owned account recovery and operations |

The current genesis definition contains the PoA finalizer public key. There is
no Operator Authority key, validator set, validator admission/removal, or
operator-signed ordinary-user onboarding in the active protocol.

The PoA secret is derived from its dedicated operator recovery phrase, held
only in memory, and cleansed at session end. Before signing, the finalizer
checks and durably journals the exact next-height block intent. Journal
conflict, rollback, or valid equivocation halts signing and requires explicit
operator investigation.

Every production signature follows the key-role policy and requires both
Ed25519 and ML-DSA-65 for PoA finality. No classical-only fallback is allowed.
