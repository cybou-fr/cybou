# Post-quantum cryptography profile

Status: CURRENT
Scope: KEM/application source review at 531dc0da; signing-role/vault source review at 75ba7969, 2026-10-09. Full standards/composition review remains pending; no conformity claim.

Applicable published standards take precedence under
[`SECURITY_STANDARDS.md`](SECURITY_STANDARDS.md). ML-KEM and ML-DSA primitive
references do not certify their hybrid compositions. The existing DEV X-Wing
profile is experimental: current-draft equivalence is unverified, and existing
vector tests cite earlier concrete-hybrid-KEM drafts. Preserve exact bytes
until the construction/vector review and any required transition are specified.

Identity key roles are derived from the 24-word recovery entropy with separate domain-separated derivation labels. Recovery uses Ed25519 + ML-DSA-65. Account authorization uses Ed25519 + ML-DSA-44. Recipient key agreement uses a separate X-Wing seed (ML-KEM-768 + X25519). Signing and KEM keys are never reused across roles.

The DEV X-Wing publication profile pins draft-05 and is bound to AccountID and key_epoch by the finalized Identity state commitment. Beta and Mainnet remain disabled until separately approved. The CYBOU capsule transcript and Mail/Files construction/scanning are implemented; see [RootPublication](ROOT_PUBLICATION.md) and [KEM evidence limits](89_IDENTITY_KEM_PUBLICATION.md). This source review does not validate the custom composition against HPKE or close clean desktop send/receive acceptance. Malformed ciphertext, wrong context and unavailable keys remain fail-closed.

No protocol-level device identity exists. An Identity record has one current Recovery key, one current Authorization key, one current KEM package commitment, one account-wide nonce, and one key_epoch. IdentityRotate atomically replaces all public roles and the package commitment. It requires the old Recovery signature and new Recovery and Authorization proofs of possession.

| Role | Algorithm | Purpose |
| --- | --- | --- |
| Recovery | Ed25519 + ML-DSA-65 | Restore identity and authorize full key rotation |
| Authorization | Ed25519 + ML-DSA-44 | Sign account-level service operations and storage payout bindings |
| Recipient KEM | X-Wing (ML-KEM-768 + X25519) | Establish or wrap content keys; separate from signing |
| Network Key | Ed25519 + ML-DSA-65 (Network Public Key = NetworkID) | Strictly offline creation-time root of trust; signs immutable genesis specification once |
| PoA Finalizer P | Ed25519 + ML-DSA-65 | Authorized in genesis; signs canonical block certificates |
| Release Signing | Separate release-policy target | Authenticate software/releases; this table does not claim an implemented Identity signing role |
| Treasury / Custody | Separate custody-policy target | No separate runtime Treasury signing role; ordinary account Authorization controls spendable funds |
| Storage Provider | Ed25519 + ML-DSA-44 | Prove provider identity per live CYBOU P2P session |

Use standard cryptographic libraries and pinned vectors. No classical-only production fallback and no custom cryptographic primitives.

Exact implemented [role values, sizes and derivation](86_IDENTITY_SECURITY_SUBSTRATE.md#current-signing-role-bytes)
and [CYBV/CYID formats](76_IDENTITY_VAULT_RECOVERY.md#current-portable-bytes)
are source-reviewed separately from adopted security standards and product acceptance.
