# 05 — Chain and state model

CYBOU uses consensus state only for data required to validate future state transitions.

Historical protocol operations, including MailTx, belong to block/history data.

## Immutable network definition

Each DEV, Beta or Mainnet instance is identified by a domain-separated hash of
one canonical `CybouNetworkDefinitionV1`:

```text
protocol version
+ genesis block ID
+ genesis state root
+ immutable protocol parameters
+ initial validator-set commitment
-> NetworkID
```

The state store owns this definition for its lifetime. Block callers cannot
substitute a different NetworkID or parameter set for individual transitions.
Genesis initialization persists the derived NetworkID beside the canonical
state. Reopening that database with any definition that hashes to a different
NetworkID fails before state transition processing.

Before initialization, the definition itself must pass structural validation:
the version must be supported; genesis block, genesis state and validator-set
commitments must be nonzero; account-creation work difficulty must fit the
256-bit work hash; and epoch length and per-block AccountCreate capacity must be
nonzero. Invalid definitions cannot initialize, load or advance canonical state.

## Target canonical consensus state

Conceptually:

```text
ChainState
├── AccountState
│   ├── Balance
│   ├── System Balance
│   ├── authorization
│   └── operation nonce / replay protection
├── Proof-of-Trust state
├── Validator-set state
├── Operator-authority state
├── Identity/name state
├── mail rate/account counters
├── fee/reward pools
├── protocol parameters
└── storage-accounting state later
```

The inherited Bitcoin UTXO/Script ledger remains transitional bootstrap code,
not the target CYBOU monetary model. Native Payment must use typed account
operations and `AccountState`; UTXO/Script removal occurs only after equivalent
Payment, authorization, fee and BFT block-production paths are implemented and
tested.

## Not current state

Do not create permanent per-email consensus objects.

```text
NO MailMarker[MailID] forever
NO full email ciphertext in state
NO Inbox/Sent UI state
NO read/unread state
```

## Historical protocol operations

```text
Block
├── Payment
├── Identity
├── SystemBalance
├── MailTx
├── ValidatorSet
├── OperatorAuthority
└── ProtocolParameter
```

MailTx is a first-class CYBOU protocol operation.

## Mail discovery

A compact block/range filter or equivalent discovery accelerator may be committed/stored outside the permanent per-account/per-mail state model.

Its job is discovery efficiency, not mail authenticity.

## Determinism

```text
CanonicalFinalizedState
+ valid typed protocol operations
+ finalized child block
-> CandidateState
-> atomic canonical commit
```

`CybouStateStore` is the sole owner of canonical CYBOU consensus state. Callers
may load and verify that state, but they do not provide an independently mutable
state copy to the production commit path.

Block operations execute against a temporary candidate derived from the current
canonical state. A validation failure leaves the stored state unchanged. After
BFT finality, the candidate state, its state root, the finalized block ID and
finalized height are persisted atomically. Each child height must equal the
previous finalized height plus one, so a caller cannot advance protocol epochs
with an arbitrary height jump.

Explicit BFT finality means a committed finalized CYBOU block is not reorged.
The CYBOU state engine therefore has no production rollback or per-block undo
path. Recovery from database corruption or operational failure belongs to
verified state sync, backup and disaster-recovery procedures, not consensus
reorganization.

## Future Store

After Object Storage:

```text
MailTx
-> content commitment / object reference

Store
-> encrypted body + attachments
```

State remains bounded and does not become the mailbox database.
