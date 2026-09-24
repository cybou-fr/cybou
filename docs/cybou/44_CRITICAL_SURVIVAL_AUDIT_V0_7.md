# 44 — Critical survival audit — v0.0.1 architecture

The largest existential risks are cryptographic mistakes, unbounded history/state growth, unsafe BFT, metadata leakage, fork debt and premature product scope expansion.

## Critical risk: product scope expansion

CYBOU contains enough architectural surface to become several products:
Email, Wallet, Storage, Backup and Drive. Feature expansion before Identity
and Email pilot validation is a survival risk. Do not build a broad service
suite to compensate for an unfinished core product.

## Product core

```text
E2E text
-> signed first-class MailTx
-> BFT finality
-> historical block data
```

There is no permanent per-mail consensus-state record.

## Critical risk: blockchain/history growth

Before Store, encrypted email text is block data.

Enforce:

- strict maximum MailTx size;
- deterministic size-aware fee;
- PoT outgoing limits;
- explicit block-weight/resource limits;
- controlled-scale pre-Store deployments;
- validator pre-Store archival retention;
- no attachments.

Mass-scale Email is gated on Object Storage.

Pre-Store Mail is an intentionally bounded pilot architecture, not the
mass-scale storage architecture.

## Critical risk: current-state growth

Never add one permanent consensus-state object per email.

Current state must remain limited to validation-relevant balances, identity, authority, PoT, counters and pools.

## Critical risk: metadata

Replicated MailTx can expose relationship metadata even with encrypted content.

Recipient discovery tags, filters and sender representation require privacy review.

## Critical risk: archived ciphertext

Assume adversaries may retain MailTx ciphertext forever.

PQ/T confidentiality and authenticated key management are therefore important.

## Critical risk: operator authority

Operator approval controls validator admission, but operator keys must not become a universal master key.

Separate Operator Authority, Validator, Release and Treasury domains.

## Critical risk: "notary" language

Finality gives cryptographic registration evidence, not automatic legal notarization.

## Survival route

```text
quarantine
-> typed protocol operations
-> identity/operator authority
-> E2E MailTx
-> bounded state + pre-Store retention
-> Email Alpha
-> snapshot/bootstrap
-> 4-validator f=1 BFT target
-> French pilot
-> Object Storage
-> attachments/Backup
```

## Stop conditions

Pause feature growth if:

```text
plaintext mail reaches chain
per-mail consensus state grows forever
MailTx size/history is unbounded
recipient metadata leaks unnecessarily
HPKE/PQ construction is ad-hoc
recipient keys are unauthenticated
honest validators can finalize conflicting blocks
operator/admin can debit user Balance
operator/support requires plaintext
account creation anti-Sybil work bypassed
```

## Monetary invariant

```text
4 fee CYBOU
-> 3 Validators
-> 1 Onboarding
```

No default burn.
