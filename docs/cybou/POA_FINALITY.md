# PoA finality target

Status: frozen architecture target; not implemented and not active on DEV. This
document supersedes the BFT consensus design for the next coordinated DEV
protocol cutover. It does not authorize a partial transition or runtime
compatibility decoder.

## Trust model

The next DEV network has one genesis-bound PoA finalizer operated by CYBOU.
Finality therefore means that the configured PoA key signed the canonical next
block; it is not Byzantine fault tolerance and must never be marketed as such.
Every full node still validates the signature, parent, height, operations,
state transition, and recomputed state root independently.

There is no ValidatorSet, staking weight, validator admission operation, BFT
round, vote, quorum, or distributed pending-operation pool in the target
protocol. Unfinalized operations remain local to their origin and the PoA
finalizer. A node may relay a direct submission without storing it as shared
pending state.

## PoA signing role

The PoA signing role is derived in memory from a dedicated operator recovery
mnemonic domain. Its public key is committed by the network definition at
genesis. It is independent of every CYBOU Identity, Release Signing, and
Treasury key. Private signing material is never persisted and is cleansed when
the finalizer session ends.

PoA block signatures follow the repository's hybrid-PQ policy: Ed25519 **and**
ML-DSA-65 are both required and verified. No classical-only fallback is
allowed. The signature digest is domain-separated and binds NetworkID, block
ID, height, and parent block ID. Exact encoding and key derivation require
cross-implementation vectors before cutover.
Use the existing consensus hash primitive for block/finality IDs; BLAKE3 is
introduced only for content-addressed encrypted chunks and their proofs.

## Anti-equivocation and chain acceptance

The finalizer durably journals the network, PoA public-key identity, height,
parent, and block ID before signing. Recovery fails closed if journal and
finalized history disagree; a journal conflict can never be cleared by normal
startup. The journal contains no private key.

Because one PoA signature cannot provide BFT fork resolution, deterministic
behavior for conflicting valid signatures, journal rollback, and operator
recovery is a hard cutover gate. Nodes must not choose a fork by arrival order.
Until the gate is specified and tested, a conflicting finalized history is a
fatal network safety incident.

## Genesis and cutover

The network definition contains the genesis block/state, protocol parameters,
and hybrid PoA public key. It contains no initial ValidatorSet commitment or
Operator Authority key. `cybou.cybou` remains an ordinary Identity unrelated
to the PoA signer.

DEV cutover is one coordinated protocol reset only after the complete format,
names integration, finality, state execution, storage admission, and clean-node
recovery gates pass. Preserve the existing DEV executable and validator
materials for rollback; do not run old and new protocol formats together.

\n