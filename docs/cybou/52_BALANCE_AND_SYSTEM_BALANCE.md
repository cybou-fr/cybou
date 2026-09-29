# 52 — Balance and System Balance

CYBOU has one indivisible native asset:

```text
1 CYBOU = minimum unit
decimals = 0
MAX_SUPPLY = 100,000,000,000
```

Each account has two balances.

## Balance

Balance is user-controlled CYBOU. It can be received, transferred, or
voluntarily locked into System Balance. A debit requires the account's valid
Identity authorization. No operator or service can seize or arbitrarily debit
Balance.

## System Balance

System Balance is an account-funded service budget. It is not transferable,
tradeable, or convertible back to Balance. Generic RootPublication byte and
chunk fees are charged from it. It does not increase Proof of Trust score or
grant consensus authority. There are no separate free-credit pools for
individual services.

## Fee routing

Every four CYBOU of protocol fee value route as:

```text
4 -> 3 Security + 1 Onboarding
```

Fees use deterministic integer arithmetic. Priority fees are disabled. DEV,
Beta, and Mainnet parameters are independent; detailed rules are in
`18_ECONOMICS_FEES.md` and the active network definition.
