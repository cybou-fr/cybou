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
- immutable network parameters bound to the active network definition.

Application schemas and private metadata remain encrypted outside canonical
state.

## RootPublication history

RootPublication commits generic encrypted content roots, authorization Merkle
root, chunk count and recipient capsules.

Finalized block history is the discovery/proof source. It does not maintain a
consensus row for each Mail message or file.

Clients rebuild private application projections from finalized publications.

## Authority

Authority is a read-only derived metric over finalized account history. It is
not a canonical state field and does not allocate resources, reward services,
bind node identities, or grant PoA power. Its current informational policy is
defined in `57_AUTHORITY_AND_VALIDATION.md`.

## Invariants

- no plaintext Mail/File metadata in consensus;
- no per-Mail/per-file canonical state object;
- no wall-clock consensus arithmetic;
- Authority and advisory Validation never grant PoA finalization weight;
- Balance, System Balance and Authority are distinct resources;
- all consensus state arithmetic is bounded integer arithmetic.
