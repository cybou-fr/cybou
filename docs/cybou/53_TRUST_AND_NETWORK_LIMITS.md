# 53 — Proof of Trust and network limits

CYBOU uses one global Proof of Trust (PoT) state per AccountID.

## Consensus inputs

Conceptually:

```text
PoT =
    capped(System Balance contribution)
  + account age contribution
  + clean-history contribution
  + valid-activity contribution
  - protocol penalties
```

All calculations are deterministic, integer-only and bounded.

## Time

Consensus PoT uses protocol epochs derived from finalized chain height.

Never use local wall-clock time for a consensus limit.

## System Balance

System Balance contributes to PoT but must be capped.

A user who consumes network services should not suffer unbounded trust collapse merely because fees reduce System Balance.

Over time, account age and clean history become larger parts of PoT.

## Mail

New valid invited AccountID:

```text
25 outgoing MailTx / PoT epoch
```

The UI may describe this as approximately daily if the configured epoch targets one day.

Higher PoT may raise the limit to bounded tiers.

## Payments

PoT may constrain:

```text
outgoing transaction count / epoch
outgoing CYBOU value / epoch
new payment recipients / epoch
```

PoT never confiscates Balance.

## Infrastructure authority separation

```text
PoT != validator admission
PoT != validator voting power
```

Operator approval controls validator admission in the current model.
