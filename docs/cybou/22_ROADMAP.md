# CYBOU protocol and product roadmap

This roadmap describes the single active target in `main`: generic encrypted
content over RootPublication and a bounded chunk DAG, finalized by a
genesis-bound single-operator PoA signer. The PoA trust model is centralized
and does not provide Byzantine fault tolerance. The currently deployed DEV
network remains untouched until all coordinated cutover gates pass.

## Protocol reset sequence

The authority is `POA_FINALITY.md`, `ENCRYPTED_CHUNK_DAG.md`,
`ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`,
`IDENTITY_DISCOVERY_AND_RECOVERY.md`, and `spec/poa_chunk_dag.yaml`.

1. Pin vetted BLAKE3 and freeze the full 256-bit encrypted ChunkID.
2. Implement bounded canonical CBOR and the encrypted chunk envelope.
3. Build and read bounded local encrypted chunk DAGs.
4. Freeze RootPublication, hybrid recipient capsules, and exact chunk
   admission accounting.
5. Integrate Identity authorization, state execution, and genesis-bound PoA
   finality with a durable anti-equivocation journal.
6. Implement finalized-publication admission into content-addressed ChunkStore.
7. Implement local publication scanning, recursive retrieval, and clean-machine
   recovery.
8. Add Mail and Files private-schema adapters over the shared substrate.
9. Pass the integration and recovery gates, then perform one coordinated DEV
   cutover. Do not add runtime compatibility or a dual operation decoder.

Implemented substrate work is tracked in `26_IMPLEMENTATION_STATUS.md`.
PoA fork/equivocation handling, state integration, provider durability, and
clean-machine recovery remain cutover gates. A finalized publication authorizes
storage; it does not prove that providers retain the chunks.

## Product direction

Mail and Files are required product surfaces with familiar Gmail and Google
Drive workflows under CYBOU branding. They share one Identity and one private
encrypted content substrate. Mail starts with one recipient and text-only
messages. Files, attachments, sharing, and Backup remain gated on the common
chunk storage and recovery path. Backup is post-Beta.

### Alpha product gate

- Create and restore an Identity on a clean installation.
- Claim and use a `.cybou` name.
- Send encrypted text to an offline recipient and retrieve it after the
  recipient reconnects.
- Rebuild local Mail indexes across restarts and verify sender authorization.
- Independently verify finality and state on a full node.
- Explain the single-operator PoA trust model clearly in product UX.

### Beta product gate

- Store and retrieve finalized encrypted chunks with a measured durability
  threshold, retry, repair, and interruption recovery.
- Support Mail attachments and Files catalogs over the shared private chunk
  graph.
- Provide Gmail-familiar Mail workflows and Google Drive-familiar Files
  workflows, including progress, failure, retry, preview, search, and recovery.
- Complete the applicable product and UX acceptance criteria in docs 79–85.
- Validate clean-machine recovery without relying on a prior client database.

### Controlled pilot

Run a controlled pilot with 20–100 users for 8–12 weeks after the Beta gates
pass. Measure Mail and Files usage, offline retrieval, recovery, storage repair,
support burden, and history growth. Keep Backup post-Beta and disclose the
single-operator trust model.

## Economic and release gates

- Keep DEV, Beta, and Mainnet parameters and genesis states separate.
- Size Beta onboarding from measured integrated Mail and Files/Storage costs.
- Freeze the Mainnet onboarding bonus only after aggregate Beta operations.
- Keep the release, treasury, Identity, and PoA key roles separate.
- No production signature path may fall back to classical-only signatures.
