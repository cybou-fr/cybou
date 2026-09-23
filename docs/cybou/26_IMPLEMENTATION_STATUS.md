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
25 MailTx / epoch for new invited account
```

## Onboarding

```text
Operator-signed one-time Invite Voucher
-> 6,000 CYBOU
OnboardingPool -> SystemBalance
```

Identity creation alone does not receive the grant.

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
- bounded Invite Voucher envelope;
- frozen versioned beneficiary/network-bound canonical voucher payload;
- suite and authority-keyset binding in the signing preimage;
- epoch-windowed Operator Authority keyset model;
- strict structural requirement for both hybrid signature components;
- exact 6,000 CYBOU structural grant check;
- deterministic epoch expiry check;
- consumed-voucher replay check;
- typed Operator Authority verifier boundary with keyset and epoch checks.
- atomic one-time Welcome Grant transition from OnboardingPool to System Balance;
- one Welcome Grant per AccountID: a second valid voucher for an already-onboarded
  account is rejected as ALREADY_ONBOARDED without state mutation;
- consensus replay set keyed only by consumed voucher ID.
- canonical versioned redemption-state encoding and domain-separated state hash.
- atomic LevelDB snapshot persistence with paired state/hash verification.
- GitHub Actions headless build gate for CYBOU protocol unit tests.
- fixed-difficulty transition enforcement and deterministic 100-block fixture hash.
- full 684-case C++ unit suite passes with upstream Bitcoin protocol vectors
  isolated from CYBOU network magic and Base58 prefixes.
- strongly typed, fixed-width AccountID with null/length validation, integrated
  into Invite Voucher validation and redemption state.
- atomic finalized-redemption apply/rollback store boundary with per-block undo,
  CYBOU tip ordering and before/after state-hash verification.

Not yet implemented:

- production Operator Authority signature verification;
- connection of the voucher state-store boundary to the block validation/finality lifecycle.
