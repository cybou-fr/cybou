# CYBOU documentation

> [!IMPORTANT]
> The `doc/` directory primarily contains inherited upstream Bitcoin Core documentation.
> It is **not** CYBOU architecture authority unless explicitly referenced from
> `docs/cybou` or `INSTALL.md`.

For normative CYBOU architecture and development specifications, start with:
- **Level 0 (Implementation authority)**: [`AGENTS.md`](../AGENTS.md)
- **Level 1 (Frozen architecture / decisions)**: [`docs/cybou/24_DECISIONS.md`](../docs/cybou/24_DECISIONS.md), [`docs/cybou/02_ARCHITECTURE.md`](../docs/cybou/02_ARCHITECTURE.md)
- **Documentation index**: [`docs/cybou/README.md`](../docs/cybou/README.md)
- **Implementation status**: [`docs/cybou/26_IMPLEMENTATION_STATUS.md`](../docs/cybou/26_IMPLEMENTATION_STATUS.md)

## Active protocol

- [Network bootstrap and genesis lifecycle](../docs/cybou/04_NETWORK_BOOTSTRAP_AND_GENESIS.md)
- [PoA finality](../docs/cybou/POA_FINALITY.md)
- [France-first sovereign network policy](../docs/cybou/37_FRANCE_SOVEREIGN_NETWORK_POLICY.md)
- [Encrypted chunk tree](../docs/cybou/ENCRYPTED_CHUNK_TREE.md)
- [RootPublication](../docs/cybou/ROOT_PUBLICATION.md)
- [Chunk storage admission](../docs/cybou/STORAGE_ADMISSION.md)
- [Identity discovery and recovery](../docs/cybou/IDENTITY_DISCOVERY_AND_RECOVERY.md)
- [Identity and `.cybou` names](../docs/cybou/10_IDENTITY_NAMES.md)
- [Authority and advisory Validation](../docs/cybou/57_AUTHORITY_AND_VALIDATION.md)
- [Economics and fees](../docs/cybou/18_ECONOMICS_FEES.md)
- [Account creation and anti-Sybil work](../docs/cybou/70_ACCOUNT_CREATION_ANTI_SYBIL.md)

## Product and development

- [Mail UX](../docs/cybou/82_MAIL_UI_UX.md)
- [Files UX](../docs/cybou/83_STORAGE_UI_UX.md)
- [Application data plane](../docs/cybou/APPLICATION_DATA_PLANE.md)
- [Product workflows](../spec/mail_files_architecture.yaml)
- [Build instructions](../INSTALL.md)
- [Contribution guidelines](../CONTRIBUTING.md)
- [License](../COPYING) and [notices](../NOTICE.md)
