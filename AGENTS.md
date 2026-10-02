# AGENTS.md — CYBOU implementation authority

Read the active CYBOU documents before coding. Git history records superseded
architecture; do not keep obsolete runtime paths alive for compatibility.

## DEV VPS deployment — migration state

There is no production or Beta network. The DEV VPS runs the
`cybou-bootstrap.service` prototype. Its pinned TLS endpoint is the approved
DEV Bootstrap locator (`51.255.46.58:29461`). This assigns discovery trust to
that live TLS key only; it grants no consensus role and does not make the
service a CYP2 full node. The target uses one full-node core software with
optional storage and PoA finalization capabilities, with the currently authorized
PoA key holder finalizing from the Central Authority desktop. France-only public peer admission
is mandatory in production/DEV. The planned new-genesis DEV cutover replaces the
legacy testnet; it is not a production-network migration.
- Central Authority is identified by the current Network-Root-authorized PoA
  key $P_{\text{epoch}}$. Never add a persistent Authority IP, host, endpoint, or
  NodeID to bootstrap state or consensus. Authenticate its current route per live
  session and discard that route on disconnect.

- After changing CYBOU core or `cybou-node`, run relevant tests, rebuild the
  affected DEV executable on the VPS, and restart its systemd service in the
  same task.
- Connect as `debian@vps-d0669a91.vps.ovh.net`; checkout:
  `/home/debian/cybou`; service binary:
  `/home/debian/cybou/build/bin/cybou-node`.
- Preserve the current executable for rollback, restart with systemd, then
  verify service health and listening port. Finalized-height checks apply only
  when a finalizer is deployed.
- Current DEV service: `cybou-bootstrap.service` runs
  `/home/debian/cybou/build/bin/cybou-bootstrap serve` on `0.0.0.0:29461`,
  with state in `/var/lib/cybou/bootstrap/state` and TLS files under
  `/etc/cybou-bootstrap/tls/`. SSH listens on port 22. The legacy
  `cybou-node.service` finalizer and both provider services are inactive.
- The DEV locator is `51.255.46.58:29461`; its SPKI SHA-256 pin is compiled
  in `src/cybou/bootstrap_nodes.h`. The pin authenticates this pre-genesis
  TLS endpoint only. Do not infer PoA or other consensus authority from it.

## Network and node architecture

- Every participant runs the same full-node software core. Storage and
  Central Authority PoA finalization are optional operational capabilities,
  not protocol node classes.
- A standard CYBOU installation knows official network profiles (DEVNET,
  TESTNET, MAINNET). Each profile pins bootstrap IP:port and TLS SPKI plus an
  immutable Network Root public key $R$.
- An `OfficialNetworkBinding` signed by $R$ defines the network generation,
  Authority epoch, exact network definition and current PoA public key $P$.
  Bootstrap distributes the binding and initial peer addresses; it cannot
  designate an official network or Authority on its own. $R$ never finalizes
  blocks; $P$ cannot authorize its successor or replace the network.
- Cross-network migration does not exist. A newer valid official network
  replaces all local network-bound state, wiping everything:
  chain/state, network definition, genesis, Identity, vault, AccountID,
  Recovery/Auth/KEM keys, balances, names, Mail, Files, application DB,
  peer DB, pending operations, storage metadata, and Authority indexes.
- Authority rotation increments `authority_epoch` under a new root-signed
  assignment `{epoch, activation_height, P}` without replacing the network or
  Identity. Historical blocks use the assignment active at their height.
  There is no working-PoA-key trust chain.
- Ordinary peers form a direct P2P mesh after initial discovery. Bootstrap is
  an initial rendezvous service, not a mandatory traffic intermediary.
- Only the Central Authority has canonical pending state. Operation relay
  through ordinary peers is bounded and volatile; there is no distributed mempool.
- Public P2P admission is France-only in production/DEV, for inbound and
  outbound peers across all capabilities. Classification uses local Geo data;
  unavailable/corrupt data fails closed. LAB loopback/private test traffic
  requires an explicit LAB bypass. Optional VPN/proxy/Tor filtering is local
  policy and never changes consensus or Identity.
- Bootstrap nodes do not vote, form a quorum, or finalize. PoA remains
  single-operator finality under the active Authority key. Keep the existing DEV
  state operational until acceptance tests and a coordinated cutover pass.

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

## Documentation hierarchy and sources of truth

The documentation has a strict hierarchy; lower levels cannot introduce
architecture that is absent from higher levels:
- LEVEL 0 — Implementation authority: `AGENTS.md`
- LEVEL 1 — Frozen architecture and decisions: `docs/cybou/24_DECISIONS.md`, `docs/cybou/02_ARCHITECTURE.md`
- LEVEL 2 — Normative domain documents: `04_NETWORK_LIFECYCLE.md`, `05_CHAIN_STATE.md`, `08_P2P.md`, `POA_FINALITY.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, `10_IDENTITY_NAMES.md`, `18_ECONOMICS_FEES.md`, `57_IDENTITY_AUTHORITY.md`, etc.
- LEVEL 3 — Mutable implementation truth: `docs/cybou/26_IMPLEMENTATION_STATUS.md`
- LEVEL 4 — Roadmap and unresolved work: `docs/cybou/22_ROADMAP.md`, `docs/cybou/25_OPEN_QUESTIONS.md`
- LEVEL 5 — Product and UX contracts: `docs/cybou/81_BETA_PRODUCT_SCOPE.md`–`85_BETA_UI_ACCEPTANCE.md`, `APPLICATION_DATA_PLANE.md`
- LEVEL 6 — Business, legal, and sovereign policy: `docs/cybou/37`–`43`
- LEVEL 7 — Machine-readable mirrors: `spec/*`
- LEVEL 8 — Public projection: `README.md`, `www/*`, `www/llms.txt`

## Finality

- Finality is single-operator hybrid-PQ PoA under the root-authorized key
  $P_{\text{epoch}}$ for the block height.
- Target operation puts the authorized PoA-key holder on its Central Authority
  desktop; bootstrap services never finalize. See
  `docs/cybou/04_NETWORK_LIFECYCLE.md`.
- Full nodes independently validate every operation, block transition, state
  root, and PoA certificate.
- PoA is centralized finality, not BFT.
- Durable signing journal and equivocation conflict halt must fail closed.
- Authority metric never grants PoA finalization power.
- The PoA finalizer independently executes operations.

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
- The default provider policy is finality-first.
- A file/message is not `Protected`/`Sent` merely because its RootPublication
  is finalized.

## Derived Identity Authority

- Authority is an informational, read-only metric derived from finalized
  history. It is separate from Balance and System Balance and grants no
  protocol, resource-allocation, or PoA power.
- There is no canonical ValidatorSet, validator registry, NodeID binding,
  liveness/storage evidence, reward/penalty system, resource budget,
  reservation, grant, ticket, or per-I/O accounting.
- Local peer failures use local disconnect, backoff, and abuse limits; they do
  not change global Authority.
- Advisory Validation is deferred non-canonical functionality and not part of
  active node operation.

## Economics

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
4 fee units -> 3 Security + 1 Onboarding
```

Balance is spendable. System Balance is an irreversible service budget.
Authority is non-transferable and informational.
