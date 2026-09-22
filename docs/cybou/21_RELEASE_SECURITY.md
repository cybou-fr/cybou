# 21 — Release and supply-chain security

A secure P2P protocol is insufficient if attackers can replace `cybou.exe`.

## Required release controls

Before public production:

- signed release artifacts;
- published cryptographic hashes;
- reproducible-build target where practical;
- multiple independent build/signature verification paths;
- dependency inventory/SBOM;
- pinned/reviewed toolchain;
- protected release keys;
- rollback policy;
- update authenticity validation.

## Upstream Bitcoin ancestry

Maintain a record of:

- original upstream tag/commit;
- imported upstream fixes;
- rejected upstream changes;
- consensus-divergent files.

After CYBOU consensus diverges, Bitcoin security patches cannot be merged blindly.

## Qt/dependencies

Track licenses and exact linked modules.

The project should prefer dynamically linked Qt modules available under acceptable open-source terms unless a commercial Qt license is intentionally adopted.

## Update safety

Automatic update functionality, if added, must not bypass signature verification.

A compromised website/CDN must not be sufficient to install a valid-looking malicious release.
