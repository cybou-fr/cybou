# CYBOU protocol and product roadmap

The active protocol substrate is genesis-bound hybrid-PQ PoA, generic
RootPublication and one encrypted content-addressed chunk tree.

## Delivered substrate

- stable AccountID / Identity authorization and recovery;
- `.cybou` names and balances;
- PoA finality with durable signing journal and conflict halt;
- generic RootPublication and recipient capsules;
- streaming encrypted ROOT/INDEX/DATA trees;
- BLAKE3 ChunkID;
- finalized-publication Merkle chunk admission;
- CYP2 finalized block sync and content-addressed PUT/GET;
- removal of legacy BFT, MailTx and indexed StorageObject paths.

## Application integration (delivered)

The encrypted per-Identity Application DB, ApplicationService,
PublicationService, StorageService, private Mail and Files backends,
clean-machine recovery with RecoveryBridge, development durability with a
remote replica target, audit/repair and placement recovery are implemented and
connected to the desktop.

## Current phase: soak, hardening, Beta preparation

No large new features. In order:

1. multi-process soak with real `cybou-node provider run` providers (loss, restart,
   repair, lost local state, long runs);
2. reproducible build + core + Qt tests after every vertical batch;
3. Beta desktop acceptance on clean installations;
4. 2 independent remote replicas (Beta target);
5. only then move Authority from preview towards enforcement.

## Identity Authority

After the basic Mail/Files/storage path is operational:

1. implement Age + capped Activity + SystemContribution Authority;
2. add generic Authority tiers and Protocol/Storage/Bandwidth budgets;
3. design canonical NodeID binding/evidence;
4. enable Liveness Authority only after canonical uptime evidence exists;
5. enable Storage Authority only after canonical storage contribution evidence
   exists;
6. validate penalty evidence and anti-farming behavior.

## Beta

Beta requires:

- Gmail-familiar Mail and Google-Drive-familiar Files UX;
- clean-machine recovery without old Application DB;
- 2 independent remote full replicas per required chunk (plus the local copy);
- measured provider loss/repair behavior;
- honest `Protected`/`Sent` states;
- operational cost and anti-abuse measurements;
- no Reed-Solomon/erasure coding requirement.

## Future research

- signed validation as local-policy pre-finalized evidence, scoped by
  [`VALIDATION.md`](VALIDATION.md);
- validation contribution to Authority only if later justified;
- erasure coding only after measured replication cost warrants it;
- more scalable provider discovery if simple peer fan-out stops being adequate.

Do not block Mail/Files Beta on future provisional validation.
