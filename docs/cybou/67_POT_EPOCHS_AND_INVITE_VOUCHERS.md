# 67 — Deterministic Proof of Trust epochs and Invite Vouchers

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
new invited AccountID
-> 25 outgoing MailTx / PoT epoch
```

If one PoT epoch targets approximately one day, the UI may describe it as a daily limit.

Consensus uses the epoch number, not civil time.

## Integer-only Trust

Consensus PoT calculations use deterministic integer/fixed-rule arithmetic.

Avoid floating point.

## Invite Voucher

Creating an identity does not automatically receive 6,000 CYBOU.

A Welcome Grant requires a valid one-time operator-authorized Invite Voucher.

Conceptually:

```text
InviteVoucher {
    version
    network_id
    voucher_id
    beneficiary_account_id
    grant_amount = 6000 CYBOU
    expiry_epoch
    optional organization_id
    operator_authority_signature
}
```

The signed payload MUST bind the voucher to exactly one CYBOU network and one
beneficiary AccountID. A bearer-style voucher is rejected because interception
before redemption would otherwise allow another account to claim the grant.

The signature is calculated over a canonical versioned payload that excludes
the signature bundle itself. The exact encoding, network identifier, AccountID
encoding, signature-suite identifier and object-specific signing domain must be
frozen together before consensus redemption is enabled.

## Redemption

```text
valid voucher
+ new AccountID
+ voucher beneficiary matches new AccountID
+ voucher network matches active network
+ unused voucher_id
-> 6,000 CYBOU transferred
   OnboardingPool -> Account.SystemBalance
```

Consensus records that the voucher has been consumed so it cannot be replayed.

## Why voucher authorization exists

Without voucher control:

```text
cheap identity creation
-> repeated Welcome Grants
-> Onboarding Pool drain
```

The voucher makes the subsidized onboarding path permissioned while AccountID mechanics can remain separately designed.

## Organization onboarding

The operator may issue a bounded batch/allocation of vouchers to an approved organization.

The organization does not gain access to users' private keys or encrypted mail.

## Trust consequence

Because Welcome Grant goes directly to System Balance:

```text
Invite Bonus
-> System Balance
-> initial PoT contribution
```

No separate bootstrap-trust token exists.
