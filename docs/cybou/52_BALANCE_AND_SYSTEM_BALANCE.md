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

Authority is the third canonical account value, denominated in AUTH. It is
non-transferable and excluded from CYBOU supply. GenesisAllocation may assign
initial AUTH, claimed exactly once by AccountCreate; ordinary accounts start
with zero AUTH.

Moving CYBOU from Balance to System Balance, onboarding credit and spending
System Balance leave AUTH unchanged. Only an explicit future protocol state
transition may change AUTH. Finalized Authority above 1,000,000 AUTH enables
advisory Validation, never PoA finalization.
