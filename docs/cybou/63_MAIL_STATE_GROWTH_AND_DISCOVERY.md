# 63 — Mail state growth and discovery

## Problem

If every finalized email creates a permanent consensus-state entry, current state grows without bound.

This is unacceptable for a desktop full-node network.

## Frozen v0.0.1 decision

```text
MailTx != permanent current-state MailMarker
```

Mail is historical protocol data.

Consensus current state stores only information required to validate the next state transition.

## What current state may keep

Examples:

```text
AccountID
Balance
System Balance
Proof of Trust
current MailTx rate counters
account creation epoch
validator set
operator authority state
protocol parameters
fee pools
```

## What current state must not keep forever

```text
one row per MailTx
full email ciphertext
Inbox/Sent folder membership
read/unread state
thread UI metadata
```

## Discovery accelerator

Recommended direction:

```text
block/range Mail Filter
```

A filter commits to recipient-discovery tags present in a block or short range.

Recipient flow:

```text
sync header/filter
-> test local recipient discovery key/tag
-> if negative: skip body fetch
-> if positive: fetch relevant block/range
-> scan MailTx
-> verify/decrypt
```

This is an optimization, not part of mail authenticity.

The filter format must be deterministic and privacy-reviewed before freeze.

## Current implementation boundary

The v1 GCS block filter can be built and stored atomically with a finalized block.
Its filter-header hash can be computed, but the header chain is not yet committed
to authenticated block headers or served by a light-client protocol. A remote
filter therefore cannot currently be authenticated by a light client.

The discovery-tag hash accepts a recipient key and per-message salt, but no
recipient-side salt discovery mechanism has been specified. The current MailOp
also carries a recipient AccountID in its public serialization. These facts
prevent a claim of private, body-free mail discovery or social-graph
unlinkability until the outer MailOp and recipient workflow are redesigned and
privacy-reviewed.

## Why this is compatible with pruning

A pruned node can keep compact block/header/filter metadata while discarding old bodies according to retention policy.

Archive/validator retention before Store supplies historical ciphertext where required.

After Store, mail content retrieval moves away from historical block-body availability.
