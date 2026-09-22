# 59 — Deterministic Fee Router

## v1 purpose

Route indivisible CYBOU protocol fees exactly between:

```text
Validators / Security
Onboarding
```

## State

```text
PendingFeePool
SecurityRewardPool
OnboardingPool
```

## v1 constants

```text
FEE_BATCH = 4

SECURITY_PER_BATCH   = 3
ONBOARDING_PER_BATCH = 1
```

Equivalent:

```text
75% / 25%
```

## Transition

```text
PendingFeePool += CollectedProtocolFees

batch_count = PendingFeePool / 4

SecurityRewardPool += 3 * batch_count
OnboardingPool     += 1 * batch_count

PendingFeePool -= 4 * batch_count
```

After routing:

```text
0 <= PendingFeePool < 4
```

## Examples

```text
1 fee:
Pending = 1
```

```text
4 total:
Security += 3
Onboarding += 1
Pending = 0
```

```text
11 total:
2 complete batches

Security += 6
Onboarding += 2
Pending = 3
```

## No mint / burn

For every routing event:

```text
decrease(PendingFeePool)
=
increase(SecurityRewardPool)
+
increase(OnboardingPool)
```

No CYBOU is created or destroyed.

## Future Store

A later protocol version can define a new batch ratio including Storage providers.

The v1 router deliberately contains no service-node bucket.
