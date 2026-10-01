# 67 — Identity Authority and onboarding

Status: Authority is an informational, read-only metric. It does not allocate
resources, authorize operations, or affect PoA finality.

## Onboarding

Account creation remains permissionless and protocol-native.

`AccountCreateOp` uses anti-Sybil work bound to NetworkID and AccountID.

There is no voucher, operator approval, or central activation. A successful
AccountCreate registers the Identity and monetary account and transfers the
configured onboarding value from OnboardingPool to System Balance. It does not
mint supply.

Automatic onboarding credit earns no Authority. Authority policy does not
change the account-creation or monetary transition.

## Network separation

DEV, Beta, and Mainnet may use different immutable genesis/network parameters.
Balances do not transfer between networks.
