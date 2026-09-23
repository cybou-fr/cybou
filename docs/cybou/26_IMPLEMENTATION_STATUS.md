# 26 — Implementation status v0.14

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
- explicit test seam for a future authority signature verifier.

Not yet implemented:

- AccountID;
- production Operator Authority signature verification;
- voucher redemption state transition.
