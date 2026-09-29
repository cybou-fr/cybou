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
- CYP2 verified block sync and content-addressed PUT/GET;
- removal of legacy BFT, ValidatorSet, MailTx and indexed StorageObject runtime
  paths.

## Designed next, not yet implemented as complete product paths

- Mail/Files semantic records and indexing atop the encrypted Application DB;
- ApplicationService publication scanning/indexing;
- PublicationService bundle staging/self-capsule/finality flow;
- StorageService provider placement, replication, health, audit and repair;
- real private Mail backend over RootPublication;
- real private Files mutation backend;
- clean-machine Mail/Files reconstruction;
- Identity RecoveryBridge for historical KEM epochs;
- development 2-remote-replica durability path;
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
