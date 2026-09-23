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
    contributes a capped component to PoT
```

## Onboarding Bonus

```text
Dev Onboarding Bonus: 6,000 CYBOU (Beta/Mainnet: TBD)
OnboardingPool -> System Balance
```

Granted automatically upon valid protocol-native `AccountCreateOpV1` with anti-Sybil work (`ACCOUNT_CREATION_WORK_V1`).

No operator vouchers or invites exist.

Identity creation alone does not mint new tokens; bonuses are debited strictly from the pre-allocated `OnboardingPool`.

## Mail fee

MailTx is a first-class protocol operation.

v1 fee structure:

```text
MailFee =
    fixed base
    + deterministic integer size tier
```

Exact byte thresholds remain open until real PQ/T MailTx serialization is measured.

There is a strict maximum MailTx size.

## No priority fee v1

```text
priority fee = disabled
fee bidding = disabled
```

v1 uses deterministic protocol fees.

## Fee Router

```text
4 CYBOU fees
-> 3 Validators / Security
-> 1 Onboarding
```

No burn.

No generic service-node reward before Store.

## Future Store

Storage provider economics are introduced only with Object Storage and only after storage work can be verified.
