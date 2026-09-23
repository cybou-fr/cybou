# 53 — Proof of Trust and network limits

CYBOU uses one global Proof of Trust (PoT) state per AccountID.

## Consensus inputs

## Current Beta score

Implemented in `src/cybou/state.{h,cpp}`:

```text
PoT = BaseScore + Capped(Age)
```

Pure integer arithmetic, bounded in `[100, 200]`:

1. **BaseScore = 100**: Granted to every legitimately onboarded AccountID that satisfied protocol anti-Sybil work.
2. **Account Age contribution**:
   $$E_{\text{age}} = \max(0, \text{current\_epoch} - \text{creation\_epoch})$$
   $$C_{\text{age}} = \min(E_{\text{age}} \times 5, 100)$$
   5 points per PoT epoch of existence, capped at 100 points (reached after 20 epochs).

## Epoch derivation

Consensus PoT uses protocol epochs derived purely from finalized chain height:

```text
current_epoch = block_height / params.epoch_blocks
```

Never use local wall-clock time, timezone, or client timestamps for consensus limits.

## Mail rate limit

Outgoing `MailTx` rate limit per protocol epoch:

```text
params.new_account_mail_limit_per_epoch = 25 (DEV default)
```

The current Beta policy has one network-bound limit. PoT and SystemBalance do
not increase it. An account tracks `last_mail_epoch` and
`mail_count_in_epoch` in its consensus `AccountState`. When `current_epoch >
last_mail_epoch`, the counter resets to zero. Exceeding the limit in the same
epoch rejects the operation with `RATE_LIMIT_EXCEEDED` without deducting fees.

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
