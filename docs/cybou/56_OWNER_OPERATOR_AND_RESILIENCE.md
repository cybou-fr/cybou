# 56 — Owner, operator, and network trust

CYBOU is commercially owned and operated by CYBOU. User Identity, balances,
names, and encrypted content remain protocol-owned under their respective
authorization rules. Commercial ownership does not confer access to user
private keys, plaintext content, or arbitrary account debits.

## Initial network operation

The current target uses one genesis-bound hybrid-PQ PoA finalizer operated by
CYBOU. The operator orders blocks and can censor transactions or stop progress.
This is explicit centralized trust, not BFT. Full nodes independently verify
finality signatures and deterministic state transitions, but cannot make the
network progress without the operator.

The PoA finalizer key, Identity recovery/authorization keys, Release Signing
key, and Treasury key are separate roles. The finalizer's private material is
derived for the session, kept in memory, and protected by a durable
anti-equivocation journal.

## Resilience boundary

Mail and Files availability depends on independent storage providers, but
provider diversity does not decentralize block finality. Operational reporting
must state these trust domains separately. Any future change to block ordering
authority is a new protocol design based on measured operating needs; no BFT,
validator admission, staking, or validator-set state is part of the current
architecture.
