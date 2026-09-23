# 73 — Core → Desktop Contract

Boundary between CYBOU core and the desktop GUI. The desktop never invents
protocol behavior: every state it shows arrives through this contract.
Core implements the producer side; the GUI (`src/qt`) is the consumer.

## Direction of truth

```text
core -> CybouDesktopModel -> pages
```

The desktop owns no protocol state of its own. UI-side request flags
(identity creation pending) are bookkeeping only, never protocol state.

## Status fields (`CybouDesktopStatus`)

Core populates, in order of expected arrival:

```text
height, peer_count, node_running   from the running node (already live)
data_directory                     after node startup (already live)
identity_state                     AccountCreateOp accepted -> CreatingKeys
                                   work verified            -> PerformingWork
                                   op broadcast             -> Broadcasting
                                   BFT certificate attached -> Active
                                   (transitions only; the GUI never sets them)
account_id, creation_height        from the finalized AccountCreateOp
balance, system_balance            from AccountState after every
                                   finalized transition that moves them
network_id                         canonical NetworkID once exposed
last_finalized_height              from the BFT finality feed (-1 until
                                   exposed; the GUI never derives it)
validator_count                    current epoch validator set (equal
                                   weight 1; 0 until exposed)
```

`WaitingForFinality` is entered when the op is broadcast and left only when
a BFT finality certificate commits the block.

## Capabilities (`CybouCapabilities`)

Each flag flips to true only when the corresponding protocol path is live
on the node, not when it is planned:

```text
account_creation   node accepts AccountCreateOpV1 + AccountCreationWorkV1
payments           PaymentOpV1 processing wired
email              MailOp/MailTx processing wired
storage            Object Storage placement wired
backup             Backup service wired
```

The GUI gates every mutating action on the matching flag plus
`identity_state == Active` and renders the exact missing precondition
when disabled.

## Service data

- Wallet ledger entries come from finalized protocol operations
  (onboarding bonus, MailTx fees, payments, LOCK_TO_SYSTEM).
- Email messages arrive with their evidence bundle (doc 69: inclusion
  proof, BFT finality certificate, sender-key authorization, salted
  domain-separated content commitment); the local client owns Inbox/Sent/
  read-state indexes.
- Storage objects are opaque CIDs with size and retention; the GUI never
  sees plaintext names or paths.
- Backup sets report size, time and verification state; restore is bound
  to the local identity keys.

## Absolute rules for both sides

- Disabled-state honesty: no control ever pretends to have performed an
  operation.
- Amounts are whole CYBOU everywhere (decimals = 0).
- The GUI offers no operation that violates docs 52 invariants
  (no admin debit, no System Balance withdrawal, no reverse lock).
- System Balance is debited only by deterministic protocol rules; the GUI
  renders this and adds no manual shortcut.
