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

## Scale transition

Broad mass-scale native Email is gated on CYBOU Object Storage.

After Store:

```text
recipient
-> finds MailTx commitment
-> retrieves encrypted object from Store
-> verifies root/commitment
-> decrypts locally
```
