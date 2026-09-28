# CYBOU architecture

CYBOU is an experimental identity-centered platform for private messaging and
user-controlled files. The active target uses one protocol substrate for all
application content; Mail and Files remain product experiences implemented by
the desktop client.

## System layers

```text
Qt desktop client
  ├── account-level Identity and portable recovery
  ├── local encrypted Mail and Files indexes
  └── native CYBOU runtime
       ├── canonical state execution and full-node validation
       ├── genesis-bound hybrid-PQ PoA finality
       ├── generic RootPublication
       ├── encrypted canonical-CBOR chunk DAG
       └── finalized-publication ChunkStore admission
```

The active protocol authority is `AGENTS.md`,
`POA_FINALITY.md`, `ENCRYPTED_CHUNK_DAG.md`, `ROOT_PUBLICATION.md`,
`STORAGE_ADMISSION.md`, `IDENTITY_DISCOVERY_AND_RECOVERY.md`, and
`spec/poa_chunk_dag.yaml`.

## Finality and validation

A genesis-bound single-operator hybrid-PQ PoA signer finalizes blocks. Each
full node independently verifies the finality proof, executes every operation,
and compares the resulting state root. This is centralized finalization and
does not provide Byzantine fault tolerance. A durable anti-equivocation journal
and deterministic conflict/fork behavior are required before activation.

## Content and privacy

`RootPublication` is the only application-content publication operation.
Application schemas, recipient identity, names, metadata, and graph edges stay
inside encrypted chunks. ChunkID is the full BLAKE3-256 digest of stored
ciphertext. Recipient capsules wrap one graph content key for a recipient KEM
capability without publishing the AccountID.

Consensus enforces generic publication count/byte bounds and deterministic
size-aware fees. Mail, Files, and Backup do not create application-specific
consensus operation types or per-item state. Local clients scan finalized
publications, recover authorized content keys, retrieve chunks, and rebuild
service indexes.

Finality authorizes chunk storage but does not prove provider durability.
Clients distinguish finalized, available, and retrievable content, retain
ciphertext locally, and expose retry/recovery states.

## Product surfaces

- Mail uses familiar Gmail workflows under CYBOU branding. The initial profile
  is one recipient and UTF-8 text; attachments and multi-recipient flows wait
  for shared storage and recovery gates.
- Files uses familiar Google Drive workflows, private encrypted catalogs,
  immutable versions, and identity-based sharing.
- Backup is post-Beta.

## Key and economic boundaries

Identity recovery, Identity authorization, Identity KEM, PoA, Release Signing,
and Treasury key roles remain separate. Production signatures require both
configured hybrid components; no classical-only fallback is allowed.

The supply cap is 100,000,000,000 CYBOU with zero decimals. Deterministic fees
route each four-unit fee as three to Security and one to Onboarding. System
Balance pays protocol services; it does not alter PoA trust or account scores.

## Cutover

The target is not active on DEV. One coordinated cutover follows only after
format, key, finality, state, storage, Identity/name, and clean-machine recovery
gates pass together. Cutover discards the old DEV state and vaults. Do not add
runtime compatibility, automatic import, or dual decoders.
