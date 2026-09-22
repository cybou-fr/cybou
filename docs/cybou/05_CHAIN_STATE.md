# 05 — Chain and state model

CYBOU uses consensus state only for data required to validate future state transitions.

Historical protocol operations, including MailTx, belong to block/history data.

## Current consensus state

Conceptually:

```text
ChainState
├── UTXO / Balance state
├── System Balance state
├── Proof-of-Trust state
├── Validator-set state
├── Operator-authority state
├── Identity/name state
├── mail rate/account counters
├── fee/reward pools
├── protocol parameters
└── storage-accounting state later
```

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
PreviousState
+ valid typed protocol operations
+ valid BFT transition
-> DeterministicNextState
```

## Future Store

After Object Storage:

```text
MailTx
-> content commitment / object reference

Store
-> encrypted body + attachments
```

State remains bounded and does not become the mailbox database.
