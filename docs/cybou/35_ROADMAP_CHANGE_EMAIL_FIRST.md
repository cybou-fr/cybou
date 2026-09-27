# 35 — Roadmap change: Email first

Status: its sequencing of general Object Storage after Email v1 is superseded
for Beta by `81_BETA_PRODUCT_SCOPE.md` and DEC-173. Email remains the first
user-facing product, but complete Beta Mail requires Store-backed encrypted
attachments. The pre-Store text-only path is transitional DEV/Alpha scope.

This document preserves the original rationale for "Email first."

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

In the initial DEV/Alpha profile, MailTx may contain bounded encrypted text
ciphertext while pre-Store retention is available.

For Beta Mail:

```text
MailTx -> content commitment
Store  -> encrypted body + attachments
```

## Consequences

- no Email-specific relay node;
- no Mailbox Store;
- no service-node reward policy before Store;
- recipient does not need to be online;
- text-only initial DEV/Alpha profile;
- chain/state is the registration/notarial-like proof layer;
- local client is the readable mailbox.
