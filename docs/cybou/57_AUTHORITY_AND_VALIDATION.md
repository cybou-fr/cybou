# 57 — Identity Authority and advisory Validation

Authority is a read-only metric derived from finalized account history. It is
separate from Balance and System Balance, is non-transferable, and grants no
PoA or protocol power. It does not allocate storage, bandwidth, operation
capacity, or other resources.

The current derived policy counts completed protocol age, capped qualifying
finalized activity, and a one-time contribution for voluntary Balance-to-System
Balance locks. Automatic onboarding credit gives no Authority. The metric is
informational and locally recomputed; it is not a social score or a consensus
eligibility rule.

## Validation context

Any CYBOU full node may produce an optional signed opinion about an operation.
Recipients independently verify the signature and operation. A recipient may
label an opinion validator-qualified when its signer has Authority of at least
1,000,000 under that recipient's local view. This threshold affects only the
recipient's display and trust choice.

Validation never changes canonical state, PoA admission, finality, or storage
authorization. Only a verified PoA-finalized block is authoritative.

## Explicit exclusions

There is no canonical validator registry, NodeID binding, liveness accounting,
storage contribution evidence, penalty debt, reward, resource tier, resource
budget, grant, ticket, or per-I/O consensus accounting. Provider admission and
replication remain local StorageService/provider policy. Ordinary peer
failures use local disconnect, backoff, and abuse limits.
