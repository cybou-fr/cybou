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
- measured provider independence for the two-replica Beta target.

Provider placement remains local StorageService policy. Do not freeze a
deterministic provider-ranking algorithm before provider independence can be
measured reliably.

## Authority and Validation

Authority is informational and read-only. It allocates no resources and creates
no rewards or penalties. Recipients may label a signed Validation as
validator-qualified when its signer has Authority >= 1,000,000 in their local
view. This is not protocol admission.

No canonical NodeID binding, validator registry, liveness/storage evidence,
resource budget, reservation, grant, ticket, reward, or penalty mechanism is
planned. The remaining product question is how much optional Validation
evidence recipients want to display or trust before PoA finality.

## Future research

- [`future/VALIDATION.md`](future/VALIDATION.md) defines optional advisory evidence; the
  future question is whether users need it before finality and how to present
  local trust choices;
- emergency Authority recovery key if the current active Authority private key is lost or compromised.
