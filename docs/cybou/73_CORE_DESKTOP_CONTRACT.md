# 73 — Core → Desktop Contract

Boundary between CYBOU core and the desktop GUI. The desktop never invents
protocol behavior: every state it shows arrives through this contract.

Identity V2 extends this contract only after core support exists: create and
restore requests, vault/phrase confirmation (local state), verified recovery
lookup, device add/revoke finality, and name commit/work/reveal finality. The
current fields below remain V1 DEV behavior. See `78_IDENTITY_DESKTOP_UX.md`.
`CybouNodeRuntime` implements the native producer/observer boundary; the GUI
(`src/qt`) consumes its verified state.

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
network_id                         canonical DEV NetworkID (already exposed)
last_finalized_height              from native runtime verified state
                                   (already exposed; GUI never derives it)
validator_count                    active validator set from native state
                                   (already exposed; weight 1)
```

`WaitingForFinality` is entered when the op is broadcast and left only when
a BFT finality certificate commits the block.

The current desktop opens native CYBOU state, polls the DEV bootstrap
endpoint for finalized blocks, and reports verified height/validator count.
The bootstrap address supplies transport location, not consensus trust.
The identity service submits AccountCreate remotely and waits for the
verified account state before reporting `Active`. Email, Storage, and Backup
pages remain capability-gated UI until their network services are live.

## Adapter surface (what core calls)

The GUI consumes exclusively through these `CybouDesktopModel` setters;
each is a no-op when the value is unchanged and emits `statusChanged()`
(resp. `capabilitiesChanged()`) only on real changes:

```text
setFinalityStatus(last_finalized_height, validator_count)
    BFT finality feed; -1 / 0 mean "not exposed".
setIdentityState(state, account_id, creation_height)
    Core drives identity transitions only (see the phase table above);
    reaching Active also clears the UI-side request-pending flag.
setBalances(balance, system_balance)
    From AccountState after every finalized transition that moves them.
setCapabilities(CybouCapabilities)
    One flag per protocol path, true only when live on the node.
```

UI-to-core direction is request-only: `requestCreateIdentity()`
(the `createIdentityRequested` signal) and page actions that core gates.
Nothing else crosses the boundary.

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
