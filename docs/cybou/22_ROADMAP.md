# 22 — Roadmap v0.0.1 — Architecture hardening

## v0.0.0 — Exact upstream baseline
Pin exact local Bitcoin Core tag/commit. Build/tests only. No normal Bitcoin-network launch.

## v0.0.1 — CYBOU quarantine + rename
`cybou.exe`, separate datadir, own network identity, no Bitcoin peers/seeds/blocks/UI/URI.

## v0.0.2 — CYBOU dev genesis
One Operator Validator, own genesis, 2-node P2P/block propagation.

## v0.0.3 — AccountID + operator authority
- AccountID/device authorization;
- `.cybou` alias skeleton;
- separate Operator Authority / Validator / Release / Treasury key domains;
- Invite Voucher signature verification skeleton.

## v0.0.4 — Typed protocol-operation layer
Introduce explicit first-class CYBOU operations.

Do not encode Mail as OP_RETURN/application data inside Bitcoin semantics.

## v0.0.5 — E2E Mail crypto vertical slice
- canonical plaintext mail representation;
- random salt/content commitment;
- CEK + AEAD;
- HPKE/PQ recipient encapsulation;
- sender signature;
- historical key authorization test;
- one recipient;
- text only.

## v0.0.6 — MailTx Alpha
- typed MailTx;
- strict max size;
- deterministic integer size-aware fee;
- normal P2P propagation;
- BFT finality;
- local mailbox index;
- block/range recipient-discovery filter prototype;
- no permanent per-mail consensus state.

## v0.1.0 — Deterministic state + PoT epochs
- Balance;
- System Balance;
- identities;
- validator/operator authority state;
- fee pools;
- integer PoT;
- block-height-derived PoT epoch;
- 25 MailTx/epoch new-account baseline;
- one-time Invite Voucher redemption.

## v0.1.1 — Checkpoint / snapshot / pruning
- bounded state;
- desktop pruning;
- validator pre-Store archival requirement;
- fresh-node bootstrap;
- filter/header sync.

## v0.2.0 — Formal BFT
- 1 validator dev;
- 2–3 integration;
- 4 validators minimum for f=1 test target;
- equal validator weight;
- operator-approved admission/removal;
- rounds/locking/finality/restart simulator.

## v0.2.1 — CYBOU Email Beta
- Inbox/Sent/Drafts/Archive;
- threads;
- Reply/Forward;
- local read/unread;
- recipient-discovery privacy review;
- MailEvidenceBundle export;
- PQ/T crypto review;
- no attachments.

## v0.2.2 — Economics hardening
```text
MAX_SUPPLY = 100,000,000,000
decimals = 0
WELCOME_GRANT = 6,000
```

```text
4 fee CYBOU
-> 3 Security
-> 1 Onboarding
```

No priority fee.

## v0.2.3 — French pilot readiness
Security/legal/CRA/crypto-export/privacy/update/runbook work.

## v0.2.4 — French controlled pilot
20–100 users, small controlled scale, 4 approved validators where claiming f=1 BFT tolerance.

## v0.3.0 — Operator continuity
Design/test emergency Operator Authority succession without introducing normal DAO/community governance.

## v0.3.1 — European controlled expansion
Multiple EU validator operators/providers.

## v0.4.0 — Object Storage
Encrypted object storage becomes the mail content layer.

New MailTx carries content root/reference rather than large body ciphertext.

## v0.4.1 — Attachments
Only after Store.

## v0.4.2 — Erasure + repair

## v0.4.3 — Verified storage accounting + provider rewards

## v0.5 — Backup
## v0.6 — Drive
## v0.7 — Email expansion / optional gateway research
## v0.8 — Sovereignty exercise
## v0.9 — Global-readiness review
## v1.0 — Production candidate
