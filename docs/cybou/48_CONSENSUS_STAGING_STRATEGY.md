# 48 — Consensus staging strategy

CYBOU freezes **BFT explicit finality** as the consensus direction.

Validator admission remains a separate, replaceable policy.

## Stage 0 — Operator validator / development

```text
1 Operator Validator
```

Purpose:

- network quarantine;
- deterministic CYBOU state;
- identity;
- Balance/System Balance;
- Proof of Trust;
- Email vertical slice.

This stage is not decentralized and provides no BFT fault tolerance.

## Stage 1 — Static multi-validator BFT

Purpose:

- implement the real BFT safety machine;
- test finality under faults;
- support controlled French pilots.

Properties:

```text
known validator keys
formal rounds / locking / finality
deterministic validator-set state
no requirement for stake
```

## Stage 2 — Resilient production validator set

CYBOU adds independent validator operators so that the service does not depend on one CYBOU-operated machine or cloud.

The CYBOU Operator Validator remains a normal production validator.

The exact admission model can evolve independently:

```text
static/federated
stake-weighted
hybrid
other reviewed admission policy
```

## Stage 3 — Admission policy evolution

Only after operational evidence should CYBOU decide whether stake adds meaningful security.

The BFT core remains the same safety domain.

## Product gates

```text
Email Alpha               -> Stage 0 allowed
controlled pilot          -> Stage 1 minimum
"survives operator loss"  -> Stage 2 required
```

## Core invariant

Proof of Trust, System Balance and commercial ownership do not automatically determine BFT voting power.
