# Open engineering and product gates

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

These are implementation and integration questions inside the active architecture,
not invitations to reintroduce superseded BFT/MailTx/StorageObject designs.

## Governing security and privacy gates

[`DATA_PROCESSING_INVENTORY.md`](DATA_PROCESSING_INVENTORY.md) records initial
code evidence. Deployment log rotation, GUI exports, processing roles and
retention decisions remain open; pool/cache bounds are not retention schedules.

[`LOGGING_RETENTION_AND_EXPORT.md`](LOGGING_RETENTION_AND_EXPORT.md) specifies
rotation/export acceptance gates. The current writer cannot be safely rotated
by renaming its live file alone; support export and live service policy remain
unimplemented/unverified respectively.

Apply [`SECURITY_GOVERNANCE.md`](SECURITY_GOVERNANCE.md) before narrowing work
to technical primitives: establish processing/metadata inventory and roles,
RGPD and NIS2 applicability, ANSSI risk scenarios, ISO control selection,
CIA acceptance criteria, continuity objectives and incident/breach response.
Assign owners and record scoped evidence; these are not completed compliance
or certification claims.

## Network and cryptographic identity

- apply the primary-source review gates in [`SECURITY_STANDARDS.md`](SECURITY_STANDARDS.md), starting with the actual X-Wing construction/vectors and current TLS requirements;
- continuing live acceptance of the ordinary headless DEV VPS/current DEVNET and compiled transport pin; the VPS cutover itself is completed as recorded in `AGENTS.md`;
- MAINNET provisioning and GUI enablement only after its actual keys, genesis and bootstrap exist.

## Network summary source gate (2026-10-09)

The three Network figures are indicative current op/min, summed reported provider
capacity and admitted encrypted bytes. DEC-290 reuses existing counters and the
storage probe, without gating display on a new audit/accounting platform. Live
peer updates/response coverage and native overlay acceptance remain open. Exact
global census, unique content accounting and independently audited byte holdings
are separate future questions, not prerequisites for the requested estimates.

## Candidate pool bounds

- bounds of the per-node candidate pool, gossip rate limits and TTL;
- re-execution policy for held candidates when the finalized base advances.

## Block depth and elapsed time after DEC-286

AccountCreate `work_epoch` is derived from finalized block height, and name
commit lifetime/reveal windows are block depths. With candidate-only automatic
block production, these impose no upper bound in elapsed time: a quiet network
can leave an onboarding work epoch valid for arbitrarily long. Before MAINNET,
measure advance-work stockpiling and Identity Sybil economics and decide whether
the challenge needs a protocol change. Preserve existing epoch validation and
name depth rules until a reviewed decision exists; never substitute local
wall-clock time into deterministic execution or restore periodic empty blocks
implicitly. Storage settlement periods retain their explicit UTC semantics.

## Application data plane

- crash/restart acceptance of the implemented encrypted Application DB, durable draft acknowledgement and loss-safe draft-to-outgoing-job handoff;
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

Storage economy (DEC-274–DEC-283) open items:

- assignment proof: how PoA attests that paid providers were network-assigned
  from finalized randomness;
- aggregation of off-chain evidence into StorageSettlement entries at the PoA;
- Sybil splitting into separate Identities: selection by payout account removed
  the free StorageId gain, but 100 Identities still reach ~95x the share of one
  large node; measure whether AccountCreate PoW prices that sufficiently before Beta;
- integration/acceptance of implemented StoragePayoutBinding verification in evidence aggregation and settlement preparation;
- lease renewal UX before expiry (renewal after expiry already works);
- Beta rate (5 CYBOU/GiB/day/replica) validation by shadow accounting;
- MiCA and French qualification of transferable CYBOU earned for storage service.

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

## Desktop product extension gates (reviewed 2026-10-08)

See `DESKTOP_BETA_ACCEPTANCE_PLAN.md` and `NETWORK_AND_ADVANCED_UX.md`.
Implemented surfaces below retain evidence/design boundaries; their presence
here is not an instruction to rebuild them.

- pending Files-to-Mail references: source retention, durable ownership,
  restart and no duplicate staging before removing Protected-only gating;
- wider network observability is outside the frozen Beta scope (DEC-289).
  Local peers are not a global census and illustrative France positions are not
  geolocation. Future storage evidence summaries need service-owned definitions,
  denominators/windows and privacy scope; no automatic remote telemetry work;
- implemented per-object protection blockers/freshness: preserve safe service-owned evidence
  without exposing provider topology or interpreting missing data as failure;
- per-object remote purge outcomes: actual available acknowledgements, privacy,
  shared references, retries and recovery interaction before stronger labels;
- implemented own-content inspector/console: preserve semantic authorization, bounded traversal,
  lock-safe output/history and redacted exports; no foreign ChunkStore browsing;
- active diagnostic/benchmark panel: bounded resources/cost and DEVNET test-build
  scope before connecting BUILD_TESTS tooling to any desktop surface.
