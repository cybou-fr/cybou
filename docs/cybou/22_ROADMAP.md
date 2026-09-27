# 22 — Roadmap v0.0.1 — Architecture hardening

This is a milestone sequence, not a list of completed releases. As of
2026-09-27, parts of several milestones exist in core and the experimental
single-validator DEV node, while the complete Beta Email product, independent
multi-validator network, pruning, and Object Storage remain open. See
`26_IMPLEMENTATION_STATUS.md` for the current code boundary and
`75_DEV_NODE_RUNBOOK.md` for the running DEV process.

The first text-only Mail profile is transitional DEV/Alpha scope. Beta requires
Storage Core followed by PQ Mail with encrypted attachments and the Files
product surface. Backup remains a post-Beta application. There is no separate
Drive product milestone; “Files” is CYBOU's familiar file-management surface.
See `81_BETA_PRODUCT_SCOPE.md`.

## Identity V2 integration gate

The current tree already implements the hybrid recovery/device keys, CYBV2
vault, AccountCreate, bounded device registry, and name commit/work/reveal
path. Promotion still requires freezing phrase/vault vectors and remaining
network parameters, finishing recovery/device edge cases and security GUI,
and verifying the versioned consensus authorization and persistence together.
Perform one intentional DEV reset only after the PQ consensus format and names
are integrated in the same cutover. Acceptance includes clean-machine restore,
vault tamper rejection, hybrid-signature failure tests, revoked-device
rejection, and adversarial name-claim ordering. See docs 10 and 76–78.

## Product gates

Milestone numbers do not authorize a new product surface by themselves.
Product expansion requires the following evidence.

### Gate A - Identity + Email Alpha

This gate validates the initial text-only Mail integration on a clean machine;
it does not establish Beta readiness:

- create and restore an identity;
- choose and use a `.cybou` name;
- add and revoke a device;
- send encrypted text mail to an offline recipient;
- later receive, verify and reply to that mail;
- preserve canonical identity and mail state across client restarts;
- pass four-validator restart and fault tests.

### Gate B - Storage + Beta Mail readiness

Before Beta, implement the minimal distributed Object Storage path and
integrate encrypted Mail attachments. Demonstrate offline-recipient retrieval,
integrity verification, provider audits, repair, recovery after interruption,
and stable accounting. The complete gates are in
`81_BETA_PRODUCT_SCOPE.md`.

Product readiness also includes the user-facing experience, not only backend
PUT/GET and MailTx integration. The Beta client must satisfy the Mail, Files,
shared design-system, and end-to-end acceptance contracts in docs 82–85:
familiar navigation, asynchronous progress and error states, Storage-backed
attachment flows, responsive desktop layouts, and progressive disclosure of
protocol detail.

### Gate C - Controlled Beta pilot

Run a controlled pilot with 20–100 real users for 8–12 weeks and four active
validators where f=1 tolerance is claimed. Measure repeated Mail and attachment
use, offline retrieval, recovery, multi-device use, Storage repair, support
burden, and history growth. Backup remains post-Beta. Files is in Beta scope;
there is no separate Drive application milestone.

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
- permissionless AccountID creation with anti-Sybil proof-of-work skeleton.

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
- permissionless AccountCreateOp and automatic onboarding bonus.

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

## v0.2.1 — CYBOU Email Alpha
- Inbox/Sent/Drafts/Archive;
- threads;
- Reply/Forward;
- local read/unread;
- recipient-discovery privacy review;
- MailEvidenceBundle export;
- initial text-only Mail profile;
- this milestone is not Beta readiness.

## v0.2.2 — Economics hardening
```text
MAX_SUPPLY = 100,000,000,000
decimals = 0
DEV_ONBOARDING_BONUS = 6,000
```

```text
4 fee CYBOU
-> 3 Security
-> 1 Onboarding
```

No priority fee.

## v0.2.3 — Object Storage Core for Beta
- opaque encrypted objects and manifests;
- bounded chunking and verifiable object commitments;
- placement, retrieval, leases, audits, repair and accounting;
- reliability and interruption tests;
- Files Beta UI: My Files, Recent, Starred, Trash, upload/download, folders,
  rename/move, progress, durability state, and advanced diagnostics;
- asynchronous transfer model; no network or Storage work on the Qt event loop;
- freeze operational/economic parameters before Beta.

## v0.2.4 — CYBOU Email Beta + encrypted attachments
- hybrid-PQ Mail confidentiality and authenticated recipient keys;
- encrypted attachment manifest and Store-backed objects;
- MailTx references/commits to content; attachment bytes stay off-chain;
- offline recipient retrieval, verification and local decryption;
- Gmail-familiar Inbox/Compose/reader/search under CYBOU visual identity;
- attachment protection progress and a minimum-durability Send gate;
- Download and Save to Files integration;
- delivery uncertainty distinct from rejection;
- pass the Mail and attachment scenarios in `85_BETA_UI_ACCEPTANCE.md`;
- evidence and historical sender authorization integrated.

## v0.2.5 — French Beta readiness
Security/legal/CRA/crypto-export/privacy/update/runbook work.

## v0.2.6 — Controlled Beta pilot
20–100 users for 8–12 weeks; four approved validators where claiming f=1 BFT tolerance. Size onboarding budget from integrated Email + Storage + Backup economics.

## v0.3.0 — Operator continuity
Design/test emergency Operator Authority succession without introducing normal DAO/community governance.

## v0.3.1 — European controlled expansion
Multiple EU validator operators/providers.

## v0.4 — Backup (post-Beta application)
## v0.7 — Email expansion / optional gateway research
## v0.8 — Sovereignty exercise
## v0.9 — Global-readiness review
## v1.0 — Production candidate

## BetaNet and Mainnet Progression
- **Separate Genesis**: BetaNet and Mainnet maintain separate genesis states; Beta balances do not carry forward to Mainnet.
- **Service Progression**: Email Alpha -> Storage Core -> PQ Mail + encrypted attachments + Files -> Beta -> Backup (post-Beta).
- **Economic Calibration**: Beta onboarding budget is sized from integrated Email + Storage + Backup usage profiles; the Mainnet onboarding bonus is frozen only after analyzing aggregate Beta operational metrics.
