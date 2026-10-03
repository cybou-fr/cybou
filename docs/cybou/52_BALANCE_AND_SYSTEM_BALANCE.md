# 52 — Balance, System Balance and AUTH

Every AccountState holds three values:

```text
Balance         transferable CYBOU
System Balance  non-transferable CYBOU, pays protocol services
AUTH            non-transferable separate unit, Validation eligibility
```

CYBOU is one indivisible native asset:

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

SystemLock:

```text
Balance        -= X
System Balance += X
AUTH           += 1   (flat finalized-operation reward, independent of X)
```

## AUTH

AUTH is not CYBOU and is excluded from CYBOU supply. Onboarding credit
leaves AUTH unchanged, and no CYBOU amount (locked, held or spent) scales AUTH. AUTH issuance and burn are defined in
[`57_IDENTITY_AUTHORITY.md`](57_IDENTITY_AUTHORITY.md). Finalized AUTH above
10,000,000 enables Validation signatures, never PoA finalization.
