# Canonical chain state

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
root, chunk count and recipient capsules.

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
its chunks leave the owner's quota. A revoked publication no longer authorizes chunk
admission, and storing nodes purge every chunk no other admitted publication still
authorizes from local `ChunkStore`.
Historical blocks remain immutable, but active consensus state does not accumulate dead objects.

## Invariants

- no plaintext Mail/File metadata in consensus;
- no per-Mail/per-file canonical state object: the register holds one record per
  finalized RootPublication (which may bundle many encrypted trees), with no
  plaintext metadata;
- each Identity's `usage.stored_chunks` equals the sum of its register records;
- no wall-clock consensus arithmetic;
- Authority never grants PoA finalization weight or consensus voting power;
- Balance, System Balance and Authority are three distinct canonical account values;
- all consensus state arithmetic is bounded integer arithmetic.

## Canonical encoding

State is the sole canonical encoding. Allocations
are immutable except for their one-time claimed_by and the pre-claim Balance
of the unique Central Authority allocation. After claim its allocation Balance
stays fixed; new fees credit the claimant account Balance. TotalCybou counts
unclaimed allocation Balances, account Balances, System Balances and lease
escrow, with checked integer arithmetic; every finalized block preserves it
exactly. All sections are always encoded, in order: accounts, identities, names,
allocations, `usage`, `publications`, settlement cursor, `leases`; maps strictly
ordered, no trailing bytes.

## Storage economy (implemented in M5; active from the M7 genesis)

Compared with the superseded DEVNET state:

- OnboardingPool is removed; the whole genesis monetary base is the `cybou`
  Treasury allocation; AccountCreate transfers `onboarding_bonus` from Treasury
  to the new System Balance.
- `TotalSupply` becomes `TotalCybou` = unclaimed genesis Balances + Balances +
  System Balances + StorageEscrow; every block preserves it exactly.
- System Balance records its onboarding-origin portion (DEC-281).
- New records: StorageLease (publication, payer, units, replicas, period, escrow
  by origin, status ACTIVE/CLOSING/EXPIRED, rate remainder) and the last settled
  StorageSettlement period.
- per-account usage counters and storage quota checks are removed; the
  publication register remains.
