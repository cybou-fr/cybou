# 68 — Operator key separation

Commercial ownership does not justify a shared master key. Production roles
remain cryptographically separate:

| Role | Responsibility |
|---|---|
| PoA finalizer | Signs the canonical next block under the active Authority key ($K_{\text{epoch}}$) |
| Release Signing | Authenticates official software releases and update artifacts |
| Treasury | Controls company-owned reserve funds |
| User Identity | End-user account recovery and operations |

## Key boundaries

- **PoA finalizer**: Derived from the Central Authority operator Identity's recovery entropy for its dedicated `POA_FINALIZER` role. Operates locally on the operator desktop and is never sent to bootstrap or peers. Blocks require both Ed25519 and ML-DSA-65 signatures.
- **Release Signing**: Kept on isolated build/release signing infrastructure.
- **Treasury**: Stored in a distinct, dedicated vault holding company assets.
- **User Identity**: Managed exclusively by each end-user in their local CVID5 vault.

Bootstrap is rendezvous infrastructure and holds no consensus or Identity keys.
There is no Operator Authority key, validator registry, or operator-signed ordinary user onboarding.
