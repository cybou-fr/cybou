# 74 — Authority block production and verified block feed

## Current implementation

`CybouAuthorityNode` provides the first canonical single-validator block
production path. It accepts up to 256 pending operations, checks the entire
candidate batch against the current state, builds a block using the same
executor as `CybouStateStore`, obtains a 1/1 BFT certificate, and commits the
finalized block atomically. The queue clears only after a successful commit.

An operator-authorized transition block can add validators while N=1. The
N=1 producer supports such a transition when the network definition contains
an Operator Authority; the current standalone DEV profile does not. The
old validator finalizes that block; the next height uses the new set. The
single-validator producer refuses to continue once N is no longer 1.

`CybouStateStore` indexes finalized blocks by height as well as block ID.
For older local databases without height keys, reads walk the verified
finalized parent chain.
The experimental TCP endpoint handles one bounded request per connection.
Block retrieval uses:

```text
request:  "CYB1" | NetworkID[32] | height:uint64_le
response: payload_size:uint32_le | SerializeFinalizedBlock(payload)
```

A zero response size means the requested height is unavailable. Payloads
over 32 MiB are rejected. The observer applies each received block through
`CommitFinalizedBlock`, which verifies parent, height, old validator-set
certificate, operations, and state root before changing canonical state.

Remote operation submission uses a separate `CYBO` request carrying NetworkID
and one serialized `ProtocolOperationV1` (maximum 128 KiB). The producer
deserializes and candidate-validates the operation, computes OperationID =
SHA256("CYBOU/OP_ID/V1" || serialized_op), and returns a 33-byte structured
reply: a 1-byte status (`ACCEPTED`, `ALREADY_PENDING`, `ALREADY_FINALIZED`,
`INVALID_PAYLOAD`, `NETWORK_MISMATCH`, or `REJECTED`) and the 32-byte
OperationID.

The desktop identity service saves keystore material durably to disk *before*
PoW and network broadcast using atomic replace and crash-safe `.bak` rotation,
closing the window where an account could be created on-chain while the private
key is lost locally. The service waits for block finality before reporting activation.

## Remaining integration

The standalone DEV process using this library is documented in
`75_DEV_NODE_RUNBOOK.md`. The Qt desktop follows verified blocks through
`CybouNodeRuntime`. Peer discovery, authenticated transport, mempool gossip,
snapshot bootstrap, and independent multi-validator operation still need
implementation. The observer needs the same trusted genesis/network
definition; a bootstrap endpoint is not a trust source.
