# 46 — Copyright, license and attribution policy

Engineering policy only; obtain legal review before changing project licensing.

Reference: Bitcoin Core v31.1 is distributed under the MIT license and its top-level `COPYING` preserves upstream Bitcoin Core/Bitcoin Developers notices.

## Preserve inherited notices

Never globally replace upstream or third-party holders with CYBOU.

## CYBOU license: Apache-2.0

All CYBOU source files are Apache-2.0 (`LICENSE`, SPDX header in every file).  Bitcoin-derived components were removed or rewritten; `COPYING` (MIT) is kept only as a record of the fork ancestry.
Legal review of the change is still recommended before release.

Structure:

- keep the Bitcoin Core MIT text in COPYING as ancestry record;
- preserve source-file headers;
- preserve third-party license files;
- add a CYBOU notice for CYBOU-authored work;
- add `NOTICE.md` for transparent fork ancestry.

Proposed project collective label:

```text
Stanislav SAVELIEV
```

Final holder wording should be revisited if a legal entity/employment structure owns contributions.

## File-header policy

### Untouched inherited file

Leave the upstream header unchanged.

### Trivial branding-only edit

Preserve upstream header. Do not claim sole CYBOU authorship.

### Substantially modified inherited file

Preserve existing header(s), then add a CYBOU contribution line, e.g.:

```text
Copyright (c) 2009-present The Bitcoin Core developers
Copyright (c) 2026-present Stanislav SAVELIEV
```

Do not rewrite upstream year ranges.

### New CYBOU file

Under Apache-2.0:

```text
Copyright (c) 2026-present Stanislav SAVELIEV
SPDX-License-Identifier: Apache-2.0
```

## Do not mechanically bump years

Do not touch every file annually.

## Third-party code

Never blanket-add CYBOU headers to vendored components.

Audit separately, including:

```text
secp256k1
leveldb
univalue
minisketch
crc32c
Qt
vcpkg source dependencies
```

## Generated files

Change source templates/generators, not generated output headers manually.

## About/licenses UX

Main product UI can identify CYBOU, while an acknowledgements/licenses surface states that parts are derived from Bitcoin Core and includes applicable open-source notices.

## NOTICE.md concept

```text
CYBOU contains software derived from Bitcoin Core.
Upstream baseline: <exact tag/commit>.
Bitcoin Core is distributed under the MIT License.
Applicable upstream copyright notices are preserved.
CYBOU modifications are copyright their respective contributors.
```

Do not fill the exact baseline until the local checkout is inspected.

## Pre-mass-edit checklist

```text
[ ] local upstream commit recorded
[ ] CYBOU license chosen
[ ] holder wording chosen
[ ] third-party inventory generated
[ ] generated files identified
[ ] vendor directories excluded
[ ] diff checked for erased notices
```
