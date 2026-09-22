# 02 — Architecture

## Desktop process

```text
cybou.exe
├── Qt UI
└── NodeCore
    ├── Identity / Wallet
    ├── Balance / System Balance
    ├── Proof of Trust
    ├── Chain State
    ├── Mail Protocol
    ├── BFT Consensus
    ├── P2P
    ├── Crypto
    ├── Object Storage later
    ├── Backup later
    ├── Drive later
    └── Persistence / Lifecycle
```

## Email architecture

```text
CYBOU Email UI
    -> Mail Protocol
    -> E2E encryption/signature
    -> MailTx
    -> ordinary CYBOU P2P propagation
    -> BFT finality
    -> bounded mail-validation state
```

There is no Email-specific relay/mailbox layer.

## Commercial operator vs network runtime

```text
CYBOU Owner / Operator
    owns product and business
    approves validators in current PoA admission model
    operates validator infrastructure

CYBOU Network
    independently verifies protocol rules
    replicates chain/state
    can gain additional approved validators
```

The operator has no master key to decrypt mail or arbitrarily debit user Balance.

## Proof of Trust

One AccountID PoT feeds bounded policies for:

```text
Email
Payments
Identity operations
future Storage
Backup
Drive
```

PoT is not validator voting power.

## Consensus

```text
BFT explicit finality
+
operator-approved authority admission
```

This is effectively a PoA admission model over a BFT consensus core.
