# 57 — Identity Authority

Status: architecture target. This document supersedes the earlier
Proof-of-Trust design while retaining the file path to avoid unnecessary link
churn.

Identity Authority is a deterministic, non-transferable anti-abuse and
network-participation value attached to AccountID.

It is not:

```text
CYBOU
System Balance
social reputation
KYC
operator rating
PoA consensus power
```

## Time

Authority uses protocol epochs derived only from finalized height:

```text
epoch = floor(finalized_height / EPOCH_BLOCKS)
```

No local clock, timezone or calendar midnight affects canonical Authority.

## Effective Authority

Conceptually:

```text
earned =
    age
  + activity
  + system_contribution
  + liveness        # only when canonical evidence exists
  + storage         # only when canonical evidence exists

effective = max(0, earned - penalties)
```

Earnings and penalties are accumulated separately. Penalty debt is not erased
when effective Authority reaches zero.

## Age

Each completed protocol epoch of Identity lifetime contributes:

```text
+1
```

Prefer deriving this from current epoch and `creation_epoch`.

## Activity

Each qualifying finalized Identity-attributable protocol operation may
contribute:

```text
+1
```

subject to an immutable per-epoch cap.

Private Mail/File actions are not visible individually. One finalized
RootPublication is one generic activity event regardless of its encrypted
application schema.

## System contribution

A voluntary finalized user-authorized:

```text
Balance -> System Balance
```

lock of `X` whole CYBOU contributes:

```text
+X
```

once.

Automatic onboarding System Balance contributes zero.

Merely holding/spending System Balance does not generate recurring Authority.

## Liveness

Target rule:

```text
union uptime of canonically bound NodeIDs >= 50% of epoch
-> +1
```

Maximum:

```text
+1 per Identity per epoch
```

regardless of how many nodes are bound.

Until canonical NodeID binding and uptime evidence exist, liveness credit is
zero.

## Storage contribution

Never reward raw chunk count.

Target metric:

```text
verified encrypted bytes × verified storage epochs
```

converted by an immutable unit.

Until canonical storage-contribution evidence exists, storage credit is zero.

## Penalties

Global Authority penalties require canonical attributable evidence.

A transport timeout is not automatically fraud.

A provably false signed storage claim receives no positive reward and incurs a
protocol-defined penalty. The target policy is that cheating costs more than
honest non-participation.

Local unauthenticated network abuse is handled by peer disconnect/backoff/ban,
not global Authority mutation.

## Authority tier

Network privileges derive from a saturating integer tier, not linear raw
Authority.

Target:

```text
tier = min(MAX_TIER, floor(log2(effective_authority + 1)))
```

Implementation must use deterministic integer bit operations, not floating
point.

## Resource domains

Authority controls generic anti-abuse limits, not Mail/File-specific consensus
quotas.

Keep three domains:

### ProtocolBudget

Canonical operation/publication work per epoch.

### StorageBudget

Maximum active remote replicated bytes attributable to the Identity.

### BandwidthBudget

PUT/GET bytes per epoch.

Every Identity has a nonzero usable base allowance and each domain has an
immutable hard ceiling.

## Governance

Authority rules are immutable for the current network.

There is no:

```text
PolicyAuthority
runtime governance
manual operator score edit
```

Any future incompatible policy change is a separate protocol/network decision.

## PoA separation

Authority never grants PoA finalization power.

Only the genesis-authorized PoA key finalizes canonical history.
