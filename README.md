# CYBOU

CYBOU is a commercially operated European sovereign peer-to-peer network designed in France.

The first product is **CYBOU Email**.

## Mail architecture v0.14

```text
compose text
-> salted content commitment
-> E2E encrypt
-> sender signature
-> first-class MailTx
-> normal CYBOU P2P
-> permissioned BFT finality
```

MailTx is historical block data.

There is **no permanent per-email consensus-state MailMarker**.

Recipient clients maintain their own mailbox index and use compact block/range discovery metadata.

## Pre-Store

```text
desktop full node:
    may prune

active validator:
    retains required historical MailTx bodies
```

This is a controlled-scale temporary rule.

Mass-scale Email is gated on Object Storage.

## Future Store

```text
chain = registration / commitment / finality
Store = encrypted body + attachments
client = decrypted mailbox
```

## BFT / authority

```text
operator-approved validator admission
+
BFT explicit finality
```

```text
1 validator = development
4 validators = minimum f=1 target
```

All v1 validators have equal weight.

## PoT

Consensus time uses finalized-height epochs, never client wall clock.

New invited identity:

```text
25 outgoing MailTx / PoT epoch
```

## Invite

```text
Operator-signed one-time Invite Voucher
-> 6,000 CYBOU System Balance
```

Identity creation alone does not receive a grant.

## Monetary baseline

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0

4 CYBOU fees
-> 3 Validators
-> 1 Onboarding
```

Priority fees are disabled in v1.
