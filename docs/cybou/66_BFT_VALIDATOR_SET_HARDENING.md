# 66 — BFT validator-set hardening

## Consensus vs admission

CYBOU uses:

```text
BFT explicit finality
+
Operator-controlled validator admission
```

This may be described as a PoA admission model, but PoA does not replace the BFT safety state machine.

## Equal validator power

```text
ValidatorWeight = 1
```

for every active validator.

No stake weighting.

No owner-only super-validator weight.

## Validator counts

```text
1 validator
    development only

2–3 validators
    multi-node integration/fault testing
    NOT enough for f=1 classical BFT tolerance

4 validators
    minimum target for f=1
```

For the standard `n >= 3f + 1` safety model:

```text
f = 1
n >= 4
```

Therefore a pilot claiming Byzantine tolerance against one faulty/malicious validator must run at least four active validators.

## Operator approval

A validator enters the active set only through an operator-authorized state transition.

Conceptually:

```text
ValidatorAdmissionTx {
    validator_id
    consensus_public_key
    activation_height_or_epoch
    operator_authority_proof
}
```

Removal is also an explicit finalized state transition.

There is no hidden server-side allowlist that changes consensus behavior without chain-visible state.

## Reward relation

Eligible active validators split Security rewards equally under the v0.0.1 reward model.

Validator admission and reward eligibility remain separate checks.

## Emergency continuity

Normal validator admission remains an owner/operator authority.

However, mature deployment needs an explicit recovery procedure if the Operator Authority becomes permanently unavailable.

This is not community governance.

It is business-continuity engineering.

The exact emergency recovery mechanism remains open.
