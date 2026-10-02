# 57 — Identity Authority (AUTH)

AUTH is a separate network-native, non-transferable unit. It is not CYBOU,
is excluded from the 100 billion CYBOU supply, and is stored directly in
`AccountState`, committed by the finalized state root.

## Account model

```text
AccountState
├── Balance         CYBOU  spendable, transferable
├── System Balance  CYBOU  non-transferable service budget
└── Authority       AUTH   non-transferable
```

## Issuance

AUTH changes only through deterministic transitions in a PoA-finalized block:

```text
GenesisAllocation.authority        -> initial AUTH, claimed once by AccountCreate
finalized Identity-authorized op   -> +1 AUTH to its authorizing account
AUTH_GRANT (PoA only)              -> +N AUTH to target account
```

AccountCreate counts as an Identity-authorized operation: the new account
receives its genesis allocation (if any) plus 1 AUTH. `AUTH_GRANT` and
`AUTH_BURN` are not Identity-authorized operations and earn no AUTH.
Submitted or Validated operations earn nothing; only finality does.

## Burn

```text
AUTH_BURN (PoA only) -> -N AUTH from target account, floor 0
```

Automatic penalties require objectively verifiable protocol evidence and are
not yet frozen. An eligible Identity's Validation signature over an operation
that is invalid against the stated finalized base block is such evidence, but
no automatic penalty rule is defined.

## No transfer

There is no AUTH transfer between Identities. SystemLock, fees, onboarding
credit and storage leave AUTH unchanged. IdentityRotate preserves the account
and its AUTH.

## Validation eligibility

```text
validation_eligible(identity) :=
    latest_finalized_state.accounts[identity].authority > 1,000,000 AUTH
```

See [`VALIDATION.md`](VALIDATION.md).

## What AUTH does not do

AUTH does not grant PoA finalization, transfer or redeem CYBOU, create stake,
voting weight or quorum, create validator registry membership, or allocate
resources. Nodes read `state.accounts[id].authority`; nothing is derived from
history, and there is no AuthorityIndex, AuthorityPolicy, age or activity
accumulator.
