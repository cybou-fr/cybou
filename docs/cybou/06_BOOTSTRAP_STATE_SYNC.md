# 06 — Bootstrap and verified state sync

## Node roles

Every node validates the same canonical chain. The Central Authority PoA finalizer
executes candidates independently and signs finalized blocks; it does not replace
full-node validation. Ordinary desktop nodes may prune old block bodies after
retaining the state and evidence required by the active product and protocol.

## Initial synchronization

Network profiles (DEVNET, MAINNET), Network Public Key (`NetworkID`), and
network replacement are defined in [`04_NETWORK_LIFECYCLE.md`](04_NETWORK_LIFECYCLE.md).

Bootstrap is an ordinary CYBOU full peer with a known IP:port and TLS SPKI pin.
It is a trusted initial discovery peer, but is NOT trusted to define canonical
truth or replace cryptographic verification.

Synchronization proceeds from the compiled Network Public Key and pinned GenesisDigest:

```text
load official signed genesis bundle (CYG1)
-> verify Network Public Key (NetworkID)
-> verify GenesisDigest matches pinned official profile
-> verify offline Network Key signature over genesis specification
-> verify hash(genesis state) == genesis_state_root
-> initialize local consensus state and genesis block ID
-> connect to ordinary bootstrap peer (or known peers) via standard CYP2 protocol
-> synchronize PoA-finalized blocks in height order
-> independently execute operations for each block
-> recompute state root and verify against block commitment
-> verify PoA certificate over BlockID
-> canonical finalized state achieved
```

Peers are untrusted data sources. A peer's reputation, claimed height, or
snapshot is never a substitute for local signature, parent, operation, and
state-root verification. The compiled Network Public Key authenticates genesis;
genesis defines the network, initial state, initial Authority baselines, and PoA key.

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
