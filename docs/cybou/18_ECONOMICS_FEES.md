# CYBOU economics and fees

Status: CURRENT
Scope: Current economic flows and lease/settlement source audit at 787e18ca, 2026-10-09. Accepted assignment/evidence gaps remain explicit; no new live acceptance claim.

## Native asset

```text
CYBOU
decimals = 0
genesis monetary base = 100,000,000,000 (all in the Central Treasury; conserved)
```

There is no perpetual base emission.

## Balance

Spendable, transferable CYBOU controlled by the Identity.

## System Balance

Irreversible service budget.

A user may voluntarily lock:

```text
Balance -> System Balance
```

There is no user-initiated unlock from System Balance back to Balance.

Protocol service fees are paid from System Balance.

## Onboarding

AccountCreate transfers the configured onboarding bonus (20,000 CYBOU):

```text
Central Treasury Balance -> new Identity System Balance (onboarding origin)
```

The claimant of the Treasury allocation itself receives no bonus. AccountCreate
does not mint CYBOU.

## Protocol fees

100% of every finalized protocol fee transfers atomically from the payer's
System Balance to the Central Authority's spendable Balance. The recipient is
the unique genesis allocation labelled `cybou`, granting the reserved
`cybou.cybou` Identity. No separate fee wallet, recipient parameter or key exists.
This economic role grants no finalization power: PoA remains authorized solely
by the genesis PoA public key.

Before claim, fees accumulate in that allocation's Balance. AccountCreate
claims the full accumulated Balance once. After claim, fees credit the claimant's
ordinary AccountState Balance; the allocation Balance no longer changes.

Missing or duplicate allocations, invalid claimants and recipient overflow
reject the operation without changing balances or authorization nonce. There
is no batching, fee burn or priority bidding. Storage rent is
not a fee: it goes through StorageEscrow to providers (below).

Payment uses the existing configured payment_fee. RootPublication uses:

```text
4 * ceil(full canonical operation bytes / 1024) + 4 * chunk_count
```

Each paid operation performs its own transfer. Block execution checks that
TotalCybou is unchanged; it performs no later fee distribution.

## Supply

TotalCybou counts unclaimed genesis allocation Balances, account Balances,
account System Balances and StorageEscrow. The genesis monetary base exists in
full from genesis; there is no cap, no unissued remainder and no virtual reserve.

## Network cutover

State has no legacy decoder. A new genesis requires a new Network Public Key
and therefore a new NetworkID, followed by a clean network-bound state reset.
Current compiled DEVNET has its new NetworkID and immutable genesis; the
former DEVNET is retired. MAINNET remains unprovisioned. Development uses DEVNET. Existing genesis is never re-signed or replaced.

## Storage economy

These flows operate under the existing compiled DEVNET genesis. MAINNET is
unprovisioned. This document does not authorize provisioning or a reset.

### Monetary base

```text
decimals = 0
genesis monetary base = 100,000,000,000 CYBOU in the cybou.cybou Treasury allocation
no MAX_SUPPLY, no OnboardingPool, no unissued supply, no mint, no burn
TotalCybou = unclaimed genesis Balances + Balances + System Balances + StorageEscrow
TotalCybou(parent) == TotalCybou(candidate); overflow -> invalid state
```

### Flows

```text
AccountCreate (PoW kept)  Treasury Balance -20,000  new System Balance +20,000
SystemLock                Balance -X                System Balance +X
protocol fee              payer System Balance -X   Treasury Balance +X
StorageLease              payer System Balance -X   StorageEscrow +X
StorageSettlement         StorageEscrow -X          provider Balance / System Balance +X
lease refund              StorageEscrow -X          payer System Balance +X
```

The Treasury claimant receives no onboarding bonus. Storage rent goes 100% to
providers; ordinary operation fees go 100% to Treasury. There is no stake,
slashing, auction or variable provider price.

### Rent

