# 57 — Identity Authority

Authority is a read-only metric derived from finalized account history. It is
separate from Balance and System Balance, is non-transferable, and grants no
PoA, protocol, or resource power. It does not allocate storage, bandwidth, operation
capacity, or other resources.

The current derived policy counts completed protocol age, capped qualifying
finalized activity, and a one-time contribution for voluntary Balance-to-System
Balance locks. Automatic onboarding credit gives no Authority. The metric is
informational and locally recomputed; it is not a consensus eligibility rule.

## Explicit exclusions

There is no canonical validator registry, NodeID binding, liveness accounting,
storage contribution evidence, penalty debt, reward, resource tier, resource
budget, grant, ticket, or per-I/O consensus accounting. Provider admission and
replication remain local StorageService/provider policy. Ordinary peer
failures use local disconnect, backoff, and abuse limits.
