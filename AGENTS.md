# AGENTS.md — CYBOU v0.14 implementation authority

Read active docs before coding.

## Hard rules

### Mail
- MailTx is a first-class CYBOU operation.
- Do not encode mail as OP_RETURN/arbitrary Bitcoin Script payload.
- v1 is one-recipient, text-only, no attachments.
- no permanent per-mail consensus-state object.
- current state contains validation-relevant counters/roots only.
- local client owns Inbox/Sent/read-state indexes.
- strict maximum MailTx size required.
- deterministic size-aware fee required.
- priority fee disabled.

### Pre-Store
- desktop nodes may prune;
- active validators retain required canonical pre-Store MailTx history;
- mass-scale Email waits for Object Storage.

### BFT
- BFT explicit finality;
- operator-approved admission;
- equal validator weight = 1;
- 4 validators minimum when claiming f=1 tolerance.

### PoT
- block-height-derived deterministic epoch;
- integer arithmetic only;
- System Balance trust contribution capped;
- no local wall-clock consensus logic.

### Invite
- Welcome Grant requires one-time Operator-signed Invite Voucher;
- grant = 6,000 CYBOU;
- source = OnboardingPool;
- destination = SystemBalance;
- identity creation alone gives no grant.

### Operator keys
Keep separate:
- Operator Authority;
- Operator Validator;
- Release Signing;
- Treasury.

### Evidence
Mail evidence must support:
- transaction inclusion proof;
- BFT finality certificate;
- historical sender-key authorization;
- salted/domain-separated content commitment.

### Economics
```text
MAX_SUPPLY = 100,000,000,000
decimals = 0
4 fees -> 3 Security + 1 Onboarding
```
