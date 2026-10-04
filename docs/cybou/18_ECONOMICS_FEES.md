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

State has no legacy decoder. A new genesis requires a new Network Public Key
and therefore a new NetworkID, followed by a clean network-bound state reset.
Current compiled DEVNET has its new NetworkID and immutable genesis; the
former DEVNET is retired. MAINNET remains unprovisioned. Development uses DEVNET. Existing genesis is never re-signed or replaced.

## Storage-economy target (frozen, not implemented)

Applies only from the DEC-283 cutover to a new DEVNET NetworkID and genesis.

### Monetary base

```text
decimals = 0
genesis monetary base = 100,000,000,000 CYBOU in the cybou.cybou Treasury allocation
no MAX_SUPPLY, no OnboardingPool, no unissued supply, no mint, no burn
TotalCybou = unclaimed genesis Balances + Balances + System Balances + StorageEscrow
TotalCybou(parent) == TotalCybou(candidate); overflow -> invalid state
```

### Flows

```text
AccountCreate (PoW kept)  Treasury Balance -20,000  new System Balance +20,000
SystemLock                Balance -X                System Balance +X
protocol fee              payer System Balance -X   Treasury Balance +X
StorageLease              payer System Balance -X   StorageEscrow +X
StorageSettlement         StorageEscrow -X          provider Balance / System Balance +X
lease refund              StorageEscrow -X          payer System Balance +X
```

The Treasury claimant receives no onboarding bonus. Storage rent goes 100% to
providers; ordinary operation fees go 100% to Treasury. There is no stake,
slashing, auction or variable provider price.

### Rent

Beta rate: 5 CYBOU per GiB per day per remote replica; `replica_target = 2`,
so 10 CYBOU per logical GiB per day. Billing unit: 512 KiB per authorized chunk
(2048 units = 1 GiB). Cost numerator `units x seconds x replicas x 5`,
denominator `2048 x 86400`, integer only, with a carried remainder. The rate is
provisional until shadow accounting (M4) measures real cost.

### Onboarding cannot be laundered

System Balance records an onboarding-origin portion consumed first by debits.
Escrow keeps the origin split, and the onboarding-origin share of a payout
credits provider System Balance; only SystemLock-origin rent becomes
transferable Balance. Providers are assigned by the network from finalized
randomness, never by the payer, so self-dealing cannot target one's own node.

### Equilibrium is an estimate

A node offering 100 GiB of provider capacity at full demand earns about what
50 GiB of two-replica storage costs. This is never a guarantee: without demand
no foreign chunks are assigned.

### Legal gate

Transferable CYBOU earned for a service may fall under MiCA. Qualification is
an open legal gate before MAINNET (`25_OPEN_QUESTIONS.md`).
