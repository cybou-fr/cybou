# 26 — Implementation status (2026-09-24)

This is the current code boundary. Frozen protocol decisions remain in
`24_DECISIONS.md`; this page distinguishes implemented DEV paths from
production requirements.

## Identity V2 gap

Current DEV has Ed25519-only V1 account authorization, AccountCreate work,
one active key, and CYBK1 local keystore. AccountID is typed opaque but the
local keystore currently sets it from the Ed25519 public key. There is no
24-word recovery, portable password vault, hybrid user authorization, device
registry, or `.cybou` consensus registry. Docs 10 and 76–78 define the V2
target; their presence does not mean it is implemented.

Local `identity_crypto` now derives distinct Ed25519/ML-DSA-65 Recovery Root
and Ed25519/ML-DSA-44 device key pairs from a supplied 32-byte secret using
HKDF-SHA256, and verifies both signature components. It is not connected to
AccountCreate, user operations, a vault, or consensus state.
The local `recovery_phrase` module now maps 256-bit entropy to a checksummed
24-word BIP-39 English phrase and back; it is not connected to the GUI or
AccountCreate.

## Hardened architecture

CYBOU Email target:

```text
first-class MailTx
E2E encrypted
signed
BFT finalized
text-only before Store
```

## State

```text
NO permanent per-mail consensus MailMarker
```

Current state contains only validation-relevant balances, identity, PoT,
validator set, counters and fee pools. Operator Authority is fixed in the
network definition, outside mutable consensus state.

Mail history remains block/history data.

## Pre-Store retention

Active validators temporarily retain full canonical pre-Store MailTx history.

Desktop nodes may prune.

Mass-scale Email is gated on Object Storage.

## BFT

```text
1 validator = Authority Mode, quorum 1/1, f=0
2–3 validators = Integration Mode, quorum 2/2 or 3/3, f=0
4 validators = minimum f=1 BFT target, quorum 3/4
```

All v1 validators have equal weight.

Admission is operator-approved.

## PoT

```text
block-height-derived epoch
integer arithmetic
25 MailTx / epoch for new account
```

## Onboarding

```text
Protocol-native AccountCreateOpV1 + AccountCreationWorkV1
-> Onboarding bonus (Dev: 6,000 CYBOU; Beta/Mainnet: TBD)
OnboardingPool -> SystemBalance
```

Permissionless: no operator vouchers or central approval.
Identity creation alone does not mint tokens; bonus is debited directly from OnboardingPool.

## Operator keys

Separate:

```text
Operator Authority
Operator Validator
Release Signing
Treasury
```

Operator Authority custody target: 2-of-3.

## Fees

MailTx uses deterministic size-aware integer fees.

Priority fees are disabled in v1.

## Current code boundary

The current `CYBOU-DEV v0.0.2` chain is a disposable network/bootstrap chain.
It has CYBOU-specific genesis, network magic, ports, address prefixes and seed
isolation, but still uses inherited Bitcoin PoW, subsidy and amount semantics.

It is therefore:

```text
NOT monetary-policy-valid
NOT BFT-finality-valid
NOT production-compatible
```

Do not build Balance/System Balance or issuance assumptions on the inherited
coinbase/subsidy path. The development genesis may be reset when the CYBOU BFT
and deterministic state-transition layers replace the bootstrap consensus.

The native CYBOU state path now has a standalone `cybou-node` DEV Authority
producer, bounded TCP block feed and operation submission, and an observer
that verifies each finalized block before committing it. The Qt desktop
creates or opens a local native state store, follows the compiled-in DEV
bootstrap endpoint, displays verified finality status, and uses the identity
service to submit AccountCreate operations. `CybouStateStore` remains the
canonical state owner, with candidate roots computed by
`ComputeCandidateStateRoot` over its persisted parent state.

