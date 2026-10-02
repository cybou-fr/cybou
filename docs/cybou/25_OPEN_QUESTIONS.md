# Open engineering and product gates

These are implementation and integration questions inside the active architecture,
not invitations to reintroduce superseded BFT/MailTx/StorageObject designs.

## Network and cryptographic identity

- coordinated DEV VPS cutover to ordinary `cybou-node` on the re-provisioned DEVNET;
- automatic dialing of the compiled bootstrap locator (with its TLS SPKI pin) by nodes and the desktop;
- multi-process LAB/CI networks: without external network files, a LAB finalizer needs the DEVNET PoA key;
- MAINNET provisioning and GUI enablement only after its actual keys, genesis and bootstrap exist.

## AUTH and Validation

- exact wire encoding of `PoaAuthAdjustment` and `ValidationAttestation`;
- bounds of the per-node candidate pool and Validation store, gossip rate limits and TTL;
- re-execution policy for held candidates and signatures when the finalized base advances;
- AUTH penalty table for verifiable invalid Validation (not frozen).

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