Beta rate: 5 CYBOU per GiB per day per remote replica; `replica_target = 2`,
so 10 CYBOU per logical GiB per day. Billing unit: 512 KiB per authorized chunk
(2048 units = 1 GiB). Cost numerator `units x seconds x replicas x 5`,
denominator `2048 x 86400`, integer only, with a carried remainder. The rate is
provisional until shadow accounting (M4) measures real cost. M4 implements
the exact arithmetic (`AccrueStorageRent`: floor with carried remainder, so
split periods sum exactly) and accrues shadow rewards from verified intervals.

### Onboarding cannot be laundered

System Balance records an onboarding-origin portion consumed first by debits.
Escrow keeps the origin split, and the onboarding-origin share of a payout
credits provider System Balance; only SystemLock-origin rent becomes
transferable Balance. Current provider selection uses local CSPRNG ordering and
deduplicates proven StorageIds and payout identities; it excludes this machine.
Settlement execution refuses payment to the lease payer. These checks do not
prove independent operators or prevent one operator owning multiple Identities.

DEC-280 retains the accepted target of assignment from finalized randomness with
PoA attestation. Current selection does not implement that target. DEC-282
evidence/assignment fields likewise remain an explicit design gap; the current
wire below must not be confused with that accepted target.

### Equilibrium is an estimate

A node offering 100 GiB of provider capacity at full demand earns about what
50 GiB of two-replica storage costs. This is never a guarantee: without demand
no foreign chunks are assigned.

### Legal gate

Transferable CYBOU earned for a service may fall under MiCA. Qualification is
an open legal gate before MAINNET (`25_OPEN_QUESTIONS.md`).

## Current lease and settlement wire

All integers below are little-endian. These are operation payloads; the common
operation envelope is separate. Hashes and AccountIDs retain their raw 32-byte order.

`StorageLease` payload is exactly 36 bytes: publication OperationID (32), then
periods (u32). Periods must be positive and respect the compiled maximum (3650).
The author can extend the lease of a finalized publication. RootPublication may
fund its initial lease atomically through `lease_periods`; zero means no initial
lease, not free remote admission. A funded active lease is required for admission.
Lease funding reserves rent rounded up; interval accrual rounds down with a
carried remainder. Remaining escrow is refunded to the payer's System Balance.

`StorageSettlement` has the following current layout:

| Field | Bytes / encoding |
|---|---|
| period | u64, 8 |
| period_start_utc | u64, 8; nonzero |
| entry_count | u32, 4; 0–1024 |
| entries | count × 72 |
| poa_signature | Ed25519 64 followed by ML-DSA-65 3309 |

Each entry is publication OperationID (32), payout AccountID (32), amount
(u64, 8). Entries strictly increase by `(publication_id, payout_account)`;
IDs and amount are nonzero. Exact payload size is `20 + 72*N + 3373` bytes;
there is no signature length prefix and trailing bytes are rejected.
The signed digest is SHA-256 of the exact concatenation
`"CYBOU/STORAGE-SETTLEMENT" || NetworkBinding || unsigned settlement body`.

Execution verifies the genesis-authorized hybrid PoA signature, next period,
contiguous start after the first period, active lease, existing payout account,
no payment to the payer, replica-count bound, period-rent/remaining-escrow bounds,
and overflow-free balance transfers. Period end is derived from the signed start
and immutable period length (86400 seconds in DEVNET). Nodes verify these rules;
they do not independently verify off-chain audits through the settlement payload.
There is no `evidence_root`, `StorageId`, assignment attestation or explicit
`period_end_utc` field in the current payload.

Defining sources: [storage_lease.h](../../src/cybou/storage_lease.h),
[storage_lease.cpp](../../src/cybou/storage_lease.cpp),
[protocol_params.h](../../src/cybou/protocol_params.h),
[storage_provider_selector.cpp](../../src/cybou/storage_provider_selector.cpp).
Regression sources: [resource limits tests](../../src/test/cybou_resource_limits_tests.cpp)
(escrow/conservation, payout attacks, lease bounds, revocation),
[storage economy tests](../../src/test/cybou_storage_economy_tests.cpp)
(billing and carried remainder), and
[state tests](../../src/test/cybou_state_tests.cpp) (Treasury and onboarding).
These references identify existing tests; this documentation audit does not
claim a new execution of those suites or new live payout acceptance.
