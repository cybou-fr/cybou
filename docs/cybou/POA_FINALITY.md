# PoA finality

Status: CURRENT
Scope: Block/state/PoA source audit at ee9d721a, 2026-10-09; existing regression references are not a fresh suite run or independent interoperability acceptance.

Recorded status: **Active PoA protocol**. Finality is single-operator hybrid-PQ PoA
under the genesis-authorized operational key `P`. It is not BFT and has no
ValidatorSet, vote, quorum or staking weight.

## Operational model

Genesis defines the network, initial state, initial CYBOU allocations, and the
authorized PoA public key `P`.
The Central Authority executes candidate operations independently from its
desktop, trusts no peer state, and publishes finalized blocks.

Every full node independently checks certificate signatures, height, parent,
operation execution and deterministic state root. The current block has no
creation timestamp; local arrival time cannot prove production time or global
freshness. A signature alone
never validates an invalid transition.

## Certificate

PoA requires both Ed25519 and ML-DSA-65. There is no classical-only fallback.
The certificate binds:

```text
NetworkBinding || BlockID || height_u64le || parent_block_id ||
Ed25519_sig || ML-DSA-65_sig
```

This is the certificate wire layout, exactly 3477 bytes, with no signature
length prefix. Integers are little-endian and IDs use raw byte order. The signed
digest is SHA-256 of `"CYBOU/POA-FINALITY" || NetworkBinding || BlockID ||
height_u64le || parent_block_id`; signature bytes are not part of that digest.
The signatures are 64 and 3,309 bytes respectively. The verifier checks both
signatures against the genesis-authorized PoA key `P`. The PoA key role remains
separate from Identity Recovery, Authorization, KEM, Network Private Key,
Release Signing and Treasury.

## Signing journal and conflict halt

The finalizer uses a journal namespace keyed by `NetworkBinding`. Its metadata
binds NetworkBinding, PoA key ID and genesis anchor; its durable head stores
height, parent and intended BlockID before signing. It must fail closed
if journal recovery, rollback or signer exclusivity is uncertain. One operational
key has only one active signer.

If two distinct valid certificates exist for the same network, height and
parent, observing full nodes record verified equivocation evidence and
deterministically select the canonical winner min(BlockID_1, BlockID_2),
re-orging if the newly observed certificate has a strictly smaller BlockID,
or rejecting it if not. Corrupt storage halts fail closed.

Current StateStore replacement is limited to a competing block at the current
head height with the same parent. It re-executes that block over the stored
parent snapshot and replaces losing operation/event indexes. A conflict below
the current head is not replayed through descendants by this path: it returns
an invalid-height result. General historical conflict recovery remains an
implementation gap, not a completed arbitrary-depth reorganization mechanism.

## Independent execution invariant

```text
PoA MUST independently execute candidate operations.

operation -> own validation/execution -> state root -> block -> PoA signature
```

PoA produces blocks from its node's ordinary candidate pool, and only when that
pool holds a candidate (DEC-286): an idle network does not grow its chain. An
explicit operator "finalize one block" may still produce an empty block. There is no
validator, fork-choice vote, BFT voting or quorum override, and no provisional
state for PoA finality to roll back.

## Genesis and signer boundary

Runtime and StateStore accept VerifiedNetworkGenesis, not an unsigned network
summary. The genesis specification digest verified by its network signature
is height-zero finalized tip, first-block parent and durable journal anchor.
CybouNodeRuntime executes candidates and constructs the final block;
PoaFinalizer checks the genesis key, persists intent and signs. No producer
wrapper, dedicated transport role or special pending state exists. A signer
whose journal conflicts with the selected canonical history fails closed.

The production worker remains alive through transient errors and signer locks.
Retry uses bounded backoff and preserves the exact journaled candidate and any
issued certificate until commit. Safety failures disable the signer and cannot
be cleared by another unlock. Locking the user Vault leaves an already active local PoA signer and production
loop running in the background. It does not pause finalization. Explicit pause
stops the production loop; signer safety rules remain unchanged. Operator controls
require the unlocked authorized Identity. Unlocking does not restart the application
or reconnect peers. Failed durable intent writes
fail closed because their persistence outcome is uncertain.

## Current block bytes and commitments

The canonical block wire order is parent BlockID[32], height u64 LE, resulting
state root[32], operation count u32 LE, then for each operation its byte length
u32 LE and exact canonical operation bytes. The fixed part is 76 bytes. No block
creation timestamp, version discriminator, PoW nonce or operations-root field
is serialized there. The operations root is recomputed for the BlockID header.

For each operation let `H_i = SHA-256(exact serialized operation bytes)`.
The ordered commitment is SHA-256 of `"CYBOU/OPS-ROOT" || count_u32le ||
H_1 || ... || H_n`, including the count for an empty list. It is a flat ordered
commitment, not a Merkle tree. These `H_i` are not the domain-separated
OperationIDs used by the operation index (`"CYBOU/OP-ID"`).
BlockID is SHA-256 of `"CYBOU/BLOCK" || parent || height_u64le ||
operations_root || resulting_state_root`.

A finalized-block envelope is `block_length_u32le || block_bytes ||
certificate_length_u32le || certificate_bytes`. Decoders reject malformed
operations, out-of-input lengths/counts, certificates of another size and
trailing bytes. Parsing is not transition acceptance: certificate binding,
height/parent, independent execution, state root and conservation are checked
by StateStore/executor. The local finalizer rejects serialized blocks exceeding
32 MiB; this does not imply every codec entry point enforces that same size bound.

## Defining sources and evidence limits

[block.cpp](../../src/cybou/block.cpp),
[poa_finality.cpp](../../src/cybou/poa_finality.cpp),
[poa_finalizer.cpp](../../src/cybou/poa_finalizer.cpp),
[poa_signing_journal.cpp](../../src/cybou/poa_signing_journal.cpp),
[poa_conflict_detector.cpp](../../src/cybou/poa_conflict_detector.cpp), and
[state_store.cpp](../../src/cybou/state_store.cpp) define these boundaries.
Existing [PoA regressions](../../src/test/cybou_poa_tests.cpp) include golden
finality/key/encoding vectors, same-intent restart, parent/history mismatch and
durable conflict evidence. [Runtime regressions](../../src/test/cybou_node_runtime_tests.cpp)
cover observer execution, current-head equivocation replacement and removal of
losing event coordinates. These are component evidence, not an independent
implementation or assurance that a complete valid journal/database rollback
can always be detected without an external durable anchor. Operationally retain
one signer and its history; an unlock never authorizes journal deletion.
