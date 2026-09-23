# 26 — Implementation status v0.0.1

## Hardened architecture

CYBOU Email:

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

Current state contains only validation-relevant balances, identity, PoT, authority, counters and fee pools.

Mail history remains block/history data.

## Pre-Store retention

Active validators temporarily retain full canonical pre-Store MailTx history.

Desktop nodes may prune.

Mass-scale Email is gated on Object Storage.

## BFT

```text
1 validator = dev
2–3 = integration
4 = minimum f=1 target
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

## v0.0.3 implementation

Implemented skeleton:

- distinct Operator Authority / Validator / Release Signing / Treasury domains;
- permissionless AccountCreateOpV1 and AccountCreationWorkV1 data structures;
- versioned `ProtocolOperationV1` typed dispatcher with canonical AccountCreate encoding;
- domain-separated anti-Sybil work hashing (`CYBOU/ACCOUNT-CREATE-WORK/V1`);
- proof-of-work difficulty verification (leading zero bits);
- network, account ID, and initial authorization commitment bindings;
- canonical immutable `CybouNetworkDefinitionV1` and domain-separated NetworkID;
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
- `MailOpV1` explicitly marked DEV EXPERIMENTAL / NOT WIRE-FROZEN.

Not yet implemented:

- consensus-operation wiring of Operator Authority signature verification for validator admissions;
- connection of the state-store boundary to the P2P wire message lifecycle.
