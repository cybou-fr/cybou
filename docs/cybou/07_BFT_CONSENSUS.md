# 07 — BFT consensus core

Implementation status (2026-09-28): the core BFT engine and certificate
verification are tested across validator-set sizes. The standalone
`cybou-node` now drives rounds for N>1, routes proposal/vote messages through
the outbound CYP2 worker, and has a socket integration test for 3/4 finality
after the round-0 leader is unavailable. The test uses explicit peer
configuration. The socket test also covers verified catch-up, outbound
disconnect/reconnect, and the next height's 4/4 finality.
Signed proposals from the elected leader never advance a validator's round by
themselves. A verified future proposal is held in a bounded buffer (at most 64
rounds ahead and 64 proposals total). The node processes it only after the
local timeout reaches that round or verified votes from distinct validators
provide the round-advance threshold below. This prevents an elected Byzantine
leader from walking peers through a future-round staircase with proposals
alone; proposal validity and lock compatibility are checked when the buffered
proposal is processed in its round.
Verified future prevotes or precommits from distinct validators can also advance
the local round once `RoundAdvanceThreshold = N - quorum + 1` is reached. This
is `f + 1` for the supported equal-weight quorum, so at least one signer is
honest under the configured fault budget. It is only round-progress evidence:
proposal acceptance, locking, precommit, and finality retain their existing
proposal-validation and `floor(2N/3)+1` quorum requirements. In particular, a
single Byzantine validator cannot advance peers by signing arbitrary future
rounds.
Receiving a proposal or producing a precommit advances the local timeout
phase immediately. The CBS2 signing journal durably records height, round,
step, locked round, and locked block. After restart, a validator resumes at the
recorded round without repeating a signing step and enforces its recovered lock.
Legacy CBS1 journals have no lock data and conservatively prevent further
signing at that height. Unit and socket tests cover journal recovery, restart,
3/4 finality, catch-up, and duplicate/conflicting signed prevotes. The
four-process smoke test adds OS-process restart and churn coverage, including
resuming one validator after the live pair has stalled through at least five
rounds. It is not yet a reliable liveness gate: in a repeated run against
`015fd86`, 7 of the first 8 executions passed, while one timed out 90 seconds
after resuming a validator. The active processes remained at the prior finalized
height; a signing journal showed the next height at round 26 with
`locked_round=15`. A following run passed, confirming this is intermittent and
not cleared by a single green run. The requested 20 consecutive passes have not
been established.

The failure points at an unresolved lock-recovery question. `BftProposalMsg`
currently carries no `valid_round` or prevote-quorum certificate. A validator
rejects a proposal that conflicts with its lock and prevotes nil; the current
polka-unlock path depends on that validator locally accumulating a quorum for
the proposed block. Confirm or reject this failure mechanism from complete
per-validator traces before changing consensus rules. A fix must have a
deterministic regression test, preserve the no-conflicting-finality invariant,
and pass the repeated four-process liveness run. Do not describe multi-validator
liveness as verified until that gate passes. See `26_IMPLEMENTATION_STATUS.md`
for the current deployment boundary.

## Frozen direction

CYBOU uses:

```text
BFT explicit finality
+
operator-controlled validator admission
```

This is a permissioned BFT network with a PoA-style admission policy.

## Equal validator weight

```text
ValidatorWeight = 1
```

for every active validator.

No stake-weighted consensus power.

## Topology modes & quorum

CYBOU uses a unified BFT consensus engine across all deployment sizes ($N \ge 1$), eliminating the need for temporary PoA branches or format conversions:

```text
Operator Authority
      │
      └── defines admissible ValidatorSet
                  │
                  ▼
             BFT engine
                  │
        ┌─────────┼─────────┐
        │         │         │
      N = 1    N = 2..3   N >= 4
        │         │         │
    Authority Integration  Byzantine BFT
    mode      mode        mode
    1/1       2/2, 3/3    3/4, 4/5, 5/6, 5/7 ...
    f = 0     f = 0       f >= 1
```

