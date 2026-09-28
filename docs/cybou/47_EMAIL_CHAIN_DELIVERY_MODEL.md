> Historical/current-DEV scope: this document describes the BFT, MailTx, validator-set, or indexed-object protocol currently running on DEV. It is superseded for the next-gen target by `POA_FINALITY.md`, `ENCRYPTED_CHUNK_DAG.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, and `IDENTITY_DISCOVERY_AND_RECOVERY.md`. Product and UX requirements remain applicable only where they do not conflict with those target documents. No cutover is active yet.
# 47 — Email chain synchronization model

CYBOU Email does not require direct recipient connectivity.

## Send

```text
sender
-> normal P2P transaction propagation
-> validators
-> finalized MailTx in block history
```

## Receive

```text
recipient
-> sync headers / consensus state / mail discovery filters
-> identify relevant historical block/range
-> obtain MailTx ciphertext
-> verify
-> decrypt locally
```

## No Email-specific relay/mailbox

There is no dedicated Email Relay, Mailbox Store or ServiceNodeRegistry in v1.

## Pre-Store retention

Before Object Storage, active validators retain the full canonical pre-Store MailTx history required for historical mail retrieval.

Desktop full nodes may use bounded pruning.

This is a temporary controlled-scale rule.

## Beta Mail with Object Storage

Object Storage is a Beta dependency, not only a later mass-scale gate. The
initial DEV/Alpha text-only path can use pre-Store history retention; it does
not satisfy Beta readiness.

After Store:

```text
recipient
-> finds MailTx commitment
-> retrieves the encrypted manifest and attachment object(s) from Store
-> verifies root/commitment
-> decrypts locally
```

Attachment bytes are not placed in blocks or consensus state. Storage
availability, placement, leases, audits, and repair must meet the Beta gates in
`81_BETA_PRODUCT_SCOPE.md`.
