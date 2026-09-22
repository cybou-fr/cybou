# 13 — Storage proofs, audits and repair

Storage reliability has several distinct problems.

## Proof of possession

Can a node prove it currently possesses assigned shard data?

## Proof/certification of capacity

Can the network distinguish genuinely available disk from a false capacity claim?

## Sybil resistance

Can one operator cheaply pretend to be many independent failure domains?

These must not be collapsed into a single word "audit".

## Lease concept

A storage lease is a technical P2P obligation, not a financial smart contract.

Conceptually:

```text
LeaseID
Object/Shard identifier
Holder NodeID
expiry/epoch
proof/challenge parameters
```

Leases need not be individually written to the blockchain.

## Accounting aggregation

Millions of shard events must not become millions of chain records.

Storage layer periodically produces an aggregated accounting commitment, e.g. a Merkle/state root, that becomes consensus state.

## Repair

When redundancy drops below policy:

```text
detect unhealthy shard set
    -> choose healthy source shards
    -> reconstruct
    -> place replacement shards
    -> verify
    -> update lease/accounting state
```

Repair must preserve failure-domain diversity.

## Placement dimensions

When information is available, avoid placing correlated shards on the same:

- NodeID;
- physical node;
- operator identity;
- disk/failure domain;
- network/ASN/geographic domain as required by policy.

Exact privacy-preserving failure-domain discovery remains open.

## Research blocker

The final proof system must be evaluated for:

- cheating by on-demand fetch;
- replay;
- outsourced proof generation;
- bandwidth cost;
- disk I/O cost;
- mobile/laptop sleep behavior;
- false slashing from temporary connectivity loss.