This path is **N=1 Authority Mode, f=0**. The core BFT engine and simulator
exercise multi-validator rounds and fault cases, but independent validators,
crash-safe consensus, peer discovery, authenticated transport, and production
network operation are not integrated. The inherited Bitcoin bootstrap chain
still exists separately and must not be described as CYBOU BFT consensus.

See `74_AUTHORITY_BLOCK_FEED.md`, `75_DEV_NODE_RUNBOOK.md`, and
`73_CORE_DESKTOP_CONTRACT.md` for the respective transport, process, and
desktop boundaries.

Network-definition and genesis serialization changed during hardening.
Previously initialized disposable DEV state may require a new genesis;
stored NetworkIDs are deliberately checked on reopen.

## v0.0.3 implementation

Implemented skeleton:

- distinct Operator Authority / Validator / Release Signing / Treasury domains;
- permissionless AccountCreateOpV1 and AccountCreationWorkV1 data structures;
- versioned `ProtocolOperationV1` typed dispatcher with canonical AccountCreate encoding;
- domain-separated anti-Sybil work hashing (`CYBOU/ACCOUNT-CREATE-WORK/V1`);
- proof-of-work difficulty verification (leading zero bits);
- network, account ID, and initial authorization commitment bindings;
- canonical immutable `CybouNetworkDefinitionV1` and domain-separated NetworkID;
- NetworkID binding for the fixed Operator Authority keyset and the Beta MailTx quota;
- shared block-operation executor used by the state-store commit path and available to BFT execution callbacks;
- BFT proposal checks against locally executed state root before prevote;
- network definition binding of genesis block, genesis state root, protocol parameters and initial validator-set commitment;
- structural network-definition validation before state initialization, loading or transition;
- atomic onboarding bonus transition from OnboardingPool to System Balance;
- duplicate AccountID prevention;
- canonical versioned state encoding and domain-separated state hash (`CYBOU/STATE/V1`);
- `CybouStateStore` as the sole owner of canonical CYBOU consensus state;
- immutable network definition owned by `CybouStateStore`, eliminating per-block caller-supplied NetworkID and protocol parameters;
- persisted NetworkID verification that rejects reopening canonical state with an incompatible network definition;
- atomic LevelDB persistence with paired state/hash verification;
- candidate-validate-commit processing for BFT-finalized blocks;
- atomic persistence of candidate state, state root, finalized tip and finalized height;
- finalized-parent and contiguous-height ordering, duplicate-finalized-block rejection and no production rollback/undo path;
- primitive OpenSSL hybrid Ed25519 + ML-DSA-65 Operator Authority signature verification;
- strongly typed, fixed-width AccountID with null/length validation;
- C++ unit tests covering account creation, signing domains, state transitions,
  persistence integrity, invalid/mismatched network-definition rejection,
  atomic failure behavior, finalized-tip/height ordering and duplicate-block rejection.
- dedicated `cybou-core-test` executable and CI path containing only native
  CYBOU suites, separate from the inherited Bitcoin test universe;
- strictly hardened BFT validator topology: $N=4, Q=3, f=1$ with equal validator weight = 1;
- BFT consensus state machine and simulator harness (`BftValidatorNode`, `BftSimulator`) verifying normal rounds, $f=1$ crash tolerance, leader timeout advance, partition safety, and healing;
- canonical `CybouBlockV1` with `ComputeBlockId` cryptographically committing to `parent_block_id`, `height`, operations commitment, and `resulting_state_root`;
- `FinalizedBlockV1` binding `CybouBlockV1` and `BftFinalityCertificateV1`;
- state store atomic `CommitFinalizedBlock` requiring `FinalizedBlockV1`, verifying BFT certificate against validator set, and ensuring exact `resulting_state_root` match;
- block retrieval via `CybouStateStore::GetBlock`;
- universal `SystemBalance` economics: payment fees and mail fees debited strictly from `SystemBalance` (user `balance` is strictly for user transfers);
- removed wire `fee` field from `PaymentOpV1` and `MailOpV1` to completely prevent fee bidding and under/overpayment attacks;
- cryptographic Proof of Possession (`CYBOU/ACCOUNT_POP/V1`) enforced on `AccountCreateOpV1`;
- `AccountState` cleaned of non-operational fields (`initial_auth_commitment` removed);
- `MailOpV1` explicitly marked DEV EXPERIMENTAL / NOT WIRE-FROZEN;
- consensus-operation wiring of Operator Authority signature verification (`ValidatorAdmissionOpV1` and `ValidatorRemovalOpV1`) with domain-separated hybrid `Ed25519 + ML-DSA-65` signatures binding `NetworkID`;
- tracking active validator set in canonical consensus state (`CybouState`) and enforcing equal weight = 1, unique keys/IDs, and non-empty active set invariants in `CybouStateStore`.
- `CybouAuthorityNode` N=1 block production using the canonical executor,
  with a bounded pending-operation queue and stop at validator-set transition;
