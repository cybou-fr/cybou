# Canonical chain state

Full nodes execute finalized blocks deterministically and derive the same state
root. Mail and Files do not create permanent per-message/per-file consensus
objects.

## Current state domains

- monetary accounts: Balance and System Balance;
- Identity registry: stable AccountID, Recovery/Authorization capabilities,
  current KEM commitment, nonce and key epoch;
- `.cybou` name registry;
- economic pools and deterministic fee accounting;
- genesis allocations and initial Authority baselines;
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

Authority is a deterministic property derived exclusively from PoA-finalized history
and state. Initial Authority baselines may be assigned at genesis (e.g., DEV bootstrap
Identity initial Authority = 1,000,001).

Authority is not a canonical currency and does not allocate spendable Balance,
system resources, or PoA finalization power. Its sole protocol eligibility effect is
qualifying an Identity to sign provisional Validation attestations when
`Authority > 1,000,000` in the latest finalized state.

Detailed Authority policies are defined in [`57_IDENTITY_AUTHORITY.md`](57_IDENTITY_AUTHORITY.md).

## Invariants

- no plaintext Mail/File metadata in consensus;
- no per-Mail/per-file canonical state object;
- no wall-clock consensus arithmetic;
- Authority never grants PoA finalization weight or consensus voting power;
- Balance, System Balance and Authority are distinct concepts;
- all consensus state arithmetic is bounded integer arithmetic.
