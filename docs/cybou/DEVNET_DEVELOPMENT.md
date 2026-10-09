# DEVNET development

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

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


## DEV VPS deployment — migration state

There is no production network.
- **Target architecture**: The DEV bootstrap is an ordinary CYBOU full peer process.
- **Current deployment**: `cybou-node.service` runs the ordinary headless
  `cybou node run --capacity 40GiB` on the current DEVNET (cut over
  2026-10-04), with state under `/var/lib/cybou/node/state`. The AUTH-era
  storage-economy state is retired under `/var/lib/cybou/node-retired-20261004-auth`;
  the pre-storage-economy one under `/var/lib/cybou/node-retired-20261004-pre-storage-economy`;
  the domain before it under `/var/lib/cybou/node-retired-20261003-de-version`;
  the earlier hardening retirement is preserved separately. Neither state may
  be reused by the current network. The prototype service is inactive.
  TLS files remain under `/etc/cybou-bootstrap/tls/` for transport identity only.
  Two permanent DEVNET storage peers run beside it as `cybou-storage@2` and
  `cybou-storage@3` (ports 29462/29463, state under `/var/lib/cybou/storage-N`,
  installed by `tools/battle/vps_storage_nodes.sh`): ordinary Full Nodes giving
  publications the two distinct remote replicas Beta durability needs. They share
  the host, so they are distinct StorageIds, not independent failure domains.
- Its pinned TLS endpoint is the approved DEV Bootstrap locator (`51.255.46.58:29461`);
  its SPKI SHA-256 pin is compiled in `src/cybou/official_networks.cpp` for initial transport
  discovery only. This grants no consensus role, no special protocol capability, and does not make
  the bootstrap a separate node class.
- The target uses one full-node core software with intrinsic encrypted storage and an optional local PoA signer, with the authorized PoA key holder finalizing from the Central Authority desktop.
  France-only public peer admission is mandatory in production/DEV.
- Central Authority is identified solely by the PoA key authorized by network genesis.
  Never add a persistent Authority IP, host, endpoint, or NodeID to bootstrap
  state or consensus. No peer session authenticates a PoA route. Verify block signatures only.
- Connect as `debian@vps-d0669a91.vps.ovh.net`; checkout: `/home/debian/cybou`;
  service binary: `/home/debian/cybou/build/bin/cybou` (headless build).


Deployment revision/evidence: [implementation status](26_IMPLEMENTATION_STATUS.md).
Historical migrations are retained under history/ and must not be replayed.
