# 35 — Roadmap change: Email first

This document records the current architectural interpretation of "Email first."

## Superseded interpretation

Earlier designs treated Email as:

```text
Messaging Core
-> relay/rendezvous
-> Mailbox Store
```

That architecture is superseded.

## Current interpretation

```text
Mail Protocol
-> MailTx
-> normal CYBOU P2P
-> BFT finality
-> historical block inclusion + bounded validation state
```

Before Object Storage, MailTx contains encrypted text ciphertext.

After Object Storage:

```text
MailTx -> content commitment
Store  -> encrypted body + attachments
```

## Consequences

- no Email-specific relay node;
- no Mailbox Store;
- no service-node reward policy before Store;
- recipient does not need to be online;
- text-only first release;
- chain/state is the registration/notarial-like proof layer;
- local client is the readable mailbox.
