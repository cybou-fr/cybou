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

## Identity Authority target

Identity Authority supersedes the earlier PoT design. Its exact state encoding
is not yet implemented.

Target accounting separates earnings from penalties.

Conceptually:

```text
AuthorityState
    activity_points
    system_contribution_points
    storage_points              # enabled only with canonical evidence
    penalty_points

    activity_epoch
    activity_points_this_epoch
    storage_remainder
```

Age is derived:

```text
AgeAuthority = current_epoch - creation_epoch
```

Liveness may be derived/credited only from canonical evidence for bound NodeIDs.

Effective Authority:

```text
max(0, earned_points - penalty_points)
```

Penalty debt is retained even while effective Authority is zero.

No operation allows an Identity or PoA operator to arbitrarily set an
individual score.

## Node binding target

A bounded future registry may associate dedicated service NodeIDs with AccountID
for liveness/storage contribution accounting.

A NodeID binding is not a device authorization mechanism and never changes
Identity key authority.

## Invariants

- no plaintext Mail/File metadata in consensus;
- no per-Mail/per-file canonical state object;
- no wall-clock consensus arithmetic;
- Authority never grants PoA finalization weight;
- Balance, System Balance and Authority are distinct resources;
- all state arithmetic is bounded integer arithmetic.
