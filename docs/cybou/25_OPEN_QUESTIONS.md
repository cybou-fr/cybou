# Open engineering and product gates

These are implementation and integration questions inside the active architecture,
not invitations to reintroduce superseded BFT/MailTx/StorageObject designs.

## Network and cryptographic identity

- one-time DEVNET provisioning procedure under gitignored `/private/`, including the Network and ordinary `cybou.cybou` Identity secret material;
- generated public C++ representation of Network Public Key, immutable signed `NetworkGenesis`, initial state, `cybou.cybou` public Identity data and bootstrap locators;
- retirement of official CYG1/CYN1 file paths, `--network` file startup and separate profile `GenesisDigest` pins;
- migration of SHA-256 definition-hash usage in runtime, DBs and wire messages to exact canonical Network Public Key bytes;
- removal of legacy bootstrap subsystem and coordinated DEV VPS cutover to ordinary `cybou-node`;
- MAINNET provisioning and GUI enablement only after its actual keys, genesis and bootstrap exist.

## Validation and provisional lifecycle

- exact payload format for `ValidationAttestation` signed with Identity Authorization Key;
- peer-to-peer gossip propagation limits, cache TTL, and rate limiting for active attestations;
- atomic rollback mechanism in `NodeRuntime` for clearing provisional effects upon PoA conflict;
- provisional chunk cache eviction semantics on storage providers if candidate publication is rejected.

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
