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

The Central Authority Identity's recovery entropy derives the distinct
`POA_FINALIZER` key role. The desktop holds and uses it only while that
Identity is unlocked; it is never sent to bootstrap. Before signing, the
finalizer checks and durably journals the exact next-height block intent.
Journal conflict, rollback, or valid equivocation halts signing and requires
explicit operator investigation. The current legacy DEV still has the
finalizer process on its VPS pending the coordinated cutover.

Every production signature follows the key-role policy and requires both
Ed25519 and ML-DSA-65 for PoA finality. No classical-only fallback is allowed.
