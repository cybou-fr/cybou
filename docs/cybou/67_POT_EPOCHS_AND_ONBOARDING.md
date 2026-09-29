# 67 — Deterministic epochs and permissionless onboarding

## Consensus time

Consensus behavior never depends on client wall clock, local timezone, local
midnight, or arbitrary user timestamps. Protocol epochs derive from finalized
block height using immutable network parameters:

```text
epoch = floor(block_height / EPOCH_BLOCKS)
```

Account age and Proof of Trust use creation height/epoch and valid protocol
history. Calculations use deterministic integer arithmetic. System Balance is a
service budget and does not increase Beta PoT score. Generic RootPublication
limits are based on public encoded bytes and chunk count; hidden Mail-specific
quota rules do not exist.

## Account creation

Account creation is permissionless and protocol-native. It uses
AccountCreationWork bound to NetworkID and AccountID. There are no vouchers,
ordinary-user operator approvals, or central activation step.

A valid AccountCreate atomically registers the Identity, creates the monetary
account, and transfers the network-configured onboarding bonus from
OnboardingPool to System Balance. It does not mint supply and does not create a
service-specific free-credit system.

DEV, Beta, and Mainnet use separate genesis and economic parameters. Beta's
onboarding budget is calibrated from integrated Mail, Files/Storage, and
post-Beta Backup capacity; Mainnet parameters wait for measured Beta data.
