# 07 — BFT consensus core

## Frozen direction

CYBOU uses:

```text
BFT explicit finality
+
operator-controlled validator admission
```

This is a permissioned BFT network with a PoA-style admission policy.

## Equal validator weight

v1:

```text
ValidatorWeight = 1
```

for every active validator.

No stake-weighted consensus power.

## Topology stages

```text
1 validator:
    development

2–3:
    integration/fault testing

4+:
    minimum target to tolerate f=1
    under n >= 3f + 1 assumptions
```

A deployment must not claim tolerance of one Byzantine validator with only three validators.

## BFT state machine

Must formally define:

- height;
- round;
- validator-set version;
- proposer selection;
- proposal validation;
- prevote/precommit or equivalent;
- lock/unlock;
- quorum;
- timeout/round change;
- finality certificate;
- validator-set transitions;
- restart persistence;
- equivocation evidence.

## Safety invariant

Two honest nodes must never finalize different blocks at the same height.

## Admission

Validator admission/removal requires an explicit operator-authorized protocol state transition.

Operator approval decides who may enter the set.

Once active, a validator's vote is handled by deterministic BFT rules.

## Simulator

Test at least:

- 4 validators for f=1 scenarios;
- delay/drop;
- partitions;
- crash/restart;
- conflicting votes;
- invalid proposal;
- timeout/round change;
- validator-set transition;
- operator-authority admission changes.
