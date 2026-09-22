# 28 — Bitcoin Core documentation migration

This document defines what to do with inherited Bitcoin Core documentation.

The audited reference tree is Bitcoin Core **v31.1**. The actual local CYBOU source baseline must still be recorded from the copy present in the repository; do not assume it is v31.1 without checking.

## Action vocabulary

| Action | Meaning |
|---|---|
| KEEP | Content remains useful and substantially correct; only naming may change. |
| ADAPT | Keep the technical subject but rewrite CYBOU-specific assumptions. |
| TRANSITIONAL | Keep temporarily while inherited subsystem still behaves like Bitcoin. |
| REPLACE | CYBOU has or needs a different authoritative document. |
| ARCHIVE | Upstream/historical reference only; not active product documentation. |
| REMOVE | Remove from active docs when the corresponding feature/code is removed. |
| KEEP-LICENSE | Must be retained for licensing/attribution reasons. |

## Root files

| Bitcoin path | Action | CYBOU treatment | Stage |
|---|---|---|---|
| `README.md` | REPLACE | CYBOU root README | now |
| `INSTALL.md` | ADAPT | CYBOU build/install entrypoint; point to supported Windows/Linux build docs | v0.0.x |
| `CONTRIBUTING.md` | REPLACE | CYBOU contribution/review rules; retain upstream history in Git | now/public repo |
| `SECURITY.md` | REPLACE | CYBOU vulnerability-reporting policy and supported versions | before public release |
| `COPYING` | KEEP-LICENSE | Keep Bitcoin Core MIT notice; add CYBOU notices separately, do not erase ancestry | always |

## `doc/` top level

| Bitcoin path | Action | CYBOU replacement / note | When |
|---|---|---|---|
| `doc/CMakeLists.txt` | ADAPT | Keep documentation build plumbing; rename targets/assets as needed | gradual |
| `doc/Doxyfile.in` | ADAPT | Keep Doxygen machinery, replace Bitcoin titles/logo/paths | gradual |
| `doc/README.md` | REPLACE | CYBOU documentation index + upstream-status warning | early |
| `doc/README_doxygen.md` | ADAPT | CYBOU source-doc landing page | early |
| `doc/README_windows.txt` | REPLACE | CYBOU Windows packaging/run notes | v0.0.1 |
| `doc/JSON-RPC-interface.md` | REMOVE | No desktop JSON-RPC API; archive until RPC code removed | when RPC removed |
| `doc/REST-interface.md` | REMOVE | No CYBOU REST API | when REST removed |
| `doc/zmq.md` | REMOVE | No product ZMQ API | when ZMQ removed |
| `doc/bitcoin-conf.md` | REPLACE | `cybou.conf`/configuration policy if config file retained; remove Bitcoin chain options | v0.0.1-v0.0.2 |
| `doc/dnsseed-policy.md` | REPLACE | CYBOU bootstrap/discovery policy | v0.0.1 |
| `doc/files.md` | REPLACE | CYBOU datadir/state/snapshot/storage layout | v0.0.1-v0.0.4 |
| `doc/assumeutxo.md` | REPLACE | CYBOU checkpoint/snapshot/bounded-history model | v0.0.4 |
| `doc/design/assumeutxo.md` | ARCHIVE | Upstream design reference only | v0.0.4 |
| `doc/bips.md` | ARCHIVE | Bitcoin BIP support matrix is not CYBOU protocol policy | after genesis fork |
| `doc/bitcoin_logo_doxygen.png` | REMOVE | Replace by CYBOU asset | branding pass |
| `doc/assets-attribution.md` | ADAPT | Keep upstream asset attributions until assets are removed; add CYBOU assets | branding pass |
| `doc/benchmarking.md` | ADAPT | Retain benchmark framework, add CYBOU consensus/storage/PQ benches | ongoing |
| `doc/dependencies.md` | ADAPT | Authoritative CYBOU dependency inventory | ongoing |
| `doc/developer-notes.md` | ADAPT | Keep coding/testing guidance; remove Bitcoin RPC/REST/ZMQ/mining-specific policy where obsolete | ongoing |
| `doc/fuzzing.md` | KEEP/ADAPT | Preserve fuzz infrastructure; add CYBOU parsers/consensus/storage/message targets | ongoing |
| `doc/productivity.md` | KEEP/ADAPT | Generic developer tooling is useful | ongoing |
| `doc/reduce-memory.md` | ADAPT | Important desktop-node resource policy | after state model |
| `doc/reduce-traffic.md` | ADAPT | Rewrite for CYBOU sync/storage/message traffic | after P2P/storage |
| `doc/tracing.md` | ADAPT | Keep tracing mechanics; rename components/events | ongoing |
| `doc/release-process.md` | REPLACE | CYBOU release/signing/reproducibility process | before first packaged release |
| `doc/release-notes.md` | REPLACE | CYBOU release-note process | first CYBOU release |
| `doc/release-notes/*` | ARCHIVE | Bitcoin historical release notes are upstream history, not CYBOU releases | early cleanup |
| `doc/translation_process.md` | ADAPT | Retain Qt localization process under CYBOU names | UI pass |
| `doc/translation_strings_policy.md` | ADAPT | Retain/review | UI pass |
| `doc/asmap-data.md` | TRANSITIONAL | Useful only if AS-map peer diversity remains | P2P review |
| `doc/p2p-bad-ports.md` | ADAPT | Retain concept, replace CYBOU ports/policies | P2P review |
| `doc/tor.md` | TRANSITIONAL/DEFER | Do not claim CYBOU support until tested | post-v1 candidate |
| `doc/i2p.md` | TRANSITIONAL/DEFER | Same | post-v1 candidate |
| `doc/cjdns.md` | TRANSITIONAL/DEFER | Same | post-v1 candidate |
| `doc/build-windows-msvc.md` | ADAPT | Primary CYBOU developer build doc | early |
| `doc/build-windows.md` | TRANSITIONAL | Keep only if cross-build path is tested/needed | build review |
| `doc/build-unix.md` | ADAPT | CYBOU Linux/infrastructure build doc | early |
| `doc/build-osx.md` | DEFER/ARCHIVE | Do not advertise macOS until tested | support-matrix review |
| `doc/build-freebsd.md` | DEFER/ARCHIVE | Not initial support target | support-matrix review |
| `doc/build-netbsd.md` | DEFER/ARCHIVE | Not initial support target | support-matrix review |
| `doc/build-openbsd.md` | DEFER/ARCHIVE | Not initial support target | support-matrix review |
| `doc/guix.md` | REPLACE/ADAPT | Preserve reproducible-build goal, use CYBOU release process/tooling | release-security pass |
| `doc/init.md` | TRANSITIONAL/ADAPT | Only for optional Linux infrastructure build; rename `bitcoind` service semantics | infra pass |

