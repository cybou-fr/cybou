# Implementation status

## Implemented substrate

Current `main` implements the canonical low-level substrate:

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
  storage peers to the development target of 2 remote replicas (local copy
  excluded), GET/BLAKE3 audit, repair and verified remote retrieval;
- placement recovery: rebuilds the ordered authorized-chunk set from
  provider-held verified proofs after Application DB loss, including private
  Mail attachments and Files content while excluding reused trees;
- ApplicationService: checkpointed, idempotent scan of finalized
  RootPublications, capsule opening inside the key store, fetch through
  StorageService, Mail Inbox/Sent and last-canonical-mutation-wins Files
  projections in the Application DB, retry of temporarily unavailable roots;
- derived Identity Authority preview: finalized-height Age, capped activity,
  voluntary System Balance contribution, integer tiers and generic budget
  calculations; not bound to network parameters and not enforced;
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
- removal of legacy BFT, ValidatorSet, MailTx and indexed StorageObject runtime
  paths.

## Current phase: soak, hardening and Beta preparation

- multi-process durability soak with real `cybou-node provide` daemons:
  provider loss after ACK, restarts, audit, repair, lost placement and
  Application DB, long-running operation;
- reproducible build + core + Qt tests after every vertical batch (CI);
- complete and verify the Beta desktop acceptance matrix on clean installations
  and across supported Windows sizes, DPI settings and accessibility paths;
- 3 independent remote replicas for Beta;
- only after the soak: bind immutable Authority rules to the network and
  enforce Authority-derived generic resource budgets.

## Evidence-gated later work

- NodeID binding and canonical uptime evidence;
- Liveness Authority;
- canonical verified-storage contribution evidence;
- Storage Authority and false-storage-claim penalties.

## Future research only

- provisional non-final validation;
- validation contribution to Authority;
- erasure coding.

Do not claim designed features are live merely because UI fixtures expose their
target states.
