# 30 — Repository documentation cleanup plan

Do not perform one giant documentation deletion.

Use staged cleanup synchronized with code changes.

## Phase D0 — Overlay only

Immediately:

- add CYBOU docs;
- replace root CYBOU `README.md`;
- add CYBOU `AGENTS.md`;
- add migration matrix;
- preserve `COPYING`;
- record upstream baseline.

Do not yet mass-delete inherited docs.

## Phase D1 — Network quarantine

When Bitcoin networks are removed/disabled:

Replace or rewrite:

```text
doc/README.md
doc/README_windows.txt
doc/bitcoin-conf.md
doc/dnsseed-policy.md
doc/files.md
```

Audit/remove Bitcoin network instructions in:

```text
README.md
INSTALL.md
src/* help text
src/qt/* user-visible strings
share/*
contrib/*
```

Acceptance grep categories:

```text
mainnet
testnet
testnet4
signet
.bitcoin
bitcoin.conf
8332
8333
bitcoincore.org
bitcoin-qt
bitcoind
```

Not every textual occurrence is forbidden: license/history/upstream-reference locations are allowlisted.

## Phase D2 — Product API cleanup

When RPC/REST/ZMQ code is disabled/removed from the product:

Archive/remove active docs:

```text
doc/JSON-RPC-interface.md
doc/REST-interface.md
doc/zmq.md
share/rpcauth/*
```

Update developer notes so new contributors do not reintroduce these surfaces casually.

## Phase D3 — Genesis/chain divergence

When CYBOU genesis and state semantics diverge:

Archive/replace:

```text
doc/bips.md
doc/assumeutxo.md
doc/design/assumeutxo.md
Bitcoin chain-selection docs
Bitcoin full-history assumptions
```

Create/maintain:

```text
CYBOU chain/state spec
CYBOU checkpoint/state-sync spec
CYBOU protocol/versioning spec
```

## Phase D4 — PoW removal

When mining/PoW ceases to be consensus:

Remove/replace:

- mining user docs;
- mining RPC references;
- difficulty/PoW operational guidance;
- miner-specific GUI text;
- Bitcoin release scripts that derive chainwork/assumevalid from Bitcoin-style flows.

Replace with:

```text
validator
stake
BFT rounds
finality
slashing/evidence
validator operations
```

## Phase D5 — Wallet/identity migration

Only after CYBOU wallet/account formats exist:

Retire/replace:

```text
doc/managing-wallets.md
doc/descriptors.md
doc/psbt.md
doc/multisig-tutorial.md
doc/offline-signing-tutorial.md
doc/external-signer.md
```

Do not remove early if inherited wallet functionality is still required for development transactions.

## Phase D6 — Packaging/release

Before first public CYBOU installer:

Replace:

```text
CONTRIBUTING.md
SECURITY.md
INSTALL.md
doc/release-process.md
doc/release-notes.md
doc/man/*
packaging descriptions
icons/assets attribution
translation project metadata
```

Ensure no user-facing package says "Bitcoin Core" except explicit ancestry/license notices.

## Phase D7 — Optional network transports

For:

```text
doc/tor.md
doc/i2p.md
doc/cjdns.md
```

Either:

- test/support and rewrite as CYBOU docs; or
- archive and mark unsupported.

Do not advertise inherited features merely because code still compiles.

## Stale-reference audit

Create a CI/documentation audit later that searches for Bitcoin-specific operational strings.

Candidate patterns:

```text
Bitcoin Core
bitcoin-qt
bitcoind
bitcoin-cli
bitcoin.conf
.bitcoin
bitcoincore.org
-chain=main
-testnet
-testnet4
-signet
8332
8333
JSON-RPC
REST
ZMQ
getblocktemplate
mining
assumeutxo
BIP
BTC
```

The checker needs an allowlist for:

- `COPYING`;
- upstream ancestry records;
- migration docs;
- archived upstream docs;
- source comments that intentionally describe inherited behavior.

The goal is not "zero word Bitcoin". The goal is "zero accidental Bitcoin operational behavior in active CYBOU documentation".
