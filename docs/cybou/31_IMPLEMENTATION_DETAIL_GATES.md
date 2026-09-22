# 31 — Implementation detail gates

A — exact upstream Bitcoin baseline  
B — CYBOU network quarantine  
C — product/binary rename  
D — AccountID/device identity  
E — native E2E MailTx crypto vertical slice  
F — MailTx + bounded mail-validation state transition  
G — deterministic state roots  
H — checkpoint/snapshot bootstrap  
I — static/operator-approved BFT safety  
J — Proof of Trust + service limits  
K — fixed-supply integer economics  
L — validator rewards  
M — Object Storage later

## A — exact upstream baseline

Record exact local Bitcoin tag/commit before source migration.

## B — quarantine

No normal CYBOU build may reach Bitcoin networks or download Bitcoin blocks.

## E — MailTx crypto

Before UI polish:

```text
plaintext text
-> CEK/AEAD
-> HPKE key wrap
-> sender authentication
-> MailTx bytes
-> verify/decrypt test
```

No plaintext fallback.

## F — MailTx + bounded mail-validation state

Implement:

```text
MailTx in block/history data
NO permanent per-mail consensus-state object
local mailbox index
bounded validation counters/state only
```

Do not implement Email Relay/Mailbox infrastructure.

## H — bounded history

Fresh-node bootstrap must define how recent MailTx block data is obtained and what happens after pruning.

Block inclusion/finality proves the MailTx commitment, but no proof can reconstruct missing ciphertext bytes.

## I — BFT

Formal locking/round/finality tests are mandatory.

Validator admission is operator-approved in the current PoA model.

## J — Proof of Trust

Apply PoT to MailTx, payments and later resource-consuming services.

PoT never grants validator authority.

## K — integer CYBOU economics

```text
MAX_SUPPLY = 100,000,000,000
decimals = 0
WELCOME_GRANT = 6,000
```

Audit inherited Bitcoin amount/formatting/fee/dust assumptions.

## L — Fee Router / validator rewards

```text
4 fees
-> 3 Security
-> 1 Onboarding
```

Eligible validators:

```text
participation >= 90%
no confirmed equivocation
equal split
```

Carry integer remainder forward.

## M — Object Storage

Attachments remain disabled until Store passes its own encryption, proof, repair and accounting gates.
