# 72 — Desktop Wallet UI

The desktop client surfaces the two protocol balances from doc 52 natively.
CYBOU has no block subsidy; the wallet shows only these finalized balances.

Normal product wording is Available / Network balance (French: Disponible /
Solde réseau). Network balance is the canonical System Balance from doc 52,
not a separate asset or credit. Funding remains an irreversible Balance ->
System Balance operation with explicit review. Identity does not duplicate
Wallet balances; technical diagnostics retain canonical field names.

## Where balances live

```text
Home
├── Balance card: Balance + System Balance summary
└── Wallet page (navigation)
    ├── Balance metric
    ├── System Balance metric
    ├── Actions: Send / Lock to System Balance / Receive
    └── Activity ledger
```

## Rendering rule

CYBOU is indivisible (decimals = 0). Every amount renders as a whole-number
CYBOU value with locale grouping. There is no decimal fraction, no smallest-
unit toggle, no satoshi-style subunit anywhere in the desktop UI.

## Balance card semantics

- `Balance` is user-controlled. The UI exposes no operation that debits it
  without the user's own authorization.
- `System Balance` is frozen CYBOU that pays deterministic protocol fees
  (Beta Email/Storage and later services) and contributes
  to protocol fees. It cannot be transferred, withdrawn or traded, and the
  UI offers no path that contradicts this.

## One-way lock

```text
Balance
   |
   | Lock to System Balance…
   v
System Balance
```

The action is labeled as irreversible. There is no reverse direction in the
UI and none will be added.

## Activity ledger

The wallet page renders a local ledger of protocol operations that move
balances:

```text
Onboarding bonus   Central Treasury -> System Balance (AccountCreate)
Publication fee    deterministic size-aware fee, debited from System Balance
Payment            user-authorized Balance transfer
Lock to System     Balance -> System Balance (one-way)
```

Every entry shows the affected side (Balance or System), the signed amount
and its verified PoA finality state. The ledger is a local view: consensus state
keeps validation-relevant balances only.

## Gating

Transfer actions are enabled only when:

```text
identity state = Active
payments capability reported by the node
```

Until then the actions stay disabled and the page states the exact missing
precondition. Disabled-state honesty is a hard rule: the desktop never
pretends to create an operation.

Payment and one-way lock submissions run on the wallet service worker. A
separate operation mutex serializes nonce selection and submission, while the
ledger mutex is held only for short entry updates and snapshots. A network wait
for peer synchronization or operation acknowledgment therefore does not block
the Qt event loop or ledger readers; results return to the page through a
queued UI callback.

## Review, service budget and evidence (delivery target)

Build on the existing review and asynchronous pending-operation coordinator.
Review shows resolved recipient, whole-number amount, exact known fee, affected
Balance and irreversible System Balance lock before signing. Prevent duplicate
submission; uncertain delivery uses the same OperationID/bytes. Local sync
completion is not a proof of global freshness.

Show finalized service budget separately from current lease/rent observations.
Any forecast declares its assumptions and observation time. A count derived
only from payment fees is not an estimate of Mail/Files capacity: storage rent,
publication size, replicas and lease duration also cost CYBOU. Hide unsupported
estimates rather than presenting false precision. Storage earnings come from
verified foreign service and finalized settlements, not capacity declarations.
