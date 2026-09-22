# 52 — Balance and System Balance

CYBOU has one native asset: `CYBOU`.

CYBOU is indivisible:

```text
1 CYBOU = minimum unit
decimals = 0
```

An account exposes two protocol balances:

```text
CYBOU Account
│
├── Balance
│
└── System Balance
```

## Balance

`Balance` is user-controlled CYBOU.

It can be received, transferred and voluntarily locked into System Balance.

### Absolute security invariant

A debit from `Balance` requires valid user/account authorization under the transaction rules.

There is no:

```text
adminDebit(account, amount)
operatorSeize(account, amount)
```

The CYBOU owner/operator, validators and protocol infrastructure operators cannot arbitrarily debit user Balance.

## System Balance

`System Balance` contains frozen CYBOU assigned to protocol use.

It receives CYBOU from:

```text
Invite Bonus
organization sponsorship
Balance -> System Balance
other explicit protocol grants
```

System Balance:

```text
cannot be transferred to another user
cannot be withdrawn back to Balance
cannot be traded directly
does not create validator voting power
```

It can be debited by deterministic protocol rules for:

```text
Email
payments / transfer processing
identity operations
future Storage
Backup
Drive
other CYBOU services
```

## One-way lock

```text
Balance
   |
   | LOCK_TO_SYSTEM
   v
System Balance
```

Irreversible.

## Invite Bonus

v0.10 baseline:

```text
WELCOME_GRANT = 6,000 CYBOU
```

A valid invited identity receives the entire amount immediately:

```text
Invite
-> +6,000 CYBOU System Balance
```

No vesting.

The same System Balance directly contributes to Proof of Trust.

There is no separate Invite Trust Credit.

## Why 6,000

Email baseline target:

```text
standard small text MailTx ~= 1 CYBOU
new-account hard send limit = 25 MailTx/day
normal-budget design point ~= 15 MailTx/day
```

```text
15 * 365 = 5,475 CYBOU/year
Welcome Grant = 6,000
margin = 525 CYBOU ~= 9.6%
```

The 25/day limit is a safety ceiling, not an assumption that every normal user sends at the ceiling every day.

A heavier user can lock additional CYBOU:

```text
Balance -> System Balance
```

## System debit invariant

"System may debit System Balance" means every full node independently verifies a protocol-defined operation and fee.

It does not mean the CYBOU operator can manually take System Balance.

## Zero System Balance

Zero System Balance does not destroy an AccountID or confiscate Balance.

Paid outgoing operations require replenishment first.
