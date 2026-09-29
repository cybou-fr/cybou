# 52 — Balance, System Balance and Authority

CYBOU has one indivisible native asset:

```text
1 CYBOU = minimum unit
decimals = 0
MAX_SUPPLY = 100,000,000,000
```

## Balance

User-controlled spendable CYBOU.

It may be received, transferred or voluntarily locked into System Balance.

## System Balance

Irreversible protocol-service budget.

```text
Balance -> System Balance
```

is allowed under the protocol.

```text
System Balance -> Balance
```

is not.

System Balance pays deterministic protocol service fees.

## Identity Authority

Authority is not CYBOU and not a third monetary balance.

The current System Balance amount does not continuously create Authority.

A voluntary finalized user-authorized lock:

```text
Balance -> System Balance
```

may grant a one-time SystemContributionAuthority equal to the locked whole
CYBOU amount.

Automatic onboarding credit:

```text
OnboardingPool -> System Balance
```

grants no SystemContributionAuthority.

Spending System Balance on fees does not grant the contribution again.

Authority is non-transferable and never grants PoA finalization power.
