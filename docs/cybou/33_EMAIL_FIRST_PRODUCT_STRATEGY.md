# 33 — Email-first product strategy

CYBOU's first user product is native Email, but it must not be implemented as a messenger with an email-shaped UI.

## Product kernel

```text
Identity
-> E2E Mail crypto
-> MailTx
-> BFT finality
-> bounded mail-validation state
-> local mailbox UI
```

## Why Email fits the chain/state model

- recipient can be offline;
- sender obtains a finalized registration record;
- recipient synchronizes later;
- normal P2P transaction/block propagation is sufficient;
- no dedicated central mailbox service is required;
- text-only v1 sharply reduces complexity.

## v1 boundary

Include:

```text
Inbox
Sent
Drafts
Archive
Threads
Compose/Reply/Forward
text-only
one recipient per MailTx
```

Exclude:

```text
realtime presence/chat
SMTP/IMAP/POP3
attachments
HTML active content
bulk mailing
Email Relay/Mailbox infrastructure
```

## Differentiator

The product is not "secure chat with an Inbox."

Its distinguishing value is:

```text
signed
E2E encrypted
consensus registered
finalized
cryptographically committed
```

native mail.
