# Canonical chain state

Full nodes execute finalized blocks deterministically and derive the same state
root. Mail and Files do not create permanent per-message/per-file consensus
objects.

## Current state domains

- account values: Balance and System Balance in CYBOU, Authority in AUTH;
- Identity registry: stable AccountID, Recovery/Authorization capabilities,
  current KEM commitment, nonce and key epoch;
- `.cybou` name registry;
- economic pools and deterministic fee accounting;
- genesis allocations containing initial CYBOU and AUTH;
- immutable network parameters bound to the active network definition.

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

## Authority

Authority is a canonical non-transferable AUTH account value, included in the
state root and excluded from CYBOU TotalSupply. AUTH changes only through
deterministic finalized state transitions: GenesisAllocation claimed once by
AccountCreate, +1 AUTH per finalized Identity-authorized operation, and
PoA-signed `PoaAuthAdjustment` GRANT / BURN (floor 0). There is no derived Authority
index. Its sole protocol eligibility effect is qualifying an Identity to sign
Validation when its latest finalized `AccountState.authority > 1,000,000`.

Detailed Authority policies are defined in [`57_IDENTITY_AUTHORITY.md`](57_IDENTITY_AUTHORITY.md).

## Invariants

- no plaintext Mail/File metadata in consensus;
- no per-Mail/per-file canonical state object;
- no wall-clock consensus arithmetic;
- Authority never grants PoA finalization weight or consensus voting power;
- Balance, System Balance and Authority are three distinct canonical account values;
- all consensus state arithmetic is bounded integer arithmetic.
