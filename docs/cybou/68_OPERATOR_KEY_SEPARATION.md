# 68 — Operator key separation

Commercial ownership does not justify a shared master key. Production roles
remain cryptographically separate:

| Role | Responsibility |
|---|---|
| Network Private Key | Strictly offline creation-time root of trust; signs immutable genesis specification once |
| `cybou.cybou` PoA key role P | Ordinary Identity's distinct PoA role operates from Central Authority desktop; signs canonical next blocks |
| Release Signing | Authenticates official software releases and update artifacts |
| Treasury | Controls company-owned reserve funds |
| User Identity | End-user account recovery, operations, and Validation signatures |

## Key boundaries

- **Network Private Key**: Strictly offline ALWAYS, including on DEVNET. Never stored on bootstrap or the PoA finalizer, never loaded into normal CYBOU node runtime. Signs the immutable genesis specification once at network creation. The official client compiles that signed public genesis and its initial state.
- **PoA finalizer**: The genesis-authorized operational key on the Central Authority desktop. It is never sent to bootstrap or peers. Blocks require both Ed25519 and ML-DSA-65 signatures. Compromising P does not confer power to forge a new genesis or change the Network Key; it triggers equivocation safety halt requiring network cutover.
- **Release Signing**: Kept on isolated build/release signing infrastructure.
- **Treasury**: Stored in a distinct, dedicated vault holding company assets.
- **User Identity**: Managed exclusively by each end-user in their local CVID5 vault. `cybou.cybou` is an ordinary Identity with mnemonic, AccountID, Recovery, Authorization, KEM and Mail/support; its distinct PoA key role is authorized only by genesis. There is no separate PoA Identity entity.

Provisioning keeps Network and `cybou.cybou` private material only under
gitignored `/private/`; Git contains public keys, public Identity data and
signed genesis constants. MAINNET private material does not yet exist.

Bootstrap is an ordinary CYBOU full peer and holds no consensus or network authority keys.
There is no Operator Authority key, validator registry, or operator-signed ordinary user onboarding.

## Full Node signing boundaries

A Full Node never announces or authenticates a PoA network role. Possession
of the matching private key enables only the local block-production worker;
receivers verify the finalized certificate against genesis and execute the
block independently. Enabling or disabling the signer preserves peer sessions.
The same network lifecycle handles discovery, sync, operation relay and gossip.

Every Full Node has intrinsic quota-controlled encrypted storage. Its separate
STORAGE_PROVIDER key is proven only on demand for a storage relationship,
with both HELLOs, the TLS exporter and a fresh challenge. ProviderID identifies
replicas and confers no finality or Validation authority.
