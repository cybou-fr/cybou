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

## Immediate application integration

Implement in this order:

1. encrypted per-Identity Application DB;
2. `ApplicationService` publication scan/index;
3. `PublicationService` outbound RootPublication flow and self capsules;
4. `StorageService` retrieval and finality-first remote placement;
5. real Mail private backend;
6. real Files private mutation backend;
7. clean-machine recovery including RecoveryBridge after Identity rotation;
8. development durability with 2 independent remote replicas;
9. provider health/audit/repair and interruption recovery.

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
- 3 independent remote full replicas per required chunk;
- measured provider loss/repair behavior;
- honest `Protected`/`Sent` states;
- operational cost and anti-abuse measurements;
- no Reed-Solomon/erasure coding requirement.

## Future research

- signed provisional validation that remains strictly non-final;
- validation contribution to Authority only if later justified;
- erasure coding only after measured replication cost warrants it;
- more scalable provider discovery if simple peer fan-out stops being adequate.

Do not block Mail/Files Beta on future provisional validation.
