# 68 — Operator key separation

Commercial ownership does not justify a shared master key. Production roles
remain cryptographically separate:

| Role | Responsibility |
|---|---|
| Network Private Key | Strictly offline root authority; signs genesis/re-genesis specifications |
| PoA finalizer P | Operates from Central Authority desktop; signs canonical next blocks |
| Release Signing | Authenticates official software releases and update artifacts |
| Treasury | Controls company-owned reserve funds |
| User Identity | End-user account recovery, operations, and provisional Validation |

## Key boundaries

- **Network Private Key**: Strictly offline ALWAYS, including on DEVNET. Never stored on bootstrap, never stored on PoA finalizer, never loaded into normal CYBOU node runtime. Signs genesis specifications with monotonic `genesis_generation`.
- **PoA finalizer**: The genesis-authorized operational key on the Central Authority desktop. It is never sent to bootstrap or peers. Blocks require both Ed25519 and ML-DSA-65 signatures. Compromising P does not confer power to forge a new genesis or change the Network Key.
- **Release Signing**: Kept on isolated build/release signing infrastructure.
- **Treasury**: Stored in a distinct, dedicated vault holding company assets.
- **User Identity**: Managed exclusively by each end-user in their local CVID5 vault.

Bootstrap is an ordinary CYBOU full peer and holds no consensus or network authority keys.
There is no Operator Authority key, validator registry, or operator-signed ordinary user onboarding.
