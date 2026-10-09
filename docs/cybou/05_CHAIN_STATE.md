# Canonical chain state

Status: CURRENT
Scope: Block/state/PoA source audit at ee9d721a, 2026-10-09; existing regression references are not a fresh suite run or independent interoperability acceptance.

Full nodes execute finalized blocks deterministically and derive the same state
root. Mail and Files do not create permanent per-message/per-file consensus
objects.

## Current state domains

- account values: Balance and System Balance in CYBOU;
- Identity registry: stable AccountID, Recovery/Authorization capabilities,
  current KEM commitment, nonce and key epoch;
- `.cybou` name registry;
- Central Treasury (the `cybou` allocation or its claimant) funding onboarding and receiving protocol fees;
- genesis allocations containing initial CYBOU; the unique `cybou`
  allocation also accumulates protocol fees before its one-time claim;
- immutable network parameters bound to the active network definition;
- the publication register `publications` (RootPublication OperationID ->
  owner, chunk authorization root, chunk count, height);
- storage economy (DEC-277..DEC-282): per-account onboarding-origin System
  Balance, `settlement` cursor (next period, its UTC start) and `leases`
  (publication -> payer, units, replicas, period range, escrow by origin).

Bootstrap is an ordinary CYBOU full peer and has no consensus grants, roles, or
separate state registry. The active network definition and genesis are signed
offline by the Network Private Key.

Application schemas and private metadata remain encrypted outside canonical
state.

## RootPublication history

RootPublication commits generic encrypted content roots, authorization Merkle
root, chunk count, optional initial lease periods and recipient capsules.
The active publication record stores owner, authorization root, count and height;
root ChunkID and capsules are retrieved from finalized operation history.

Finalized block history is the discovery/proof source. It does not maintain a
consensus row for each Mail message or file.

Clients rebuild private application projections from finalized publications.

## No Authority unit

There is no AUTH, Authority index or per-Identity usage counter in state
(DEC-284). Spam is priced by fees, storage rent and relay proof-of-work.

## State synthesis and object pruning

Block finalization synthesizes transaction history into canonical `CybouState`.
Active publications remain in the notarial registry to authorize chunk storage
and verify inclusion proofs. When an authenticated `RevokePublication` from the
publication's owner is finalized, its record is removed from the active register and
its funded lease closes after the current settlement period. There is no author
quota to free. A revoked publication no longer authorizes chunk admission.
Compliant providers journal purge of unshared chunks, retain physical accounting
until unlink succeeds or absence is confirmed, and retry failures. This does not
prove deletion of hidden copies; see [erasure boundary](DATA_ASSURANCE_AND_ERASURE.md).
Historical blocks remain immutable, but active consensus state does not accumulate dead objects.

## Invariants

- no plaintext Mail/File metadata in consensus;
- no per-Mail/per-file canonical state object: the register holds one record per
  finalized RootPublication (which may bundle many encrypted trees), with no
  plaintext metadata;
- execution does not read local wall-clock time: settlement uses the signed UTC
  start, contiguous period cursor and immutable period length;
- Authority never grants PoA finalization weight or consensus voting power;
- Balance and System Balance are the two canonical account values; there is no Authority unit;
- all consensus state arithmetic is bounded integer arithmetic.

## Canonical encoding

State is the sole canonical encoding. Allocations
are immutable except for their one-time claimed_by and the pre-claim Balance
of the unique Central Authority allocation. After claim its allocation Balance
stays fixed; new fees credit the claimant account Balance. TotalCybou counts
unclaimed allocation Balances, account Balances, System Balances and lease
escrow, with checked integer arithmetic; every finalized block preserves it
exactly. All sections are always encoded, in order: accounts, identities, names,
allocations, `publications`, settlement cursor, `leases`; maps strictly
ordered, no trailing bytes.

## Storage economy

- The whole genesis monetary base is the `cybou`
  Treasury allocation; AccountCreate transfers `onboarding_bonus` from Treasury
  to the new System Balance.
- `TotalCybou` = unclaimed genesis Balances + Balances +
  System Balances + StorageEscrow; every block preserves it exactly.
- System Balance records its onboarding-origin portion (DEC-281).
- StorageLease records publication, payer, units, replicas, first/end periods
  and escrow by origin. There is no encoded ACTIVE/CLOSING/EXPIRED enum or
  accrual remainder in `StorageLeaseRecord`; lifecycle follows the period range
  and publication presence. State stores the next expected settlement period
  and its start, not a separate last-settlement record.
- Storage rights derive from finalized leases; the publication register
  authorizes content admission.

## Current state snapshot layout

All integers are little-endian; IDs are raw 32-byte values. Maps arrive strictly
ordered by key, with duplicates, invalid registry relationships and trailing
bytes rejected. Counts/lengths are checked against input and domain limits before
allocation. The order in [state.cpp](../../src/cybou/state.cpp) is:

| Section | Encoding |
|---|---|
| accounts | u32 count; each AccountID[32] then five u64 values: Balance, System Balance, onboarding portion, creation height, creation epoch (72 bytes) |
| identities | u32 byte length; canonical IdentityRegistry bytes |
| names | u32 byte length; canonical NameRegistry bytes |
| allocations | u32 count; recovery key ID[32], Balance u64, label byte length u32 and bytes, claimed flag u8 (0/1), optional AccountID[32] |
| publications | u32 count; OperationID[32], owner[32], authorization root[32], count u32, height u64 (108 bytes) |
| settlement cursor | next period u64, next period start UTC u64 |
| leases | u32 count; publication ID[32], payer[32], units u32, replicas u8, first period u64, end period u64, onboarding escrow u64, locked escrow u64 (101 bytes) |

State root is SHA-256 of `"CYBOU/STATE" || exact canonical state bytes`.
Network parameters are supplied by verified genesis; they are not another field
in this snapshot. There is no `usage` section, Authority unit, placement map,
provider reliability table or per-audit record.

Sources: [state.h](../../src/cybou/state.h),
[state.cpp](../../src/cybou/state.cpp),
[block_executor.cpp](../../src/cybou/block_executor.cpp),
[storage_lease.cpp](../../src/cybou/storage_lease.cpp).
Existing regressions: `account_create_funds_system_balance_and_roundtrips_state`,
`state_validation_invariants`, `supply_conservation_invariant_check` in
[state tests](../../src/test/cybou_state_tests.cpp), plus lease/settlement attacks
in [resource limits tests](../../src/test/cybou_resource_limits_tests.cpp).
