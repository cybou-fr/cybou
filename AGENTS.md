# AGENTS.md — CYBOU implementation authority

Read the active CYBOU documents before coding. Git history is the record of
superseded architecture; do not keep obsolete runtime paths alive for
compatibility.

## DEV VPS deployment

- After changing CYBOU core or `cybou-node`, rebuild `cybou-node` on the DEV VPS
  and restart `cybou-node.service` in the same task.
- Connect as `debian@vps-d0669a91.vps.ovh.net`; checkout:
  `/home/debian/cybou`; service binary:
  `/home/debian/cybou/build/bin/cybou-node`.
- Run relevant tests before deployment, preserve the current executable for
  rollback, restart with systemd, then verify service health, listening port and
  advancing finalized height.
- Do not reset DEV state or replace the PoA key during a routine deployment.
- DEV also runs two storage providers, `cybou-provider-1.service` and
  `cybou-provider-2.service` (`cybou-node provider run`, P2P ports 29471/29481,
  state in `/var/lib/cybou/provider-N-db`). The authority advertises them via
  `/var/lib/cybou/peers.txt`. Restart them with `cybou-node.service` after a
  rebuild and verify they follow the finalized height.

## Identity

- One protocol Identity is account-level. Device is not a protocol entity.
- Stable random AccountID is independent from mnemonic and keys.
- Recovery: Ed25519 + ML-DSA-65. Authorization: Ed25519 + ML-DSA-44.
- Identity KEM uses its own mnemonic-derived role; do not reuse signing keys.
- Portable CYBV2/CVID5 vault stores stable AccountID plus recovery entropy.
- IdentityRotate atomically replaces Recovery, Authorization and KEM roles.
- No device registry, primary-device concept, session authorization, identity
  transfer, DeviceAdd or DeviceRevoke.
- `.cybou` names use finalized commit/work/reveal and the active name rules.

## Finality

- The active protocol is genesis-bound single-operator hybrid-PQ PoA.
- Full nodes independently validate every operation, block transition, state
  root and PoA certificate.
- PoA is centralized finality, not BFT.
- Durable signing journal and equivocation conflict halt must fail closed.
- Authority, provider contribution, liveness and any future provisional
  validation never grant PoA finalization power.

## Application content

- `RootPublication` is the only application-content protocol operation.
- Mail, Files, filenames, folders, recipients and application schemas remain
  encrypted application data.
- ChunkID is full BLAKE3-256 of exact stored encrypted bytes.
- Unfinalized operations/chunks remain local. Remote providers accept chunks
  only after a finalized RootPublication authorizes them by Merkle proof.
- A RootPublication may locally bundle multiple encrypted content trees under
  one authorization root; this is an application implementation pattern, not a
  new wire entity.
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
- Core services are intentionally few:
  `ApplicationService`, `PublicationService`, `StorageService`.
- The GUI consumes semantic model data, never provider storage topology.

## Storage durability

- Development target: 1 remote full replica per required chunk.
- Beta target: 2 independent remote full replicas per required chunk.
- DEV may run more provider daemons than the target for failover, repair
  and soak; they do not raise the target.
- Local encrypted content is useful cache/staging but does not count toward the
  remote durability target.
- Beta uses full replication. Reed-Solomon/erasure coding is disabled.
- Placement is per ChunkID. Provider selection, health, audit and repair are
  StorageService concerns, not consensus state.
- Do not freeze a sophisticated deterministic placement algorithm before a
  mature anti-Sybil provider registry exists.
- A file/message is not `Protected`/`Sent` merely because its RootPublication is
  finalized.

## Identity Authority

Identity Authority supersedes the earlier Proof-of-Trust concept.

- Authority is deterministic per AccountID and uses finalized-height epochs.
- Authority is not CYBOU, not System Balance, not social scoring and not PoA
  power.
- Current-network Authority rules are immutable; no runtime governance or
  operator score editing.
- Age earns +1 per completed protocol epoch.
- Finalized qualifying Identity activity may earn +1 per operation, capped per
  epoch.
- A voluntary finalized `Balance -> System Balance` lock gives a one-time
  Authority contribution equal to the whole-CYBOU amount. Automatic onboarding
  credit gives none.
- Liveness may earn at most +1 per Identity/epoch when canonical evidence shows
  the union of bound-node uptime is at least 50%. Until that evidence protocol
  exists, liveness credit is zero.
- Storage contribution is based on verified encrypted byte×epoch work, never
  raw chunk count. Until canonical evidence exists, storage credit is zero.
- Provably false signed storage claims receive no reward and a protocol-defined
  penalty. Ordinary timeout is not automatically fraud.
- Penalties are accumulated separately from earnings; negative debt is not
  erased merely because effective Authority reaches zero.
- Global Authority changes require canonical attributable evidence. Local peer
  failures use local disconnect/backoff/ban logic.

## Authority resource limits

- Do not add Mail-specific or File-specific consensus quotas.
- Anti-abuse/resource policy is generic and Authority-tier based.
- Keep three resource domains:
  1. ProtocolBudget — canonical operation/publication work.
  2. StorageBudget — active remote replicated bytes.
  3. BandwidthBudget — PUT/GET bytes per epoch.
- Every new Identity has a non-zero usable base allowance and every budget has
  immutable hard ceilings.
- Use bounded integer arithmetic only.

## Future provisional validation

- Future research may add signed provisional validation.
- `network validated` is never equivalent to `PoA-finalized`.
- Only PoA finality advances canonical state and authorizes remote chunk
  admission.
- Do not reintroduce BFT, validator-set consensus or a competing canonical
  chain to implement this idea.

## Economics

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
4 fee units -> 3 Security + 1 Onboarding
```

Balance is spendable. System Balance is irreversible service budget. Authority
is separate and non-transferable.
