# 67 — Authority epochs and permissionless onboarding

Status: the file path is retained for compatibility; Identity Authority
supersedes the earlier PoT terminology.

## Canonical epoch

```text
epoch = floor(finalized_block_height / EPOCH_BLOCKS)
```

All canonical age/activity/liveness windows use this epoch.

No client wall clock, timezone or local midnight affects consensus accounting.

## Account creation

Account creation remains permissionless and protocol-native.

`AccountCreateOp` uses anti-Sybil work bound to NetworkID and AccountID.

There is no:

```text
voucher
operator approval
central activation
```

A successful AccountCreate:

- registers the Identity;
- creates the monetary account;
- transfers configured onboarding value from OnboardingPool to System Balance.

It does not mint supply.

## Authority at onboarding

Account creation establishes `creation_epoch`.

Age Authority begins from protocol lifetime.

The automatic onboarding System Balance amount gives:

```text
0 SystemContributionAuthority
```

A later voluntary user-authorized Balance->SystemBalance lock may earn the
one-time contribution defined by the Authority policy.

## Network separation

DEV, Beta and Mainnet may use different immutable genesis/network parameters.

Beta balances do not carry to Mainnet.
