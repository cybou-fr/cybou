# 72 — Desktop Wallet UI

The desktop client surfaces the two protocol balances from doc 52 natively.
No inherited wallet UI is used: the inherited Bitcoin wallet is disabled in
CYBOU builds and the desktop never touches coinbase/subsidy semantics.

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
  (Email today; Storage, Backup and future services later) and contributes
  to Proof of Trust. It cannot be transferred, withdrawn or traded, and the
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
Onboarding bonus   OnboardingPool -> System Balance (AccountCreate)
Email fee          deterministic size-aware fee, debited from System Balance
Payment            user-authorized Balance transfer
Lock to System     Balance -> System Balance (one-way)
```

Every entry shows the affected side (Balance or System), the signed amount
and its BFT finality state. The ledger is a local view: consensus state
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
