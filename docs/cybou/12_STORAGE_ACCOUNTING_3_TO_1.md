# 12 — Storage accounting and the 3:1 policy

## Meaning of 3:1

The target policy is:

```text
3 units verified effective network contribution
    -> up to 1 unit logical storage entitlement
```

This is **not** "three complete replicas".

Physical erasure-coding overhead and global capacity solvency are separate concepts.

## Never trust configured capacity

This does not count:

```text
user selected: "provide 1 TB"
```

Only storage that the network can verify contributes to entitlement.

## Capacity terms

Use distinct metrics:

```text
ProvisionedCapacity   user-configured upper bound
StoredCapacity        bytes currently assigned to the node
VerifiedCapacity      capacity proven by successful audits/challenges
FreeReservedCapacity  verified/controlled free contribution reserve
EffectiveCapacity     risk-adjusted capacity accepted by accounting
```

Do not compute contribution purely from free disk space.

## Byte-time accounting

Contribution must include time.

Conceptual unit:

```text
verified byte-hours
```

Entitlement is based on a rolling window, not lifetime accumulation.

Candidate concept:

```text
EffectiveContribution =
    rolling_verified_byte_hours
    * reliability factors

StorageEntitlement =
    EffectiveContribution / 3
```

Exact window/decay constants are not frozen.

## Network solvency invariant

New logical commitments are admitted only if the network remains above the configured safety ratio.

Conceptually:

```text
LogicalCommitted <= EffectivePhysicalCapacity * SafetyFactor / 3
```

`SafetyFactor < 1` reserves emergency capacity.

## If a node disappears

Do not immediately delete the owner's existing backups.

State progression:

```text
HEALTHY
    -> DEGRADED
    -> grace period
    -> new writes limited/blocked
    -> restore remains available while data exists
```

A future `contribute-or-pay` policy may allow CYBOU payments to compensate for insufficient contribution.

## Separate invariants

Do not confuse:

1. capacity solvency — enough global verified physical capacity exists;
2. placement diversity — shards are spread across failure domains;
3. reconstruction safety — enough shards remain to recover data;
4. repair reserve — enough free capacity exists to repair after failures.