- `cybou-node` DEV process with persistent LevelDB, validator key file,
  verified observer synchronization, and bounded remote operation submission;
- `CybouNodeRuntime` sharing native state operations between the headless
  process and Qt desktop; a DEV bootstrap endpoint is transport metadata,
  while the network definition remains the trust root;
- `NodeRuntimeState` tracking runtime health (`UNINITIALIZED`, `READY`,
  `NETWORK_MISMATCH`, `CORRUPT`) with automated state-mismatch reset in the desktop;
- crash-safe `CybouKeyStore` atomic persistence (`.tmp`, `FlushFileBuffers`,
  `.bak` rotation, atomic rename), legacy 32-byte auto-migration, and memory cleansing;
- `CybouIdentityService` pre-saving key custody to disk before PoW and
  network broadcast;
- 128 KiB operation transport framing boundary accommodating full Mail payloads;
- structured operation submit replies with SHA-256 domain-separated OperationID;
- fine-grained `SyncPeerResult` distinguishing up-to-date observer state from
  connectivity loss;
- local `CybouIdentityService` creation flow, proof of possession, remote
  AccountCreate submission, finality wait, and OS-protected identity
  keystore on Windows;
- compact Mail discovery filters and Mail evidence bundle construction and
  verification in core, without a complete Email delivery service.

Desktop GUI:

- native wallet page and Home balance card surfacing `Balance` / `System Balance` per doc 52 (whole-CYBOU rendering, one-way lock labeling, local activity ledger, capability-gated actions);
- Identity page walking the full AccountCreateOp flow (local keys, AccountCreationWork, broadcast, BFT finality, atomic OnboardingPool funding) with per-phase explanations and onboarding economics (DEV/Beta/Mainnet separation, no carry-over, no free credits);
- full Email client UI enforcing MailTx rules (one recipient, text-only, strict size meter, deterministic size-aware fee line, local read-state);
- Storage and Backup client UIs (opaque content-addressed object list with pin/prune retention; encrypted backup sets with identity-key restore binding);
- Network page with explicit-BFT finality section (finalized height, validator count, f = 1 ≥ 4 validators rule) fed by the doc 73 status contract;
- core → desktop integration contract (doc 73): status fields, capability flags, service data flows, absolute rules both sides obey;
- node diagnostics restyled to the CYBOU theme; inherited Bitcoin locale files dropped until real CYBOU translations exist.
- Qt desktop runtime opens native CYBOU state, follows verified DEV blocks,
  reports finalized height and validator count, and gates service actions
  according to connected capabilities.

Not yet implemented:

- independent multi-validator operation with durable consensus recovery;
- peer discovery and a native, authenticated P2P message lifecycle beyond
  the bounded DEV TCP block/operation transport;
- complete encrypted CYBOU Email send/receive and local mailbox indexing;
- operational Object Storage, Backup, and Drive;
- removal of inherited Bitcoin PoW, subsidy, wallet, and runtime paths.
