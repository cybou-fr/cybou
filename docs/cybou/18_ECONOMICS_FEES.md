# 18 — CYBOU economics and fees

## Native asset

```text
CYBOU
decimals = 0
MAX_SUPPLY = 100,000,000,000
```

## Balances

```text
Balance
    user-controlled

System Balance
    irreversible protocol-use balance
    pays fees
    does not increase PoT score
```

## Onboarding Bonus

```text
Dev Onboarding Bonus: 6,000 CYBOU (Beta/Mainnet: TBD)
OnboardingPool -> System Balance
```

Granted automatically upon valid protocol-native `AccountCreateOp` with anti-Sybil work (`ACCOUNT_CREATION_WORK_V1`).

No operator vouchers or invites exist.

Identity creation alone does not mint new tokens; bonuses are debited strictly from the pre-allocated `OnboardingPool`.

## Publication fee

Content uses generic RootPublication. The fee includes operation size and
authorized chunk count:

```text
RootPublicationFee = 4 * ceil(full_canonical_operation_bytes / 1024) + 4 * chunk_count
```

Each four-unit increment is split 3 to Security and 1 to Onboarding. The
canonical wire profile has a strict maximum operation size. The same
resource accounting applies to every encrypted application schema; consensus
does not inspect Mail content or enforce Mail-specific quotas.

## No priority fee v1

```text
priority fee = disabled
fee bidding = disabled
```

v1 uses deterministic protocol fees.

## Fee Router

```text
4 CYBOU fee units
-> 3 Security
-> 1 Onboarding
```

No burn.

No generic service-node reward before provider obligations and operating costs
can be measured.

## Future Store

Storage-provider economics remain open until chunk durability and provider
work can be verified and measured.
