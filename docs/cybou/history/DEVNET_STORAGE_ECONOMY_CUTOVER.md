# DEVNET storage-economy cutover (M7) — operator runbook

Status: HISTORICAL
Scope: Completed migration or superseded delivery order; recorded claims retain their original dates.

Status: steps 1–3 executed 2026-10-04 (NetworkBinding `6d202ccf…2d97`, genesis
anchor `e0d3d6ee…c25f`, `verify-devnet` passed, phrases kept, Network Root generated
on a networked machine by operator decision). Step 4 executed 2026-10-04: the VPS
runs `--capacity 40GiB` at height 0 on the new genesis, SPKI pin unchanged, Geo
data carried over, old state archived under
`/var/lib/cybou/node-retired-20261004-pre-storage-economy`, operator CLI acceptance
passed. Steps 5–6 (Central Authority desktop, live acceptance) pending.
Every step that creates keys, signs
genesis, touches `/private/`, commits constants or changes the DEV VPS needs the
operator's explicit authorization at the time it is run (`AGENTS.md`, DEC-283).

This is a new official network, not an upgrade: new Network Root, new NetworkID,
new signed genesis. The current DEVNET (`846e8f22…b311`) is retired permanently;
nothing is imported from it (DEC-238, DEC-252).

## What the new genesis contains

`cybou-provision create-devnet` already builds it from the code on main:

| Item | Value |
|---|---|
| Network Root | freshly generated, offline |
| `cybou` allocation (Central Treasury) | 100,000,000,000 CYBOU, 10,000,001 AUTH |
| `bootstrap` allocation | 0 CYBOU, 10,000,001 AUTH |
| `onboarding_bonus` | 20,000 CYBOU (Treasury → System Balance) |
| Storage rent | 5 CYBOU / GiB / day / replica, 2 replicas, 86,400 s periods, ≤ 3,650 periods per lease |
| PoA key | unchanged if the `cybou.cybou` phrase is kept (recommended) |
| Bootstrap locator / TLS pin | unchanged: `51.255.46.58:29461`, same SPKI |

## Decisions the operator makes first

1. **Keep the `cybou.cybou` phrase** (`--keep-central-authority`): same PoA key
   and same desktop recovery phrase; only the AccountID is new. Recommended.
2. **Keep the bootstrap Identity phrase** (`--keep-bootstrap-identity`). Recommended.
3. **VPS local capacity `V`** (minimum 15 GiB), chosen from `df -h` on the VPS.
4. **Where the new Network Root mnemonic is kept offline** after signing.

## 0. Preconditions

- `main` contains M1–M6 and the payout-account selection (`c203ae3` or later);
  the full core suite and the Qt suite pass.
- No user depends on the current DEVNET data.
- The machine used for step 2 can be disconnected from the network.

## 1. Archive the current public constants

```text
mkdir private/devnet-retired-pre-storage-economy-20261004
copy src/cybou/official_devnet_constants.h private/devnet-public-retired-pre-storage-economy-20261004.h
```

The current `private/devnet/` secrets stay where they are; they are the input for
`--keep-*` and the evidence of the retired network.

## 2. Provision offline

Disconnect the network. Build the offline tool (`BUILD_PROVISION_TOOL=ON`), then:

```text
cybou-provision create-devnet private/devnet-storage-economy src/cybou/official_devnet_constants.new.h
  --keep-central-authority private/devnet/cybou_identity_secret.txt
  --keep-bootstrap-identity private/devnet/bootstrap_identity_secret.txt
```

The tool refuses existing paths and never overwrites secrets or constants. It
writes the three secret files and `summary.txt` under
`private/devnet-storage-economy/` and the public header.

Write the new **Network Root mnemonic** on paper, verify it, then move
`network_root_secret.txt` off this machine (DEC-244: the Network Private Key is
never kept online). Its only use was this one signature.

## 3. Install the constants and verify

```text
move src/cybou/official_devnet_constants.new.h src/cybou/official_devnet_constants.h
cmake --build BUILD_DIR
cybou-provision verify-devnet private/devnet-storage-economy
```

