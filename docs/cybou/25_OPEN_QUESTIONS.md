# Open engineering and product gates

These are implementation and integration questions inside the active architecture,
not invitations to reintroduce superseded BFT/MailTx/StorageObject designs.

## Governing security and privacy gates

Apply [`SECURITY_GOVERNANCE.md`](SECURITY_GOVERNANCE.md) before narrowing work
to technical primitives: establish processing/metadata inventory and roles,
RGPD and NIS2 applicability, ANSSI risk scenarios, ISO control selection,
CIA acceptance criteria, continuity objectives and incident/breach response.
Assign owners and record scoped evidence; these are not completed compliance
or certification claims.

## Network and cryptographic identity

- apply the primary-source review gates in [`SECURITY_STANDARDS.md`](SECURITY_STANDARDS.md), starting with the actual X-Wing construction/vectors and current TLS requirements;
- coordinated DEV VPS cutover to an ordinary headless `cybou` node on the re-provisioned DEVNET, serving a TLS certificate that matches the compiled SPKI pin (or a re-pinned locator);
- MAINNET provisioning and GUI enablement only after its actual keys, genesis and bootstrap exist.

## AUTH and Validation

- exact wire encoding of `PoaAuthAdjustment` and `ValidationAttestation`;
- bounds of the per-node candidate pool and Validation store, gossip rate limits and TTL;
- re-execution policy for held candidates and signatures when the finalized base advances;
- AUTH penalty table for verifiable invalid Validation (not frozen).

## Application data plane

- exact encrypted Application DB implementation and crash-recovery strategy;
- cross-implementation vectors for typed binary private profiles for Mail, Files mutations and RecoveryBridge;
- ApplicationService scan bounds and rebuild performance;
- interruption-safe PublicationService staging/journal behavior.

## Recovery

- cross-platform RecoveryBridge vectors;
- exact historical-KEM retention bounds;
- clean-machine recovery performance from large finalized history;
- provider discovery/fan-out behavior when old placement metadata is gone.

## Storage durability

Decided:

```text
development = 1 remote full replica
Beta        = 2 remote full replicas (plus local copy = 3 physical copies total)
erasure coding Beta = disabled
```

Still open:

- provider eligibility/diversity rules;
- exact health/audit challenge profile;
- retention/lease/GC semantics;
- repair cadence and retry/backoff;
- scalable discovery after small-network fan-out;
- measured provider independence for the two-replica Beta target.

## Data assurance and erasure assessment

[`DATA_ASSURANCE_AND_ERASURE.md`](DATA_ASSURANCE_AND_ERASURE.md) records the
current evidence limits and a proposed recovery/erasure design. It does not
change frozen protocol decisions. Gates before implementing stronger claims:

- reconcile mnemonic/self-capsule recovery with per-object erasure, including
  retained envelopes, historical KEM seeds, recovery bridges and old backups;
- specify recovery-store durability and rollback handling without treating a
  compliant client's refusal to decrypt as cryptographic destruction;
- distinguish remote admission ACKs, signed obligations, recent full-chunk
  checks and physical/administrative replica independence;
- verify managed purge under blob deletion failure, crashes and shared chunks;
- justify any canonical storage obligation register by concrete accounting
  transitions; commitments alone do not prove physical 1:3 contribution.
