# AGENTS.md — CYBOU implementation authority

Read the active CYBOU documents before coding. Git history records superseded
architecture; do not keep obsolete runtime paths alive for compatibility.

## DEV VPS deployment — migration state

The current DEV installation is a legacy topology. It is not the frozen target
architecture in `docs/cybou/04_NETWORK_BOOTSTRAP_AND_GENESIS.md`. Do not treat
its finalizer endpoint as the future bootstrap, or its two providers as part
of the official VPS target. The target is one bootstrap service on the VPS and
the genesis-key holder finalizing from its desktop. The migration is not
implemented yet; keep the current network operational until the specified
acceptance tests and coordinated DEV cutover are complete.

- After changing CYBOU core or `cybou-node`, run relevant tests, rebuild
  `cybou-node` on the DEV VPS, and restart `cybou-node.service` in the same
  task.
- Connect as `debian@vps-d0669a91.vps.ovh.net`; checkout:
  `/home/debian/cybou`; service binary:
  `/home/debian/cybou/build/bin/cybou-node`.
- Preserve the current executable for rollback, restart with systemd, then
  verify service health, listening port, and advancing finalized height.
- Do not reset DEV state or replace the PoA key during a routine deployment.
- Current legacy topology: `cybou-node.service` is the VPS PoA finalizer on
  port 29461; `cybou-provider-1.service` and `cybou-provider-2.service` are
  independent storage providers (`cybou-node provider run`, P2P ports
  29471/29481, state in `/var/lib/cybou/provider-N-db`). Restart all three
  after a rebuild and verify the providers follow finalized height.

## Identity

- One protocol Identity is account-level. Device is not a protocol entity.
- Stable random AccountID is independent from mnemonic and keys.
- Recovery: Ed25519 + ML-DSA-65. Authorization: Ed25519 + ML-DSA-44.
- Identity KEM uses its own mnemonic-derived role; do not reuse signing keys.
- Portable CYBV2/CVID5 vault stores stable AccountID plus recovery entropy.
- IdentityRotate atomically replaces Recovery, Authorization, and KEM roles.
- No device registry, primary-device concept, session authorization, identity
  transfer, DeviceAdd, or DeviceRevoke.
- `.cybou` names use finalized commit/work/reveal and the active name rules.

## Finality

- Finality is genesis-bound single-operator hybrid-PQ PoA.
- Target operation puts the genesis-key holder on its Central Authority
  desktop; the official VPS bootstrap never finalizes. See
  `docs/cybou/04_NETWORK_BOOTSTRAP_AND_GENESIS.md`.
- Full nodes independently validate every operation, block transition, state
  root, and PoA certificate.
- PoA is centralized finality, not BFT.
- Durable signing journal and equivocation conflict halt must fail closed.
- Authority and advisory Validation never grant PoA finalization power.
- The PoA finalizer independently executes operations and ignores Validation
  for correctness.

## Application content

- `RootPublication` is the only application-content protocol operation.
- Mail, Files, filenames, folders, recipients, and application schemas remain
  encrypted application data.
- ChunkID is full BLAKE3-256 of exact stored encrypted bytes.
- The current storage protocol admits chunks remotely only after a finalized
  RootPublication authorizes them by Merkle proof. Application publication
  remains local until finality.
- A RootPublication may locally bundle multiple encrypted content trees under
  one authorization root; this is an application implementation pattern, not
  a new wire entity.
- Recoverable owner content requires an application-layer self capsule.
- Mail and Files share the same application publication/storage substrate.

## Application data plane

- The common ChunkStore is a network store of encrypted bytes keyed by ChunkID.
  It has no user-facing own/foreign classification.
- GUI/pages never enumerate the common ChunkStore or provider DB.
- Each unlocked Identity uses a separate encrypted rebuildable Application DB
  for Mail/Files semantic state.
- Publication discovery indexes only publications whose capsules the Identity
  can open.
- Core services are intentionally few: `ApplicationService`,
  `PublicationService`, and `StorageService`.
- The GUI consumes semantic model data, never provider storage topology.

## Storage durability

- Development target: 1 remote full replica per required chunk.
- Beta target: 2 independent remote full replicas per required chunk.
- DEV may run extra providers for failover, repair, and soak; they do not raise
  the durability target.
- Local encrypted content is useful cache/staging but does not count toward
  remote durability. Beta uses full replication; erasure coding is disabled.
- Placement, provider selection, health, audit, and repair are StorageService
  concerns, not consensus state.
- The current/default provider policy is finality-first. A future provider may
  independently choose to offer provisional resources under local trust policy
  (including optional advisory Validation), at its own risk. Such resources
  create no canonical right or durability guarantee and are not Protected.
  This provisional path is not implemented. Do not add canonical per-I/O
  accounting or resource tickets.
- A file/message is not `Protected`/`Sent` merely because its RootPublication
  is finalized.

## Authority and Validation

- Authority is an informational, read-only metric derived from finalized
  history. It is separate from Balance and System Balance and grants no
  protocol, resource-allocation, or PoA power.
- There is no canonical ValidatorSet, validator registry, NodeID binding,
  liveness/storage evidence, reward/penalty system, resource budget,
  reservation, grant, ticket, or per-I/O accounting.
- Any full node may produce optional signed advisory Validation. Recipients
  verify the claim and decide locally whether to trust it or act before
  finality.
- A recipient may label an opinion validator-qualified when the signer has
  Authority >= 1,000,000 in that recipient's local view. This is presentation
  policy, never protocol admission.
- `network validated` is never equivalent to `PoA-finalized`. Only PoA finality
  advances canonical state and authorizes remote chunk admission.
- Local peer failures use local disconnect, backoff, and abuse limits; they do
  not change global Authority.

## Economics

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
4 fee units -> 3 Security + 1 Onboarding
```

Balance is spendable. System Balance is an irreversible service budget.
Authority is non-transferable and informational.
