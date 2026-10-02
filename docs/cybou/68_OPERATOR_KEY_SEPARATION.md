# 68 — Operator key separation

Commercial ownership does not justify a shared master key. Production roles
remain cryptographically separate:

| Role | Responsibility |
|---|---|
| Network Root R | Signs official network bindings and Authority assignments; never blocks |
| PoA finalizer P | Signs the canonical next block under the assignment active at its height |
| Release Signing | Authenticates official software releases and update artifacts |
| Treasury | Controls company-owned reserve funds |
| User Identity | End-user account recovery and operations |

## Key boundaries

- **Network Root**: Separate purpose and key material from the routine Central Authority runtime. DEV may use operational custody; Mainnet should use offline custody. Both signature components are required for root-signed bindings and assignments.
- **PoA finalizer**: The current root-authorized operational key on the Central Authority desktop. It is never sent to bootstrap or peers. Blocks require both Ed25519 and ML-DSA-65 signatures. Compromising P does not confer power to change R, the network generation or the next assignment.
- **Release Signing**: Kept on isolated build/release signing infrastructure.
- **Treasury**: Stored in a distinct, dedicated vault holding company assets.
- **User Identity**: Managed exclusively by each end-user in their local CVID5 vault.

Bootstrap is rendezvous infrastructure and holds no consensus or Identity keys.
There is no Operator Authority key, validator registry, or operator-signed ordinary user onboarding.