### Consensus modes

1. **Authority Mode ($N = 1$)**:
   - Single authorized validator (`ConsensusMode::AUTHORITY`);
   - Quorum threshold = 1 (1/1 commit vote);
   - Fault tolerance $f = 0$;
   - Leader index is always node 0;
   - Produces canonical `BftFinalityCertificate` with 1 commit vote;
   - Desktop and observer nodes verify finality certificates identically.

2. **Integration Mode ($N \in \{2, 3\}$)**:
   - Development & staging clusters (`ConsensusMode::INTEGRATION`);
   - Quorum threshold = 2 ($N=2$) or 3 ($N=3$);
   - Fault tolerance $f = 0$ (all nodes must agree; crash of any node pauses progress).

3. **Byzantine BFT Mode ($N \ge 4$)**:
   - Target production decentralized operation (`ConsensusMode::BFT`);
   - Quorum threshold: $\lfloor 2N / 3 \rfloor + 1$;
   - Fault tolerance: $f = \lfloor (N - 1) / 3 \rfloor \ge 1$;
   - $N=4 \implies Q=3, f=1$;
   - $N=5 \implies Q=4, f=1$;
   - $N=6 \implies Q=5, f=1$;
   - $N=7 \implies Q=5, f=2$.

A deployment must not claim tolerance of one Byzantine validator with fewer than 4 validators. Sets with $N < 1$ are strictly rejected by `ValidateValidatorSet`.

For an N=1 Beta deployment, the Operator Validator private key requires an
encrypted offline recovery copy. If that sole validator key is irretrievably
lost, the network cannot finalize a validator admission block. Operator
Authority alone cannot restore finality.

### Quorum formula

Quorum is calculated directly with integer arithmetic:

```text
quorum = (2 * N) / 3 + 1
```

Fault tolerance is an informational property:

```text
f = N == 0 ? 0 : (N - 1) / 3
```

Wire formats (`CybouBlock`, `BftFinalityCertificate`), state transitions, and validator set serialization remain strictly identical across all modes. Transitioning from $N=1$ to $N \ge 4$ requires only an operator-authorized `ValidatorSet` update without changing the consensus engine or block structures.

## BFT state machine

The canonical reference implementation is defined in `cybou::BftValidatorNode` and `cybou::BftSimulator`:

- **Height**: Monotonically increasing 64-bit integer, starting at 1 after Genesis.
- **Round**: 32-bit integer per height, starting at 0.
- **Proposer selection**: Deterministic round leader `(height + round) % N`.
- **Phases**:
  1. `PROPOSE`: Round leader broadcasts `BftProposalMsg` signed over `CYBOU/BFT_PROPOSAL/V1`.
  2. `PREVOTE`: Nodes validate proposal against canonical chain tip and locking rules. If valid, broadcast `BftPrevoteMsg` for `block_id`; otherwise prevote `nil`.
  3. `PRECOMMIT`: At the validator-set quorum of prevotes for `block_id`, the node locks on `(block, round)` and broadcasts `BftPrecommitMsg` signed over `CYBOU/BFT_COMMIT/V1`. If a quorum forms without one block (or timeout), it precommits `nil`.
  4. `FINALIZED`: At the validator-set quorum of precommits for `block_id`, their signatures assemble `BftFinalityCertificate`. The node packages `FinalizedBlock{block, cert}`, advances to height + 1, and resets round to 0.

## Safety invariant

Two honest nodes must never finalize different blocks at the same height.
For $N=4, Q=3, f=1$, any two quorums of 3 intersect in at least 2 validators
($3 + 3 - 4 = 2$), including at least 1 honest validator under the fault
assumption. This intersection is necessary, but does not prove safety by
itself: the proposal, prevote, and lock-transition rules must ensure the honest
intersection member cannot support conflicting finality certificates. That
proof and the observed intermittent lock-recovery liveness failure remain open
for the current implementation.

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
