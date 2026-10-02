# Implementation status

## Target architecture vs prototype

- **Target architecture**: Every participant runs the same full-node software (`CybouNode`). Bootstrap, storage, advisory Validation, and PoA finalization are optional local capabilities, not protocol node classes. Genesis authorizes 1–4 bootstrap Identities (by stable `AccountID` and expected `RecoveryKeyID`). Public P2P admission is France-only (fails closed). The Central Authority desktop runs PoA finalization; bootstrap never finalizes. State v9 schema.
- **Current implementation prototype**: Temporary standalone utility `cybou-bootstrap provision|serve`, bounded STATUS/CLAIM/REPLACE protocol handler, LevelDB `EMPTY`/`BOUND` store with generation numbering, legacy v8 state support, and target v9 state with grants keyed by stable AccountID plus expected RecoveryKeyID.
- **Current DEV deployment**: The existing legacy testnet remains available for routine development: `cybou-node.service` finalizes on port 29461, and `cybou-provider-1.service` / `cybou-provider-2.service` provide storage on ports 29471/29481. Keep its state and PoA key until acceptance tests and the planned new-genesis DEV cutover are complete.

## Bootstrap prototype and target gap

The current branch contains a bootstrap prototype, not the revised target
genesis roster. France-only peer admission is implemented as local transport
policy and does not change consensus:

- grant-bearing state currently uses v8 and keys grants by RecoveryKeyID with
  optional claimed AccountID; grant-free legacy state retains the exact v7
  encoding and root;
- the public network-file format has a canonical serializer, and a `CYBB1`
  NetworkBinding envelope can be signed by and verified against the genesis PoA key;
- the core has a durable LevelDB `EMPTY`/`BOUND` store: provisioning generates a
  256-bit one-use activation code, stores its domain-separated hash, and an initial
  claim atomically stores generation 1;
- `cybou-bootstrap provision|serve` is a prototype executable; initial contact requires
  client SPKI SHA-256 pinning;
- signed key replacement primitive is implemented in the store, but client-side
  acceptance and database rollback remain open;
- the pinned pre-genesis proof binds a fresh challenge, TLS exporter, and
  Recovery public key, but does not yet bind the candidate's stable AccountID;
- `CAP_BOOTSTRAP` verifies an AccountID session proof against a claimed grant
  and current Authorization key;
- the initial locator list is intentionally empty pending an approved target
  endpoint and SPKI pin;
- the pre-genesis proof is not yet wired into the running bootstrap CLI/vault
  flow or desktop network-creation wizard;
- `PeerAdmissionPolicy` checks the SHA-256 pin and a declared DB-IP release
  month before admitting public peers; data older than 45 days is rejected.
  Node, bootstrap, and desktop use the built-in updater to check the official
  DB-IP Lite release page over verified HTTPS at startup and every 14 days,
  verify the published archive SHA-1, validate the decompressed CSV, calculate
  its local SHA-256, and atomically activate it. They reuse a validated cache
  during network outages; if no fresh dataset is available, P2P admission
  stays closed. The `CYBOU_GEO_COUNTRY_CSV`, `CYBOU_GEO_SHA256`, and
  `CYBOU_GEO_ISSUED_MONTH` desktop settings and matching CLI flags remain
  available for explicit offline overrides. `CYBOU_DEV_PEER_ADMISSION=lab`
  is the explicit private-address-only developer override.
- Inbound and outbound policy checks, explicit LAB-only private-address policy,
  pre-TLS bootstrap filtering, and automatic Geo database refresh are
  implemented. Optional anonymizer filtering, a visible desktop Geo update
  status, and full desktop/bootstrap boundary acceptance coverage remain open.

The revised target requires a new canonical state version (v9) with one to
four grants keyed by stable AccountID and storing expected RecoveryKeyID plus
a claimed flag. It also requires retaining v7/v8 legacy-state support until a
coordinated new-genesis cutover.

## Implemented substrate

Current `main` implements the canonical low-level substrate:

- prototype optional genesis bootstrap grants keyed by RecoveryKeyID,
  one-time AccountCreate claim into stable AccountID, and current
  Authorization-key lookup that survives IdentityRotate;
- CYP2 `CAP_BOOTSTRAP` proof bound to NetworkID, TLS exporter, both HELLOs and
  AccountID, with remote Authorization keys resolved only from finalized
  genesis grants and caller-supplied unlocked-Identity signing;
- pinned-TLS pre-genesis Recovery-key challenge/response bound to a fresh
  challenge and that TLS exporter;
- no compiled CYP2 peer seed; the DEV bootstrap's STATUS/CLAIM protocol is not
  a CYP2 endpoint. The future pre-genesis address/SPKI locator also remains
  unconfigured until the official endpoint and pin are approved;

- Identity keys/authorization/recovery and current KEM publication;
- names, balances and deterministic state execution;
- genesis-bound hybrid-PQ PoA finality;
- durable PoA anti-equivocation journal and conflict safety halt;
- generic Identity-authorized RootPublication;
- recipient capsules and finalized-history lookup;
- encrypted content-addressed chunks;
- common persistent/in-memory ChunkBlobStore independent of provider admission;
- streaming ROOT/INDEX/DATA tree;
- chunk-authorization Merkle proofs;
- finalized provider chunk admission;
- encrypted per-Identity local Application DB primitives, bound to the unlocked
  key store and rebuildable without changing canonical state;
- strict canonical-CBOR private Mail, Files and RecoveryBridge schema codecs;
- local multi-tree publication staging with one durable chunk-authorization
  proof index and no pre-finality provider admission;
