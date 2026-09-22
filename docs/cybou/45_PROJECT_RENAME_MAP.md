# 45 — Bitcoin Core → CYBOU rename map

Reference audit: Bitcoin Core v31.1.

Reconcile this map with the exact local checkout before applying edits.

## Rule

Rename by exposure/risk, not by global string replacement.

Priority:

```text
network identity
user-visible identity
filesystem/config identity
packaging
public executable
active documentation
then internal identifiers when useful
```

## R0 — Preserve ancestry

Keep:

```text
COPYING
applicable upstream source headers
third-party licenses
exact upstream tag/commit record
```

Add:

```text
docs/cybou/UPSTREAM_BASELINE.md
NOTICE.md
```

## R1 — Root build/product metadata

Bitcoin v31.1 contains product metadata such as:

```text
CLIENT_NAME = Bitcoin Core
project(BitcoinCore)
DESCRIPTION = Bitcoin client software
HOMEPAGE_URL = bitcoincore.org
```

CYBOU target:

```text
CLIENT_NAME = CYBOU
project(CYBOU)
DESCRIPTION = CYBOU peer-to-peer network software
HOMEPAGE_URL = https://cybou.fr/
```

Do not invent a bug-report repository URL before one exists.

## R2 — Public executable

Bitcoin GUI target:

```text
bitcoin-qt
```

CYBOU:

```text
cybou
```

Windows:

```text
cybou.exe
```

Do not create `cybou-qt.exe`.

### Initial consumer package

Do not ship by default:

```text
bitcoind
bitcoin-cli
bitcoin-tx
bitcoin-util
bitcoin-wallet
bitcoin-node
bitcoin-gui
```

They may remain available in an upstream/internal developer build profile during migration.

## R3 — Multiprocess

CYBOU desktop is single-process:

```text
Qt UI + NodeCore
```

Disable the inherited multiprocess product path for the consumer build.

## R4 — Qt application identity

Set CYBOU equivalents for:

```text
QAPP_ORG_NAME
QAPP_ORG_DOMAIN
QAPP_APP_NAME_DEFAULT
```

Target:

```text
CYBOU
cybou.fr
CYBOU
```

Remove Bitcoin main/test/testnet4/signet application identities from the normal product.

Development identity may be:

```text
CYBOU Dev
```

## R5 — Qt source/class names

Useful early rename when files are already touched:

```text
BitcoinGUI         -> CybouGUI
BitcoinApplication -> CybouApplication
```

Defer purely cosmetic internal renames if they create large review noise:

```text
bitcoinqt internal target
bitcoin_common
bitcoin_node
bitcoin_util
BitcoinUnits
```

User-visible behavior matters more than internal spelling.

## R6 — Resources and branding

Replace/remove:

```text
Bitcoin logo/icons
testnet/signet icons
splash branding
Doxygen Bitcoin logo
About-dialog Bitcoin product branding
bitcoincore.org product links
```

Use CYBOU-owned assets.

## R7 — Config/datadir

Must change before normal CYBOU use:

```text
bitcoin.conf
Bitcoin/.bitcoin datadir
Bitcoin settings/registry namespace
service/PID names
package paths
```

Critical invariant:

> CYBOU never reads/writes a user's real Bitcoin Core datadir by default.

Use CYBOU-specific paths.

## R8 — Network/protocol identity

Replace before first normal network launch:

```text
network magic
ports
DNS seeds
fixed seeds
genesis
network names
checkpoints/assumevalid data
address/network prefixes as applicable
subversion/user-agent product identity
```

These are functional protocol changes, not branding.

## R9 — URI/payment surfaces

Inherited Qt contains Bitcoin payment/URI/PSBT/address-signing UX.

Do not mechanically relabel it.

Early action:

```text
disable/hide obsolete Bitcoin payment surfaces
```

A future `cybou:` URI requires an explicit specification.

## R10 — Translations

Do not ship stale Bitcoin translation catalogs.

Recommended first pilot scope:

```text
English
French
```

Keep translation tooling; rebuild reviewed CYBOU catalogs.

## R11 — Windows installer

Bitcoin v31.1 installer logic includes Bitcoin executable/config/network shortcuts and a Bitcoin URI handler.

CYBOU installer:

```text
Program Files\CYBOU
cybou.exe
CYBOU assets
no testnet/signet shortcuts
no bitcoin.conf
no unapproved Bitcoin utilities
no bitcoin: handler
```

Windows application identity must no longer use `org.bitcoincore.*`.

## R12 — Developer tooling

### Keep/adapt

```text
fuzzing
benchmarks
sanitizers
coverage
dependency checks
binary/reproducibility concepts
tracing
```

### Remove/archive when obsolete

```text
Bitcoin seed data
signet tooling
Bitcoin RPC auth
mining helpers
Bitcoin-specific URI/payment helpers
```

## Acceptance

```text
[ ] public executable is cybou.exe
[ ] product/window name is CYBOU
[ ] installer path is CYBOU
[ ] datadir/settings are isolated
[ ] no Bitcoin network selector/seeds
[ ] no bitcoin: handler
[ ] no Bitcoin logo in UI
[ ] no normal user docs require bitcoin-*
[ ] applicable upstream notices remain
```
