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
of 10,000,001 AUTH stays exactly 10,000,001. Submitted or Validated operations
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
    latest_finalized_state.accounts[identity].authority > 10,000,000 AUTH
```

See [`VALIDATION.md`](VALIDATION.md).

## Resource ladder and anti-spam limits

AUTH serves as the network's deterministic rate-limiting and resource-allocation governor:
- **Anti-Spam protection**: freshly created identities with 0 AUTH start with conservative per-epoch operation limits. Storage is no longer an AUTH resource: it is paid by lease (DEC-274).
- **Progressive tier scaling**: as an account demonstrates utility through verified storage retention, high uptime, and finalized operations, its AUTH increases and its limits expand (DEC-272). Block execution enforces them against the parent finalized AUTH:

| Tier | AUTH | Ops / block | Ops / epoch (1024 blocks, ~17 min) | Relay PoW |
|---|---|---|---|---|
| T0 | < 10,000 | 1 | 30 | 22 bits |
| T1 | >= 10,000 | 5 | 150 | 21 bits |
| T2 | >= 100,000 | 25 | 750 | 20 bits |
| T3 | >= 1,000,000 | 100 | 3,000 | 19 bits |
| Validator | > 10,000,000 | 1,000 | 30,000 | 18 bits |

  The Validator tier (`> 10,000,000 AUTH`) is also eligible to sign Validation attestations. Its limits are high but finite so that no single Identity can fill a block.
- **Relay proof-of-work** (DEC-273): every user operation needs the tier's proof-of-work before any Full Node, the PoA included, holds or relays it. Name operations need 4 more bits. The work never enters a block.

## What AUTH does not do

AUTH does not grant PoA finalization, transfer or redeem CYBOU, create stake,
voting weight or quorum, or create validator registry membership. Nodes read
`state.accounts[id].authority` directly; nothing is derived from loose off-chain
heuristics, and there is no AuthorityIndex, AuthorityPolicy, age or activity
accumulator.
