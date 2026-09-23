# 53 — Proof of Trust and network limits

CYBOU uses one global Proof of Trust (PoT) state per AccountID.

## Consensus inputs

Conceptually:

```text
## Consensus formula (Frozen V1)

Implemented in `src/cybou/state.{h,cpp}`:

```text
PoT = BaseScore + Capped(SystemBalance) + Capped(Age)
```

Pure integer arithmetic, bounded in `[100, 250]`:

1. **BaseScore = 100**: Granted to every legitimately onboarded AccountID that satisfied protocol anti-Sybil work.
2. **Capped System Balance contribution**:
   $$C_{\text{sb}} = \min\left(\frac{\text{SystemBalance}}{100}, 50\right)$$
   1 point per 100 CYBOU in SystemBalance, capped at 50 points (reached at 5,000 CYBOU).
3. **Account Age contribution**:
   $$E_{\text{age}} = \max(0, \text{current\_epoch} - \text{creation\_epoch})$$
   $$C_{\text{age}} = \min(E_{\text{age}} \times 5, 100)$$
   5 points per PoT epoch of existence, capped at 100 points (reached after 20 epochs).

## Epoch derivation

Consensus PoT uses protocol epochs derived purely from finalized chain height:

```text
current_epoch = block_height / params.epoch_blocks
```

Never use local wall-clock time, timezone, or client timestamps for consensus limits.

## Mail rate limit tiers

Outgoing `MailTx` rate limits per PoT epoch:

```text
PoT < 150:       25 MailTx / epoch  (Baseline: new account)
150 <= PoT < 200: 50 MailTx / epoch  (Tier 2: funded or mature)
PoT >= 200:      100 MailTx / epoch (Tier 3: long-standing clean account)
```

An account tracks `last_mail_epoch` and `mail_count_in_epoch` in its consensus `AccountState`. When `current_epoch > last_mail_epoch`, the counter automatically resets to zero. Exceeding the tier limit in the same epoch strictly rejects the transaction with `RATE_LIMIT_EXCEEDED` without deducting fees.

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
