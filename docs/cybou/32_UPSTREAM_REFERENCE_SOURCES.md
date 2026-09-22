# 32 — Upstream reference sources used for v0.3 audit

The v0.3 documentation-migration audit used the Bitcoin Core v31.1 repository tree as a reference snapshot.

Primary upstream references:

- `https://github.com/bitcoin/bitcoin/releases` — release list/current v31.1 reference;
- `https://github.com/bitcoin/bitcoin/tree/v31.1/doc` — documentation tree;
- `https://github.com/bitcoin/bitcoin/blob/v31.1/doc/developer-notes.md`;
- `https://github.com/bitcoin/bitcoin/blob/v31.1/doc/files.md`;
- `https://github.com/bitcoin/bitcoin/blob/v31.1/doc/release-process.md`;
- `https://github.com/bitcoin/bitcoin/blob/v31.1/doc/multiprocess.md`;
- `https://github.com/bitcoin/bitcoin/blob/v31.1/doc/design/multiprocess.md`;
- `https://github.com/bitcoin/bitcoin/blob/v31.1/COPYING`;
- `https://github.com/bitcoin/bitcoin/blob/v31.1/CONTRIBUTING.md`;
- `https://github.com/bitcoin/bitcoin/security/policy`.

These links are research references, not CYBOU runtime dependencies.

The local CYBOU repository may contain another Bitcoin Core revision. Always reconcile `spec/bitcoin_doc_migration.yaml` against the actual local tree before deleting or moving files.
