# Canonical chain state

Full nodes execute finalized blocks deterministically and derive the same state
root. State is account-level and application-neutral; Mail and Files do not
create permanent per-message or per-file records.

## State domains

- Monetary accounts: spendable balance and service `SystemBalance`.
- Identity registry: stable AccountID, current hybrid recovery and
  authorization capabilities, current KEM commitment, shared nonce, and
  `key_epoch`.
- `.cybou` name registry: finalized commit/work/reveal ownership records.
- Economic pools and deterministic generic publication accounting.
- Network parameters bound by the immutable network definition.

The exact Identity, Name, and monetary encodings are owned by their active
specifications. Application schemas and content metadata remain encrypted in
the chunk DAG and are indexed locally by each client.

## Block execution

For each candidate block, a node validates canonical operation bytes,
authorization, replay protection, resource bounds, and fees against a
candidate state. It derives the state root and accepts the block only when the
genesis-bound hybrid-PQ PoA finality proof is valid. Full nodes independently
re-execute the block and compare the state root.

The PoA trust model is centralized. Durable anti-equivocation and deterministic
fork handling are cutover requirements. See `POA_FINALITY.md`.

## RootPublication

RootPublication is the only operation that publishes application content. It
commits to a generic sorted set of opaque ChunkIDs and stored sizes, a Merkle
root, a root chunk, and recipient KEM capsules. State validation recomputes the
Merkle root and aggregate count/bytes; it does not interpret Mail or Files
schemas.

Finalized blocks retain canonical operation history for proof and discovery.
Clients rebuild Inbox, Sent, Files, and other indexes from finalized
publications and locally decrypted content. These indexes are not consensus
state.

## Invariants

- No per-Mail or per-file permanent state object.
- No plaintext name, recipient, MIME type, path, or graph topology in consensus.
- System Balance is a service budget and never changes PoA signing weight.
- Consensus arithmetic and fee routing use bounded integer operations.
- No local wall-clock input affects state transitions.
