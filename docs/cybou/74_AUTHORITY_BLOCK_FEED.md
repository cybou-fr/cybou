> Historical/current-DEV scope: this document describes the BFT, MailTx, validator-set, or indexed-object protocol currently running on DEV. It is superseded for the next-gen target by `POA_FINALITY.md`, `ENCRYPTED_CHUNK_DAG.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, and `IDENTITY_DISCOVERY_AND_RECOVERY.md`. Product and UX requirements remain applicable only where they do not conflict with those target documents. No cutover is active yet.
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
and one serialized `ProtocolOperation` (maximum 128 KiB). The producer
deserializes and candidate-validates the operation, computes OperationID =
SHA256("CYBOU/OP_ID/V1" || serialized_op), and returns a 33-byte structured
reply: a 1-byte status (`ACCEPTED`, `ALREADY_PENDING`, `ALREADY_FINALIZED`,
`INVALID_PAYLOAD`, `NETWORK_MISMATCH`, or `REJECTED`) and the 32-byte
OperationID.
On the client, a transport failure after the request write begins is marked
`delivery_uncertain`; it is distinct from an explicit `REJECTED` reply and is
local metadata, not an additional wire status. Callers must reconcile an
uncertain operation by its OperationID instead of assuming that its nonce is
free for reuse.

The DEV authority publishes its CYP2 listener at `51.255.46.58:29461` alongside
the legacy block-feed port `29460`. The Qt desktop uses CYP2 by default; the
CYB1 block feed remains an explicit diagnostic fallback.

The desktop identity service saves keystore material durably to disk *before*
PoW and network broadcast using atomic replace and crash-safe `.bak` rotation,
closing the window where an account could be created on-chain while the private
key is lost locally. The service waits for block finality before reporting activation.

## Remaining integration

The standalone DEV process using this library is documented in
`75_DEV_NODE_RUNBOOK.md`. The Qt desktop follows verified blocks through
`CybouNodeRuntime`; it uses the published CYP2 seed by default, maintains
multiple outbound sessions, exchanges bounded peer-discovery hints, syncs
verified finalized blocks, and fans out recent blocks and admitted operations.
The bounded CYB1 endpoint remains available as an explicit diagnostic fallback.

CYP2 discovery and gossip are initial bounded implementations, not
authenticated transport: peer addresses remain untrusted routing hints, and
consensus trust comes from the locally provisioned network definition and
verification of every finalized block. Snapshot bootstrap is not implemented.
The four-process BFT smoke exercises local consensus and recovery, but
independently operated multi-validator deployments remain unverified. The
observer needs the same trusted genesis/network definition; a bootstrap
endpoint is not a trust source.
