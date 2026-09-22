# 16 — Native Mail Protocol

CYBOU Email is a chain-native protocol, not an overlay messenger.

## First-class operation

```text
MailTx
```

is a typed CYBOU protocol operation.

Do not encode it as OP_RETURN or arbitrary Bitcoin Script application data.

## v1 pipeline

```text
Protected text mail
-> random salt + content commitment
-> CEK + AEAD encryption
-> HPKE/PQ recipient key encapsulation
-> sender authentication
-> canonical MailTx serialization
-> fee/size validation
-> P2P propagation
-> BFT finality
```

## Block/history vs state

```text
block/history:
    full encrypted MailTx

current state:
    only counters/authority/balance/PoT data
    required to validate future operations

local client:
    mailbox index
```

There is no permanent per-mail consensus-state record.

## Discovery

Recipient discovery uses a privacy-reviewed tag and a compact block/range filter or equivalent mechanism.

Exact design is still open.

## v1 limitations

```text
one recipient
text only
bounded size
no attachment
no SMTP
no realtime chat
```

## Future Store

Later MailTx commits to encrypted Store objects instead of embedding large mail bodies.
