# DEVNET development

Development and integration use the existing DEVNET. Its compiled public keys,
immutable signed genesis and pre-generated local private material are preserved.
Cleanup is not a provisioning request and does not authorize a NetworkID change.
MAINNET remains unprovisioned.

## Full Node and CLI checks

All nodes use the ordinary `cybou` executable and France-only peer admission.
The compiled initial locator is `51.255.46.58:29461` with its TLS SPKI pin.

```text
cybou network info --network devnet
cybou node run --network devnet --data-dir DEVNET_NODE_DIR --peer-admission france
cybou doctor --network devnet --data-dir DEVNET_NODE_DIR
python test/cybou_operator_cli.py PATH_TO_HEADLESS_CYBOU
```

The acceptance script creates a temporary ordinary DEVNET node, connects to the
existing compiled locator, checks restart/event logging and verifies doctor is
read-only. It does not create a genesis, export a key, enable PoA or submit user
operations. Its optional offline Geo overrides are `--geo-country-csv`,
`--geo-sha256` and `--geo-issued-month`; all three must be supplied together.
Geo data must pass the same integrity and freshness checks as ordinary nodes.

## Existing PoA signer

The genesis-authorized operator runs one active signer with one durable signing
history. Use the existing Central Authority Identity and its existing vault and
node state. Never start an additional signer to make a test pass, never export
the Network Root to a running node, and never provision another development
network. Identity creation and publication finality require the existing signer
to be unlocked by the operator.

## Component and content checks

`BUILD_TESTS=ON` builds the core tests, Qt tests, load generator and storage
smoke/soak clients. Component tests use in-memory fixtures and synthetic inputs;
they are not a selectable network profile. Socket component tests provide
synthetic Geo CSV data to the actual integrity-checked admission implementation.
There is no runtime admission bypass.

The load generator and storage clients operate on compiled DEVNET and use
ordinary Identity, funding, fees, finality and remote replica rules. Run them
only with the intended real budgets and the existing signer available. Storage
fault scenarios require the operator to act on the indicated replica and
acknowledge the client's marker; the former automatic process/fault controller
is removed. No test harness generates or distributes network or signing keys.

Production builds use `BUILD_TESTS=OFF` and `BUILD_PROVISION_TOOL=OFF`. The
offline provisioning tool remains a separate explicit network-creation tool.
