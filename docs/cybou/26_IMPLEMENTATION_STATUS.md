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
- CYP2 verified block sync and content-addressed PUT/GET;
- removal of legacy BFT, ValidatorSet, MailTx and indexed StorageObject runtime
  paths.

## Remaining product and operational work

- durability soak with multiple provider daemons, including provider loss,
  restart, audit and repair;
- complete and verify the Beta desktop acceptance matrix on clean installations
  and across supported Windows sizes, DPI settings and accessibility paths;
- persist drafts across desktop restarts;
- basic Identity Authority (Age, capped Activity, SystemContribution);
- Authority-derived generic resource budgets.

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
