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

System Balance cannot be transferred back to Balance.

Protocol service fees are paid from System Balance.

## Onboarding

A valid AccountCreate receives the network-configured onboarding amount:

```text
OnboardingPool -> System Balance
```

This does not mint new CYBOU.

Automatic onboarding credit leaves AUTH unchanged.

## AUTH economics

AUTH has issuance independent from CYBOU, is excluded from the 100 billion
CYBOU supply and is non-transferable. Finalized Identity activity (+1 AUTH per
finalized Identity-authorized operation) and explicit PoA `AUTH_GRANT` /
`AUTH_BURN` change AUTH; see [`57_IDENTITY_AUTHORITY.md`](57_IDENTITY_AUTHORITY.md).
Fees, Balance-to-System Balance locks and onboarding credit never change AUTH.

## RootPublication fee

Generic encrypted content uses deterministic RootPublication fees based on
canonical operation size and authorized chunk count.

The current DEV formula remains:

```text
4 * ceil(full operation bytes / 1024)
+
4 * chunk_count
```

## Fee routing

Every complete four fee units route:

```text
3 -> Security
1 -> Onboarding
```

No burn and no priority bidding.

Authority is not transferable or redeemable as CYBOU, and there are no
protocol-level validator or provider monetary reward distributions.

## Network separation

DEV, Beta, and Mainnet have separate genesis allocations and economic parameters.
Beta balances do not carry to Mainnet. Mainnet onboarding values remain unset
until integrated Mail and Files operating data is measured.
