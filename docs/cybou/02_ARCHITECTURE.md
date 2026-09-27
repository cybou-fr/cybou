# 02 — Architecture

The diagrams below describe the target shared network. The Qt desktop opens a
native `CybouNodeRuntime`, bootstraps over CYP2 by default, verifies finalized
blocks, and submits supported operations. Name and Wallet use the durable
device operation coordinator; Mail encryption and Object Storage are not yet
operational end to end. See `26_IMPLEMENTATION_STATUS.md` for implementation
status and `86`–`88` for the identity and encrypted-object architecture.

Identity is the security root. Name, Wallet, Mail, and Files are identity
capabilities and share device authorization and key lifecycle. Files and
Storage-backed encrypted Mail attachments are Beta requirements. Backup is
post-Beta; there is no separate CYBOU Drive product. See
`81_BETA_PRODUCT_SCOPE.md`.

## UX invariant

Protocol complexity must not leak into normal user workflows. The normal UI
should expose names and outcomes such as `stanislav.cybou`, `Sending`,
`Delivered / Finalized`, `Backup protected` and `Device revoked`, rather than
AccountID, nonce, epoch, PoW difficulty, validator quorum, ML-DSA, block height
or OperationID. Advanced and diagnostic views may expose protocol detail.

## Desktop process

```text
cybou.exe
├── Qt UI
└── NodeCore
    ├── Identity Security Substrate
    │   ├── Recovery authority
    │   ├── Device signing
    │   ├── Device key agreement (target)
    │   └── DeviceOperationCoordinator
    ├── Identity capabilities
    │   ├── Name
    │   ├── Wallet
    │   ├── Mail
    │   └── Files
    ├── Encrypted Object Layer
    ├── Object Storage (Beta requirement)
    ├── BFT finality and verified state
    ├── CYP2 networking
    └── Persistence / lifecycle
```

The identity holds the authorization and key lifecycle; private keys remain
under client control. Signing keys and encryption/KEM keys are separate
domains. Mail and Files do not create independent identities or parallel
device-authorization systems. Backup is a post-Beta application of the shared
Storage layer.

## Email architecture

```text
CYBOU Email UI
    -> Mail Protocol
    -> Identity-published recipient capability
    -> E2E encryption + identity device authorization
    -> MailTx
    -> ordinary CYBOU P2P propagation
    -> BFT finality
    -> bounded mail-validation state
    -> encrypted manifest/object reference
    -> CYBOU Object Storage for opaque attachment bytes
```

Attachment bytes stay out of chain/state. Store holds opaque encrypted objects;
the recipient retrieves and decrypts locally, even if offline at send time.
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
Files
Backup
```

PoT is not validator voting power.

## Consensus

```text
BFT explicit finality
+
operator-approved authority admission
```

This is effectively a PoA admission model over a BFT consensus core.
