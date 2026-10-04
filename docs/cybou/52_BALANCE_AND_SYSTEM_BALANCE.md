# 52 — Balance and System Balance

Every AccountState holds two values:

```text
Balance         transferable CYBOU
System Balance  non-transferable CYBOU, pays protocol services
```

There is no AUTH or other non-CYBOU account unit (DEC-284).

CYBOU is one indivisible native asset:

```text
1 CYBOU = minimum unit
decimals = 0
genesis monetary base = 100,000,000,000, conserved (no mint, no burn)
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

SystemLock:

```text
Balance        -= X
System Balance += X
```

## Storage economy (implemented in M5; active from the M7 genesis)

`MAX_SUPPLY` is removed: the genesis monetary base
(100,000,000,000 CYBOU) belongs entirely to the `cybou.cybou` Treasury and is
conserved forever; no mint and no burn. New Identities receive a 20,000 CYBOU
System Balance start budget from Treasury. System Balance also funds storage rent
into StorageEscrow; lease refunds return to System Balance, never to Balance.
Provider payouts credit Balance only for the SystemLock-origin share; the
onboarding-origin share credits provider System Balance (DEC-281). The wallet
shows Balance, System Balance, Storage Escrow and, separately, non-canonical
pending provider earnings.
