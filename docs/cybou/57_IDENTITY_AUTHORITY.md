# 57 — Identity Authority

Authority is a deterministic property derived exclusively from PoA-finalized history
and state. It is separate from Balance and System Balance, is non-transferable, and
grants NO PoA finalization power, NO consensus voting rights, and NO resource allocations.

## Genesis Authority baseline and computation

Genesis defines the initial Authority baseline for designated ordinary Identities
(e.g., DEV bootstrap Identity initial Authority = 1,000,001).

Beyond genesis, Authority develops deterministically from PoA-finalized history:
- completed protocol age;
- capped qualifying finalized activity;
- voluntary Balance-to-System Balance lock contributions.

Automatic onboarding credit gives no Authority.

## Protocol effect: Validation eligibility

Authority confers exactly ONE protocol eligibility:
```text
validation_eligible(identity) :=
    authority_from_latest_PoA_finalized_state(identity) > 1,000,000
```

An Identity with finalized `Authority > 1,000,000` is qualified to sign advisory
Validation attestations for candidate operations.

Crucially:
- eligibility is evaluated strictly against the latest **PoA-finalized** state;
- provisional state never elevates an Identity's Authority or makes it eligible;
- Authority never grants PoA finalization power, block signing rights, or quorum weight.

## Explicit exclusions

There is no canonical validator registry, ValidatorSet, NodeID binding, liveness accounting,
storage contribution evidence, penalty debt, reward, resource tier, resource budget,
grant, ticket, or per-I/O consensus accounting. Provider admission and replication remain
local StorageService/provider policy. Ordinary peer failures use local disconnect,
backoff, and abuse limits.
