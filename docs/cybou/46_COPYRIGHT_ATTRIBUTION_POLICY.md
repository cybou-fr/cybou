# 46 — Copyright, license and attribution policy

Engineering policy only; obtain legal review before changing project licensing.

## CYBOU license: Apache-2.0

All CYBOU source files are Apache-2.0 (`LICENSE`, SPDX header in every file).
Project history and the MIT text kept in `docs/legal/BITCOIN_CORE_MIT.txt` are described in
`docs/legal/PROJECT_HISTORY.md`. Legal review is still recommended before release.

Holder label:

```text
Stanislav SAVELIEV
```

Revisit the wording if a legal entity or employment structure owns contributions.

## File headers

New CYBOU files:

```text
Copyright (c) 2026-present Stanislav SAVELIEV
SPDX-License-Identifier: Apache-2.0
```

Do not bump years mechanically across all files.

## Third-party code

Never add CYBOU headers to vendored components and never replace their holders.
Keep each component's own license file and audit it separately:

```text
LevelDB         src/leveldb   BSD-3-Clause
crc32c          src/crc32c    BSD-3-Clause
BIP-39 wordlist src/cybou/bip39_english.inc   MIT (BIP-0039)
Qt, Boost, OpenSSL, BLAKE3, zlib and other vcpkg dependencies
```

## Generated files

Change source templates and generators, not generated output headers.

## About/licenses UX

The product UI identifies CYBOU; an acknowledgements surface lists the
third-party components above with their notices.
