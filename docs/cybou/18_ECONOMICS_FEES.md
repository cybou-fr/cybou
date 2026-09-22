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

## Welcome Grant

```text
6,000 CYBOU
OnboardingPool -> System Balance
```

only after redemption of a valid one-time Operator-signed Invite Voucher.

Identity creation alone does not mint or automatically grant CYBOU.

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
