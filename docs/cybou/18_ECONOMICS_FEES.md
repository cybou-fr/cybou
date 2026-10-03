# CYBOU economics and fees

## Native asset

```text
CYBOU
decimals = 0
MAX_SUPPLY = 100,000,000,000
```

There is no perpetual base emission.

## Balance

Spendable, transferable CYBOU controlled by the Identity.

## System Balance

Irreversible service budget.

A user may voluntarily lock:

```text
Balance -> System Balance
```

There is no user-initiated unlock from System Balance back to Balance.

Protocol service fees are paid from System Balance.

## OnboardingPool

DEV genesis funds OnboardingPool with exactly 100,000,000 CYBOU.
AccountCreate transfers the configured onboarding bonus:

```text
OnboardingPool -> new Identity System Balance
```

The DEV bonus remains 6,000 CYBOU. The pool only decreases; fees never replenish
it. AccountCreate does not mint CYBOU and earns no AUTH (a genesis allocation
may grant initial AUTH).

## Protocol fees

100% of every finalized protocol fee transfers atomically from the payer's
System Balance to the Central Authority's spendable Balance. The recipient is
the unique genesis allocation labelled `cybou`, granting the reserved
`cybou.cybou` Identity. No separate fee wallet, recipient parameter or key exists.
This economic role grants no finalization power: PoA remains authorized solely
by the genesis PoA public key.

Before claim, fees accumulate in that allocation's Balance. AccountCreate
claims the full accumulated Balance once. After claim, fees credit the claimant's
ordinary AccountState Balance; the allocation Balance no longer changes.

Missing or duplicate allocations, invalid claimants and recipient overflow
reject the operation without changing balances or authorization nonce. There
is no batching, fee burn, priority bidding, validator reward or provider reward.

Payment uses the existing configured payment_fee. RootPublication uses:

```text
4 * ceil(full canonical operation bytes / 1024) + 4 * chunk_count
```

Each paid operation performs its own transfer. Block execution checks that
TotalSupply is unchanged; it performs no later fee distribution.

## Supply and AUTH

TotalSupply counts OnboardingPool, unclaimed genesis allocation Balances,
account Balances and account System Balances. The 100 billion cap is not a
promise that all units are issued: the proposed DEV genesis issues 200 million
(100 million onboarding and 100 million Central Authority), leaving 99.8 billion
unissued. There are no virtual reserves for that remainder.

AUTH is non-transferable, excluded from CYBOU supply, and changes through the
finalized rules in `57_IDENTITY_AUTHORITY.md`. Fee amounts do not scale AUTH.

## Network cutover

State has no v11 decoder. A new genesis requires a new Network Public Key
and therefore a new NetworkID, followed by a clean network-bound state reset.
Current compiled DEVNET has its new NetworkID and immutable genesis; the
former DEVNET is retired. MAINNET remains unprovisioned. Development uses DEVNET. Existing genesis is never re-signed or replaced.
