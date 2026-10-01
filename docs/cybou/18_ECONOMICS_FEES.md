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

Automatic onboarding credit does not generate SystemContributionAuthority.

## Identity Authority interaction

Authority is not money and is not a third balance.

A voluntary finalized user-authorized lock of `X` whole CYBOU:

```text
Balance -> System Balance
```

may give a one-time:

```text
SystemContributionAuthority += X
```

under the immutable Authority rules.

Holding System Balance does not continuously generate Authority. Spending fees
does not generate another Authority credit.

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