`verify-devnet` needs the Network Root file; run it before moving that file
offline, or bring the paper/offline copy back temporarily.

Restore the compiled-DEVNET checks that are disabled until M7:

- `cybou_network_genesis_tests`: replace
  `compiled_devnet_fails_closed_until_the_storage_economy_cutover` with the
  positive checks (compiled genesis verifies, state root matches, locator and
  SPKI pin unchanged);
- `CybouShellTests::runtimeStartupFailureCanBeRetried`: the retry succeeds again.

Run the full core and Qt suites. `git status` must show only
`src/cybou/official_devnet_constants.h`, the restored tests and docs — never a
file under `private/`.

Update `04_NETWORK_LIFECYCLE.md` (new NetworkBinding and genesis anchor),
`AGENTS.md` (DEVNET status, VPS deployment state), `26_IMPLEMENTATION_STATUS.md`,
`22_ROADMAP.md` (M7 done), regenerate `MANIFEST.md`, commit and push.

## 4. VPS cutover

On `debian@vps-d0669a91.vps.ovh.net`:

```text
sudo systemctl stop cybou-node.service
sudo mv /var/lib/cybou/node /var/lib/cybou/node-retired-20261004-pre-storage-economy
cd /home/debian/cybou && git pull && cmake --build build --target cybou
systemctl cat cybou-node.service
```

Add `--capacity <V>GiB` to the `ExecStart` line (step 0 decision), keep the
existing TLS certificate/key arguments (`/etc/cybou-bootstrap/tls/`), then:

```text
sudo systemctl daemon-reload
sudo systemctl start cybou-node.service
journalctl -u cybou-node.service -n 50
```

Check: the node starts on the new NetworkID, listens on `29461`, presents the
same SPKI pin, and height 0 is the new genesis anchor.

Optional: restore the `bootstrap` Identity on the VPS and run the node with
`--identity-vault/--identity-password-file`, so its StorageId carries a payout
binding and it can be paid for storage.

## 5. Central Authority desktop

The desktop does not wipe the retired network automatically: it reports
"Local CYBOU state belongs to another network." (gap against
`73_CORE_DESKTOP_CONTRACT.md`). Close the desktop and move its data directory:

```text
move %LOCALAPPDATA%\CYBOU %LOCALAPPDATA%\CYBOU-retired-20261004-pre-storage-economy
```

Start the new desktop, restore `cybou.cybou` from its (kept) phrase: the
AccountCreate claims the `cybou` allocation — 100,000,000,000 CYBOU, no
onboarding bonus — and the name `cybou`. Unlock the PoA signer with a fresh
signing journal. Exactly one signer runs.

## 6. Acceptance on the new DEVNET

1. A new Identity receives exactly 20,000 CYBOU System Balance; Treasury
   Balance drops by 20,000.
2. Publishing a file finalizes a RootPublication with its initial lease
   (default 30 periods) and reaches `Protected` with replicas on distinct payout
   accounts.
3. Revoking it closes the lease after the current period.
4. A PoA `StorageSettlement` for period 0 is accepted (Central Authority page,
   "Settle storage period") and `TotalCybou` stays 100,000,000,000.
5. `python test/cybou_operator_cli.py PATH_TO_HEADLESS_CYBOU` passes.

Record results in `DEVNET_LIVE_ACCEPTANCE.md`.

## Rollback

Before step 4 nothing outside this repository changed: revert the constants
commit and keep `private/devnet-storage-economy/` as an unused network. After
step 4 the old network can be resumed only with a pre-M5 binary and the archived
`/var/lib/cybou/node-retired-…` state; it is never mixed with the new one.

## Known gaps after cutover

- no transport of other payers' off-chain storage evidence to the PoA: the
  settlement command pays only leases the Central Authority Identity placed;
- no proof that a paid provider was the network-assigned one (DEC-280 assignment
  evidence);
- lease renewal UX before expiry;
- the AccountCreate PoW price against splitting into many Identities is unmeasured.

Archived from docs/cybou/DEVNET_STORAGE_ECONOMY_CUTOVER.md at 1f08f8c5.
Do not execute or use this record as current implementation requirements.
