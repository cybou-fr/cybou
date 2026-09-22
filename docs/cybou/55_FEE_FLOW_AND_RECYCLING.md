# 55 — Fee flow and recycling

CYBOU v1 has no rewarded Email Relay/Mailbox service-node role.

Before Object Storage exists, recurring protocol fees have only two destinations:

```text
75%  Validators / Security
25%  Onboarding
```

There is no default burn.

There is no recurring owner tax.

The owner/operator earns validator rewards when it performs validator work.

## Integer settlement

CYBOU is indivisible.

The exact ratio is settled in 4-CYBOU batches:

```text
4 CYBOU fees
    -> 3 CYBOU SecurityRewardPool
    -> 1 CYBOU OnboardingPool
```

## PendingFeePool

Consensus state keeps:

```text
PendingFeePool
```

Routing:

```text
chunks = PendingFeePool / 4
remainder = PendingFeePool % 4

SecurityRewardPool += chunks * 3
OnboardingPool     += chunks * 1
PendingFeePool      = remainder
```

Invariant:

```text
0 <= PendingFeePool < 4
```

No rounding loss exists.

## Why 25% onboarding remains

The existing circulation model already used 25% recycling.

Keeping it preserves the onboarding sustainability assumptions.

## Why 75% validators in v1

Before Store, validators/full nodes perform the consensus work that gives MailTx its registration/finality property.

There is no separate paid Email delivery infrastructure.

The old 40% `Network Services` allocation is removed rather than paying a nonexistent service role.

## Future Storage fee-policy version

When CYBOU Object Storage becomes a production service, the fee policy may be versioned to add a Storage provider bucket.

That is a future protocol change.

Do not prepay a service that does not exist.

## Economic loop

```text
Onboarding Pool
    -> Invite Bonus
    -> System Balance
    -> Mail/Payment/etc. fees
    -> PendingFeePool
       -> 75% Validators
       -> 25% Onboarding
```
