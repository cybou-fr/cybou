# 07 — BFT consensus core

## Frozen direction

CYBOU uses:

```text
BFT explicit finality
+
operator-controlled validator admission
```

This is a permissioned BFT network with a PoA-style admission policy.

## Equal validator weight

v1:

```text
ValidatorWeight = 1
```

for every active validator.

No stake-weighted consensus power.

## Topology stages

```text
Beta Stage 1 (Frozen):
    Strictly N=4 validators, equal weight = 1
    Quorum threshold = 3 (2f + 1)
    Fault tolerance f = 1
```

A deployment must not claim tolerance of one Byzantine validator with fewer than 4 validators. Sets with $N \ne 4$ are strictly rejected by `ValidateValidatorSet` in Stage 1.

## BFT state machine

The canonical reference implementation is defined in `cybou::BftValidatorNode` and `cybou::BftSimulator`:

- **Height**: Monotonically increasing 64-bit integer, starting at 1 after Genesis.
- **Round**: 32-bit integer per height, starting at 0.
- **Proposer selection**: Deterministic round leader `(height + round) % 4`.
- **Phases**:
  1. `PROPOSE`: Round leader broadcasts `BftProposalMsg` signed over `CYBOU/BFT_PROPOSAL/V1`.
  2. `PREVOTE`: Nodes validate proposal against canonical chain tip and locking rules. If valid, broadcast `BftPrevoteMsg` for `block_id`; otherwise prevote `nil`.
  3. `PRECOMMIT`: When $\ge 3$ prevotes received for `block_id`, node locks on `(block, round)` and broadcasts `BftPrecommitMsg` signed over `CYBOU/BFT_COMMIT/V1`. If $\ge 3$ prevotes received without quorum for a single block (or timeout), precommit `nil`.
  4. `FINALIZED`: When $\ge 3$ precommits received for `block_id`, their signatures directly assemble `BftFinalityCertificateV1`. The node packages `FinalizedBlockV1{block, cert}`, advances to height + 1, and resets round to 0.

## Safety invariant

Two honest nodes must never finalize different blocks at the same height. Quorum intersection guaranteed by $N=4, Q=3, f=1$: any two quorums of 3 share at least 2 nodes ($3 + 3 - 4 = 2$), containing at least 1 honest node ($2 - 1 = 1$), which prevents conflicting locks.

## Admission

Validator admission/removal requires an explicit operator-authorized protocol state transition.
Operator approval decides who may enter the set.
Once active, a validator's vote is handled by deterministic BFT rules.

## Simulator

Implemented and verified in `cybou::BftSimulator` (`src/test/cybou_bft_tests.cpp`):
- Unanimous 4-node consensus;
- $f=1$ crash fault tolerance (1 node offline, 3 nodes finalize smoothly);
- Leader crash and round timeout advance to round 1;
- Network partition safety (2 vs 2 cannot reach quorum of 3, preventing split-brain);
- Partition healing and subsequent finalization.
