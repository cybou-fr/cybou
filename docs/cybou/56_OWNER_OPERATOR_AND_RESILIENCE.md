# 56 — Owner, operator, and network trust

CYBOU is commercially owned and operated by CYBOU. User Identity, balances,
names, and encrypted content remain protocol-owned under their respective
authorization rules. Commercial ownership does not confer access to user
private keys, plaintext content, or arbitrary account debits.

## Initial network operation

The target uses one genesis-bound hybrid-PQ PoA finalizer operated by the
Central Authority Identity from its desktop. The operator orders blocks and
can censor transactions or stop progress. This is explicit centralized trust,
not BFT. Full nodes independently verify finality signatures and deterministic
state transitions, but cannot make the network progress while that desktop is
offline. Bootstrap is an optional capability granted by genesis to one through
four ordinary full-node Identities. Bootstrap nodes may relay and cache but
never finalize by virtue of that capability. Any full node may independently
run storage or advisory Validation capabilities under the relevant local
policy.

The PoA finalizer key, Identity recovery/authorization keys, Release Signing
key, and Treasury key are separate roles. The Central Authority Identity's
recovery entropy derives its role-specific PoA key; the key remains local to
the unlocked desktop and is protected by a durable anti-equivocation journal.
The current DEV deployment has not yet migrated away from its VPS finalizer;
see `04_NETWORK_BOOTSTRAP_AND_GENESIS.md`.

## Resilience boundary

Mail and Files availability depends on independent storage providers, but
provider and bootstrap diversity do not decentralize block finality.
Operational reporting must state these trust domains separately. Any future change to block ordering
authority is a new protocol design based on measured operating needs; no BFT,
validator admission, staking, or validator-set state is part of the current
architecture.
