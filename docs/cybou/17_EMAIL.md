# 17 — CYBOU Email

Implementation status: native bounded Mail operations, deterministic fees,
compact discovery filters, and evidence verification exist in core. The
complete encrypted send/receive lifecycle below and local Inbox/Sent indexes
are not operational yet. The Qt Email page is capability-gated.

Product scope: the initial DEV/Alpha profile below remains text-only. Beta Mail
requires encrypted Store-backed attachments and is not complete until the
Storage, manifest, retrieval, and durability path passes `81_BETA_PRODUCT_SCOPE.md`.

CYBOU Email is native E2E encrypted, signed, consensus-registered email.

It is not a realtime messenger and not an SMTP/IMAP mailbox service.

## Initial DEV/Alpha profile

```text
text only
one recipient per MailTx
no attachments
no SMTP/IMAP/POP3
recipient may be offline
```

## Send lifecycle

```text
compose
-> canonicalize protected text mail
-> generate random salt/content commitment
-> E2E encrypt
-> sign
-> first-class MailTx
-> normal CYBOU P2P propagation
-> BFT finality
```

## Receive lifecycle

```text
recipient client syncs headers/filters/history
-> discovers relevant MailTx
-> obtains ciphertext
-> verifies inclusion/finality/signature
-> decrypts locally
-> stores local mailbox index
```

There is no permanent consensus-state MailMarker per email.

## Sent semantics

`Sent` means the MailTx reached CYBOU finality.

It does not mean the recipient opened or legally accepted the contents.

## Offline behavior

Recipient online presence is not required.

Before Object Storage, validator archival retention preserves historical encrypted MailTx bodies during the controlled deployment phase.

## Initial profile large-content boundary

MailTx has:

```text
strict maximum serialized size
size-aware deterministic integer fee
```

Exact byte tiers are benchmarked before freeze.

Attachments remain disabled in the initial profile until Store is operational.
This does not make text-only mail sufficient for Beta.

## Beta Store boundary

```text
Blockchain:
    MailTx registration / commitment / finality

Store:
    opaque encrypted message/attachment objects and manifest

MailTx:
    commitment and opaque object reference; never attachment bytes

Client:
    decrypted local mailbox
```

## Evidence

CYBOU can export a `MailEvidenceBundle` proving inclusion, finality and historical sender-key authorization.

This is cryptographic evidence, not automatically a legal notarial act.
