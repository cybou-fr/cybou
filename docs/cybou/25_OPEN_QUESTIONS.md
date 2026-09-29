# Open engineering and product gates

These are implementation/readiness questions inside the active architecture,
not invitations to reintroduce superseded BFT/MailTx/StorageObject designs.

## Application data plane

- exact encrypted Application DB implementation and crash-recovery strategy;
- canonical private CBOR profiles for Mail, Files mutations and RecoveryBridge;
- ApplicationService scan bounds and rebuild performance;
- interruption-safe PublicationService staging/journal behavior.

## Recovery

- cross-platform RecoveryBridge vectors;
- exact historical-KEM retention bounds;
- clean-machine recovery performance from large finalized history;
- provider discovery/fan-out behavior when old placement metadata is gone.

## Storage

Decided:

```text
development = 1 remote full replica
Beta        = 2 remote full replicas
local copy  = not counted
erasure coding Beta = disabled
```

Still open:

- provider eligibility/diversity rules;
- exact health/audit challenge profile;
- retention/lease/GC semantics;
- repair cadence and retry/backoff;
- scalable discovery after small-network fan-out;
- canonical storage contribution evidence for Authority;
- physical/canonical accounting for active replicated-byte budgets.

Do not freeze a grindable deterministic provider-placement algorithm before a
mature anti-Sybil provider registry/evidence model exists.

## Identity Authority

Decided:

- immutable per-network rules;
- +1 Age per completed protocol epoch;
- capped finalized Activity;
- one-time voluntary Balance->SystemBalance contribution;
- penalty debt preserved separately;
- generic Protocol/Storage/Bandwidth budgets;
- no PoA power.

Still open:

- exact Authority numeric caps/constants;
- exact Authority tier hard maximum;
- exact Protocol/Storage/Bandwidth base/per-tier/hard-ceiling values;
- NodeID binding format and bounded registry;
- canonical liveness evidence and slot profile;
- canonical storage byte×epoch evidence;
- exact false-claim penalty constants beyond the architecture principle.

Until canonical liveness/storage evidence exists, those Authority components
remain zero.

## Future research

- signed provisional validation profile;
- whether it delivers enough value to justify added pre-finality complexity;
- erasure coding after Beta measurements.
