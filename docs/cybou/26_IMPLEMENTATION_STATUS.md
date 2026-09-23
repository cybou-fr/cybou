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
- domain-separated anti-Sybil work hashing (`CYBOU/ACCOUNT-CREATE-WORK/V1`);
- proof-of-work difficulty verification (leading zero bits);
- network, account ID, and initial authorization commitment bindings;
- atomic onboarding bonus transition from OnboardingPool to System Balance;
- duplicate AccountID prevention;
- canonical versioned state encoding and domain-separated state hash (`CYBOU/STATE/V1`);
- atomic LevelDB snapshot persistence with paired state/hash verification (`CybouStateStore`);
- atomic finalized block apply/rollback store boundary with per-block undo, CYBOU tip ordering and before/after state-hash verification;
- strongly typed, fixed-width AccountID with null/length validation;
- full C++ unit test suite covering account creation, signing domains, state transitions, and store rollbacks.

Not yet implemented:

- production Operator Authority signature verification for validator admissions;
- connection of the state-store boundary to the block validation/finality lifecycle.
