# 74 — Authority block production and verified block feed

## Current implementation

`CybouAuthorityNode` provides the first canonical single-validator block
production path. It accepts up to 256 pending operations, checks the entire
candidate batch against the current state, builds a block using the same
executor as `CybouStateStore`, obtains a 1/1 BFT certificate, and commits the
finalized block atomically. The queue clears only after a successful commit.

An operator-authorized transition block can add validators while N=1. The
old validator finalizes that block; the next height uses the new set. The
single-validator producer refuses to continue once N is no longer 1.

`CybouStateStore` indexes finalized blocks by height as well as block ID.
For older local databases without height keys, reads walk the verified
finalized parent chain.
The experimental TCP block feed serves one finalized block per connection:

```text
request:  "CYB1" | NetworkID[32] | height:uint64_le
response: payload_size:uint32_le | SerializeFinalizedBlock(payload)
```

A zero response size means the requested height is unavailable. Payloads
over 32 MiB are rejected. The observer applies each received block through
`CommitFinalizedBlock`, which verifies parent, height, old validator-set
certificate, operations, and state root before changing canonical state.

## Remaining integration

This is a block-production library and a one-request transport primitive.
It is not yet a VPS daemon, a peer-discovery network, a mempool gossip
protocol, a snapshot bootstrap, or a desktop sync service. A supervised
listener, persistent operator-key loading, retry/backoff, peer limits, and
end-to-end deployment tests are required before exposing it as a public
service. The observer needs the same trusted genesis/network definition.
