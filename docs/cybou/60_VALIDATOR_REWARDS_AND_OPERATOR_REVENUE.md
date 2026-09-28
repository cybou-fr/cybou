> Historical/current-DEV scope: this document describes the BFT, MailTx, validator-set, or indexed-object protocol currently running on DEV. It is superseded for the next-gen target by `POA_FINALITY.md`, `ENCRYPTED_CHUNK_DAG.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, and `IDENTITY_DISCOVERY_AND_RECOVERY.md`. Product and UX requirements remain applicable only where they do not conflict with those target documents. No cutover is active yet.
# 60 — Validator rewards and operator revenue

Until CYBOU Object Storage exists, validators are the only recurring rewarded infrastructure role.

## Eligibility

A validator earns an epoch reward when:

```text
consensus participation >= 90%
AND
no confirmed equivocation
AND
validator is in the active operator-approved ValidatorSet
```

## Distribution

Eligible validators split the epoch payout equally.

No stake weighting in v1.

Example:

```text
Security payout = 7,500 CYBOU
eligible validators = 5
-> 1,500 CYBOU each
```

If only the Operator Validator exists:

```text
eligible validators = 1
-> operator receives the full validator payout
```

## Operator model

CYBOU is a commercial owner/operator service.

The operator is currently the authority that approves validator admission and is expected to remain a validator.

Its recurring protocol revenue comes from valid validator participation, not from a hidden fee tax.

## Reward epoch

Reward calculation is epoch-based.

Candidate:

```text
1 protocol day
```

Exact duration remains implementation-tunable.

## Integer remainder

If a validator payout does not divide exactly:

```text
remainder
-> remains in SecurityRewardPool
-> next epoch
```

No fractional CYBOU.

## Future Storage

When Store launches, storage-provider rewards are specified separately.

Do not reintroduce generic Relay/Mailbox rewards.
