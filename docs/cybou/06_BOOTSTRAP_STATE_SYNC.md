# 06 — Bootstrap, state sync and bounded history

A normal CYBOU desktop node must not require replay of every block from genesis after the network becomes old.

## Node roles

### Full desktop node

Stores:

- current verified state;
- bounded recent block history;
- finalized checkpoint chain/certificates required by the protocol;
- local wallet/identity state;
- locally assigned storage shards;
- application data/cache.

### Archive node

Optionally retains full historical data.

## Bootstrap model

```text
genesis / trusted root
    -> recent trusted/finalized checkpoint
    -> state snapshot
    -> verify canonical state commitment
    -> verify finality and validator-set trust path
    -> post-checkpoint blocks
    -> current state
```

## BFT bootstrap warning

Explicit BFT finality does not magically tell a brand-new node which current validator set is legitimate.

CYBOU therefore needs an explicit checkpoint/trust-anchor policy for bounded-history bootstrap.

Initial candidate:

- release contains a recent trusted checkpoint;
- checkpoint commits to height, block hash, state root and validator set;
- node verifies finality transitions from that point;
- snapshot bytes remain untrusted until commitments verify.

Release-signing authority and consensus-validator authority must remain separate concepts.

## Snapshot sources

Snapshot transport can come from any peer, archive node, removable media or multiple mirrors.

Acceptance depends on cryptographic verification, not the reputation of the source.
