# 67 — Deterministic Proof of Trust epochs and Onboarding

## Consensus time rule

Consensus-critical Proof of Trust must not depend on:

- client wall clock;
- Windows clock;
- local timezone;
- local midnight;
- arbitrary user timestamp.

Use deterministic chain time units.

## Protocol epoch

Preferred v1 direction:

```text
PoTEpoch = deterministic function of finalized block height
```

For example:

```text
PoTEpoch = floor(height / EPOCH_BLOCKS)
```

The exact `EPOCH_BLOCKS` constant is implementation-tunable until block cadence is measured.

## Account age

Use:

```text
account_creation_height
account_creation_epoch
```

not wall-clock age.

Age contribution must use integer arithmetic and saturating/diminishing steps.

## Rate limits

Example:

```text
new AccountID
-> 25 outgoing MailTx / PoT epoch
```

If one PoT epoch targets approximately one day, the UI may describe it as a daily limit.

Consensus uses the epoch number, not civil time.

The current Beta policy uses one network-bound MailTx limit for all accounts.
SystemBalance funds fees but does not increase mail quota. Account age remains
an integer PoT signal; quota tiers require a later explicit policy revision.

## Integer-only Trust

Consensus PoT calculations use deterministic integer/fixed-rule arithmetic.

Avoid floating point.

## Protocol-native permissionless onboarding

Creating an AccountID is permissionless and protocol-native. No operator vouchers, invites, or central approvals exist.

To prevent Sybil flood and pool draining, account creation is protected by protocol-native anti-Sybil proof-of-work:

```text
AccountCreationWorkV1 {
    version
    network_id
    account_id
    initial_authorization_commitment
    work_epoch
    nonce
}
```

Canonical work hash domain: `CYBOU/ACCOUNT-CREATE-WORK/V1`.

## Onboarding bonus

When a valid `AccountCreateOp` is applied to consensus:

```text
valid AccountCreateOp
+ account_id does not exist
+ valid AccountCreationWorkV1 meeting network difficulty
+ matching initial_authorization_commitment
-> Onboarding bonus transferred:
   OnboardingPool -> Account.SystemBalance
```

- Dev testnet: 6,000 CYBOU
- Beta testnet: TBD
- Mainnet: TBD

The bonus is atomic, non-minted, and debited directly from `OnboardingPool`. It is placed into `SystemBalance` (non-spendable for transfer, only for protocol gas/bandwidth).

## Trust consequence

Because the onboarding bonus goes directly to System Balance:

```text
Onboarding Bonus
-> System Balance
-> no automatic increase in the MailTx quota
```

No separate bootstrap-trust token exists.
