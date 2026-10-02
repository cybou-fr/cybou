# Contributing to CYBOU

Before changing code, read `AGENTS.md` and the active protocol documents it
names. The single protocol authority in `main` is the PoA + encrypted chunk-tree
target described by `docs/cybou/04_NETWORK_LIFECYCLE.md`,
`docs/cybou/POA_FINALITY.md`, `ENCRYPTED_CHUNK_TREE.md`, `ROOT_PUBLICATION.md`,
`STORAGE_ADMISSION.md`, `IDENTITY_DISCOVERY_AND_RECOVERY.md`,
`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`, and `spec/poa_chunk_tree.yaml`.
`docs/cybou/26_IMPLEMENTATION_STATUS.md` distinguishes implemented substrate
code from integration and deployment gates.

## Architecture rules

- All participants run the same full-node software core. Storage and
  Central Authority PoA finalization are optional operational capabilities.
- Bootstrap is an ordinary CYBOU full peer for initial discovery; it has no
  special consensus powers and runs the same binary.
- Public P2P admission is France-only in production and DEV (inbound and outbound)
  using local Geo data (fails closed).
- The finalizer uses the genesis-authorized hybrid-PQ PoA key operated from the
  Central Authority desktop. This is single-operator finality, not BFT consensus.
  Every full node executes every candidate operation. Identities with finalized
  AUTH > 1,000,000 may add Validation signatures after their own node validated
  an operation; signatures are evidence only and PoA always re-executes.
- Generic `RootPublication` is the only application-content publication
  operation. Mail, Files, Backup, filenames, recipients, graph edges, and
  application schemas are private encrypted content.
- Chunk IDs are full BLAKE3-256 digests of stored ciphertext. Use the pinned,
  vetted implementation; do not implement cryptographic primitives locally.
- Consensus and state-transition logic use deterministic integer arithmetic.
- Parsers enforce the frozen size, count, and depth limits.
- Keep Identity authorization, Identity recovery, Identity KEM, PoA, release,
  and treasury key roles separate. Production signatures require the configured
  hybrid classical and post-quantum components.
- Preserve Gmail-familiar Mail and Google Drive-familiar Files UX requirements
  under CYBOU branding. Keep service indexes client-local and encrypted.
- Do not add legacy protocol documentation, runtime compatibility, or dual
  operation decoders to `main`.

## Development workflow

- Use C++20 and the repository's existing formatting conventions.
- Keep changes focused and commit them with concise, descriptive messages.
- Add focused tests for new serialization, cryptography integration, bounds,
  and state-machine behavior. Run the relevant tests and build targets before
  reporting completion.
- For CYBOU core or `cybou-node` changes, follow the DEV deployment and safety
  gates in `AGENTS.md`. Preserve rollback artifacts and do not reset DEV except
  after the coordinated integration gate.
