# Open questions and cutover gates

This list tracks decisions that still block the active PoA and encrypted
chunk-DAG target. Product details belong in the Mail and Files UX documents;
wire and state authority belongs in the active protocol documents listed by
`AGENTS.md`.

## Finality and state

- Deterministic handling of conflicting PoA signatures and divergent forks.
- Durable anti-equivocation journal format, rollback detection, and recovery
  procedure for an unavailable genesis-bound signer.
- Canonical hybrid signature vectors and finality/state-root integration.
- State synchronization and independently recomputed finalized state on a
  clean full node.

## Encrypted content and admission

- Cross-implementation canonical-CBOR and encrypted chunk/graph vectors.
- Reviewed vectors for X-Wing draft-05 recipient capsule wrapping and recovery.
- Identity-authorized RootPublication execution, replay protection, and exact
  fee/state accounting.
- Provider admission proof format and verification against finalized
  RootPublication data.
- Durability threshold, independent per-chunk placement, retry, retention,
  audits, repair, accounting, and provider-loss handling.
- Publication scanning and recursive retrieval from a clean client without a
  previous local database.

## Product and operations

- Gmail-familiar Mail and Google Drive-familiar Files UI acceptance on desktop.
- Offline-recipient retrieval and recovery after interrupted transfers.
- Beta operational cost measurements for Mail, Files/Storage, and onboarding.
- Legal review of product claims and evidence exports before public service.

No single item authorizes a partial DEV rollout. Cutover follows only after all
format, key, finality, state, storage, Identity/name, and clean-machine recovery
gates pass together.