## Wallet-specific inherited docs

These remain useful **only while the inherited Bitcoin wallet model is still present**.

| Bitcoin path | Action | Trigger for replacement |
|---|---|---|
| `doc/managing-wallets.md` | TRANSITIONAL | CYBOU account/device wallet model |
| `doc/descriptors.md` | TRANSITIONAL | wallet/key/address redesign |
| `doc/external-signer.md` | TRANSITIONAL/DEFER | CYBOU hardware/device signer design |
| `doc/multisig-tutorial.md` | ARCHIVE/REPLACE | CYBOU multi-authority/account model |
| `doc/offline-signing-tutorial.md` | ARCHIVE/REPLACE | CYBOU offline/device authorization model |
| `doc/psbt.md` | TRANSITIONAL/ARCHIVE | CYBOU transaction/signing workflow |

Do not remove these before the code stops relying on them. Do not present them as final CYBOU UX.

## Mempool/policy docs

Bitcoin mempool code is useful inherited infrastructure. Keep these during the early fork, but label them transitional until CYBOU fees/state transitions are frozen:

```text
doc/policy/README.md
doc/policy/mempool-design.md
doc/policy/mempool-replacements.md
doc/policy/mempool-terminology.md
doc/policy/packages.md
```

Action: **TRANSITIONAL -> ADAPT**.

They must eventually describe CYBOU policy, not Bitcoin policy/BIP compatibility.

## Multiprocess docs

CYBOU's desktop decision is single-process full node + Qt UI.

Therefore:

```text
doc/multiprocess.md
doc/design/multiprocess.md
```

Action: **ARCHIVE/REMOVE from active CYBOU docs**.

Useful architectural lesson retained:

- Bitcoin's internal `src/interfaces/` separation is valuable even when all components execute in one process.

Do not interpret upstream multiprocess plans as CYBOU product direction.

## Design libraries doc

```text
doc/design/libraries.md
```

Action: **KEEP/ADAPT**.

Library boundaries remain valuable and should evolve into CYBOU NodeCore module boundaries.

## Man pages

Current Bitcoin man pages:

```text
doc/man/bitcoin.1
doc/man/bitcoin-cli.1
doc/man/bitcoin-qt.1
doc/man/bitcoin-tx.1
doc/man/bitcoin-util.1
doc/man/bitcoin-wallet.1
doc/man/bitcoind.1
```

Action:

- remove from CYBOU distributable package as the associated binaries disappear;
- do not rename all of them mechanically;
- create only man/help surfaces for binaries CYBOU actually ships;
- desktop goal is `cybou.exe`; optional Linux infrastructure binary naming is a separate decision.

## Documentation outside `doc/`

A Bitcoin documentation cleanup is incomplete if only `doc/` is edited.

Audit:

```text
README.md
INSTALL.md
CONTRIBUTING.md
SECURITY.md
COPYING

.github/
contrib/
share/
src/qt/              user-visible strings/help/about
src/                 command-line help text
test/                Bitcoin-specific test descriptions
ci/                  Bitcoin naming/network assumptions
.tx/                  translation project identity
CMakeLists.txt        product/binary names and options
```

Special attention:

- sample `bitcoin.conf` files;
- `bitcoin-qt`, `bitcoind`, `bitcoin-cli` references;
- RPC authentication documentation;
- service-unit examples;
- package metadata;
- desktop files/icons;
- generated manpages;
- URLs to bitcoincore.org;
- Bitcoin network ports and datadirs.

## What must never be blindly removed

### License/copyright notices

Bitcoin Core is MIT-licensed. Required copyright and permission notices must remain in copies/substantial portions.

Keep `COPYING` and preserve applicable source-file notices.

CYBOU may add its own license/notice policy without pretending the inherited code has no origin.

### Technical documentation for still-active code

If CYBOU still uses an inherited component, deleting its only correct technical documentation is worse than keeping an explicitly marked upstream reference.

Rule:

```text
replace code + replacement doc
THEN
remove obsolete upstream doc
```

not:

```text
delete docs first
guess later
```