- PublicationService: recipient plus mandatory self capsule, durable private
  job record and exact-operation resume through IdentityOperationCoordinator;
  Mail (new or reused attachments), Files mutation batches with uploaded
  content, queued publication while another Identity operation is unresolved,
  and PROTECTED only after StorageService reports remote durability;
- StorageService: post-finality placement on distinct CSPRNG-selected CYP2
  storage providers, distinct by handshake-proven ProviderID (not
  address:port), to the remote replica target (development 1, Beta 2; local
  copy excluded), GET/BLAKE3 audit, repair and verified remote retrieval;
- local retention: generic pin/cache registry; staged publication chunks are
  pinned until PROTECTED, then become LRU cache under a budget; GC never
  removes pinned, provider-admitted or unrecorded blobs;
- placement recovery: rebuilds the ordered authorized-chunk set from
  provider-held verified proofs after Application DB loss, including private
  Mail attachments and Files content while excluding reused trees;
- ApplicationService: checkpointed, idempotent scan of finalized
  RootPublications, capsule opening inside the key store, fetch through
  StorageService, Mail Inbox/Sent and last-canonical-mutation-wins Files
  projections in the Application DB, retry of temporarily unavailable roots;
- derived Identity Authority preview from finalized history; informational and
  not used for protocol admission or resource allocation;
- Identity RecoveryBridge: verified before IdentityRotate, readable by the
  next KEM key, and imported on restore only for seeds that reproduce the
  canonical historical KEM package, followed by a rescan;
- Qt desktop adapter: live Mail and Files actions, attachments and protected
  Mail/Files content reuse; per-unlocked-Identity worker owns the three
  application services, periodically scans and advances publication
  durability without blocking the GUI thread;
- desktop Identity rotation: RecoveryBridge is published and reaches remote
  durability, then verified with the replacement phrase before IdentityRotate;
- core acceptance tests for offline Mail delivery, Sent/Files rebuild after
  Application DB loss, exact file download, Mail/Files content reuse and
  pre-rotation content on a clean machine with the new mnemonic only;
- hardening: atomic Application DB batches (record + index, block +
  checkpoint) with retry of unindexed publications; desktop session reopened
  after a finalized IdentityRotate (drafts carried, projection rebuilt);
  bounded periodic durability audit that returns Protected to Securing when
  remote copies are lost; size-only provider startup with BLAKE3 on every
  GET and self-healing re-PUT; private Files schema v2 (modified time, every
  FILE has content, reserved Trash ID);
- CYP2 verified block sync and content-addressed PUT/GET;
- any full node may relay exact signed operations hop by hop through bounded
  volatile queues; only the Authority admits them over a live PoA-authenticated
  session;
- removal of legacy BFT, ValidatorSet, MailTx and indexed StorageObject runtime
  paths.

## Current phase: soak, hardening and Beta preparation

- multi-process durability soak with real `cybou-node provider run` daemons:
  provider loss after ACK, restarts, audit, repair, lost placement and
  Application DB, long-running operation;
- reproducible build + core + Qt tests after every vertical batch (CI);
- complete and verify the Beta desktop acceptance matrix on clean installations
  and across supported Windows sizes, DPI settings and accessibility paths;
- 2 independent remote replicas for Beta;
- keep Authority informational; optional advisory Validation remains a local
  recipient trust decision.

### Multi-process failure soak (CI, 2026-09-30)

`test/cybou_storage_soak.py` runs a real PoA finalizer, three real
`cybou-node provider run` processes and a light client under the Beta target
(2 remote replicas), all over CYP2. Every run passes this sequence:

1. A replica holder is killed; audit detects the loss and repairs elsewhere.
2. The provider restarts and returns under the same ProviderID.
3. A provider blob is corrupted on disk; audit drops it and repairs until
   every recorded replica serves valid bytes.
4. The finalizer restarts; the client reconnects for sync and submission.
5. `app.db` is deleted; the projection and placements are rebuilt from
   history and provider proofs.
6. The recovery phrase is rotated (RecoveryBridge PROTECTED and verified
   first) and `app.db` is rebuilt under the new keys.
7. A clean restore runs on a fresh node with only the new phrase; both the
   pre-rotation and post-rotation files return with exact bytes from
   providers.

It found a client bug: after a finalizer restart a light node connected only
to storage peers reported submissions as rejected. The configured finalizer is
now always tried, and no acknowledgment is treated as uncertain delivery with
the exact bytes retained. The soak takes about 45 seconds.

### Bounded DEV canary evidence (2026-09-29 to 2026-09-30)

- A finalized private Files RootPublication reached two remote replicas in this canary test
  (the current development target is one; Beta requires two). Removing its provider-1 root blob while the provider
  stayed online caused `StorageService::Audit` to detect and repair the missing
  copy from provider-2; verified retrieval still matched the original content
  after the local root was evicted.
- The same root blob survived independent provider-1 and provider-2 restarts.
  Provider-2 was also unavailable for 12 seconds while authority and provider-1
  stayed online; its service rejoined afterward. Both remote SHA-256 values
  remained `91b99c6cfac6e49dd0c981603e0a2cf45042882f5ec0f88f330df2da6fd974db`.
- At the final sample, all three services were active and listening; finalized
  heights were authority `34231`, provider-1 `34230`, provider-2 `34229`.
- This is bounded recovery evidence, not a sustained soak. Long-running
  operation/restart coverage, clean-machine Beta acceptance, and live desktop
  observation of the `Protected` to `Securing` transition remain outstanding.

## Future research only

- optional signed advisory Validation if recipients need it;
- erasure coding only if Beta replication measurements justify it.

Do not claim designed features are live merely because UI fixtures expose their
target states.
