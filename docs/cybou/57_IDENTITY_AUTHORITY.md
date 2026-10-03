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
GenesisAllocation.authority          -> initial AUTH, claimed once by AccountCreate
finalized utility op                 -> +1 AUTH to authorizing account (max 1 per block)
PoaAuthAdjustment GRANT N            -> +N AUTH to target account
```

```text
RootPublication, SystemLock          -> +1 AUTH (velocity-capped at max 1 per block)
Payment, IdentityRotate              -> 0 AUTH (no Sybil or ping-pong farming)
NameCommit, NameReveal               -> 0 AUTH
AccountCreate                        -> genesis AUTH or 0, no +1
PoaAuthAdjustment                    -> no +1
```

The Identity does not exist before its AccountCreate, so a genesis allocation
of 1,000,001 AUTH stays exactly 1,000,001. Submitted or Validated operations
earn nothing; only finalized block execution does.

## Burn

```text
PoaAuthAdjustment BURN N -> target AUTH -= min(AUTH, N)
```

`PoaAuthAdjustment { action: GRANT | BURN, target_account_id, amount, nonce,
poa_signature }` is signed by the genesis-authorized PoA finalizer key; every
node verifies that signature itself. GRANT issues new AUTH and BURN destroys
it.

Automatic penalties require objectively verifiable protocol evidence and are
not yet frozen. An eligible Identity's Validation signature over an operation
that is invalid against the stated finalized base block is such evidence, but
no automatic penalty rule is defined.

## No transfer

There is no AUTH transfer between Identities. Locked, paid or stored CYBOU
amounts never scale AUTH, and onboarding credit leaves it unchanged.
IdentityRotate preserves the account and its AUTH (and earns 0 AUTH).

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
