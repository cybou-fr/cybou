# 06 — Bootstrap and verified state sync

## Node roles

Every node validates the same canonical chain. The genesis-bound PoA signer
orders and finalizes blocks; it does not replace full-node validation. Ordinary
desktop nodes may prune old block bodies after retaining the state and evidence
required by the active product and protocol.

## Initial synchronization

Network existence, initial claim, and network replacement are defined by
[`04_NETWORK_BOOTSTRAP_AND_GENESIS.md`](04_NETWORK_BOOTSTRAP_AND_GENESIS.md).
This document covers syncing after a network definition has been authenticated
and installed; a peer or successful sync connection is not itself the network
trust anchor.

The initial implementation synchronizes finalized blocks from the network
definition's genesis in height order:

```text
trusted network definition
-> genesis state and genesis block ID
-> PoA certificate verification for each next block
-> deterministic operation execution
-> resulting state-root verification
-> canonical finalized state
```

Peers are untrusted data sources. A peer's reputation, claimed height, or
snapshot is never a substitute for local signature, parent, operation, and
state-root verification. The genesis definition is the trust anchor and binds
the PoA finalizer key.

## Pruning and snapshots

Desktop nodes may prune old block bodies. The active implementation does not
define checkpoint trust, snapshot import, or a bounded-history fast-sync format.
Do not add a snapshot decoder or compatibility path until its trust anchor,
state commitment, PoA evidence, and clean-machine recovery format are specified
together. Archive retention is an operator choice.

## Product data recovery

Mail and Files content is recovered from finalized RootPublication records and
their admitted encrypted chunks. Local Inbox/Sent/read-state indexes and the
private Files catalog are rebuilt after decryption; they are not consensus
state. Clean-machine recovery requirements are defined in
`IDENTITY_DISCOVERY_AND_RECOVERY.md` and `ENCRYPTED_CHUNK_TREE.md`.
