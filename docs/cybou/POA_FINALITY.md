# PoA finality target

Status: active PoA protocol. This document defines the genesis-bound PoA
format implemented by `main`; it supersedes the former BFT target. The target
deployment runs the genesis-key holder on the Central Authority desktop; the
current DEV deployment has not completed that migration. See
[`04_NETWORK_BOOTSTRAP_AND_GENESIS.md`](04_NETWORK_BOOTSTRAP_AND_GENESIS.md).
It does not authorize a compatibility decoder.

## Trust model

The target DEV network has one genesis-bound PoA finalizer operated from the
Central Authority desktop. Finality therefore means that the configured PoA
key signed the canonical next block; it is not Byzantine fault tolerance and
must never be marketed as such.
Every full node still validates the signature, parent, height, operations,
state transition, and recomputed state root independently.

There is no ValidatorSet, staking weight, validator admission operation, BFT
round, vote, quorum, or distributed pending-operation pool in the target
protocol. Unfinalized operations remain local to their origin and the PoA
finalizer. A node may relay a direct submission without storing it as shared
pending state.

## PoA signing role

The Central Authority Identity's recovery entropy derives its role-specific
`POA_FINALIZER` hybrid key through the existing HKDF-SHA256 derivation and
purpose label. Its public key is committed by the network definition at
genesis. This is a local capability of an ordinary full node, separate from
its optional bootstrap, storage, or advisory Validation capabilities. It is a
distinct key role, separate from Identity recovery,
authorization and KEM keys, Release Signing, and Treasury. In the target
deployment, the desktop unlocks this Identity and runs the finalizer locally;
the private material is never sent to bootstrap. The current legacy DEV still
loads operator entropy on the VPS until cutover.
Its journal and operator diagnostics identify the public key by
`SHA256(CYBOU/POA-FINALIZER-KEY-ID/V1 || Ed25519_public_key ||
ML-DSA-65_public_key)`; the genesis definition still commits the full public
key.

PoA block signatures follow the repository's hybrid-PQ policy: Ed25519 **and**
ML-DSA-65 are both required and verified. No classical-only fallback is
allowed. The signature digest is domain-separated and binds NetworkID, block
ID, height, and parent block ID. Version 1 hashes
`CYBOU/POA_FINALITY/V1 || NetworkID || BlockID || height_u64le || parent_block_id`
with SHA-256, then signs the 32-byte digest with both components. The fixed
certificate encoding is `version_u8 || NetworkID || BlockID || height_u64le ||
parent_block_id || Ed25519_signature || ML-DSA-65_signature`; signatures are
64 and 3,309 bytes, respectively. Verification also compares every certificate
field with the independently reconstructed canonical block. Key-derivation,
digest, Ed25519, ML-DSA-65, key-ID, and certificate-encoding vectors are
recorded in `POA_FINALITY_VECTORS.md` and were independently reproduced using
PyNaCl/libsodium, `dilithium-py`, and Python's standard hash implementation.
Production signing retains randomized ML-DSA behavior.
Use the existing consensus hash primitive for block/finality IDs; BLAKE3 is
introduced only for content-addressed encrypted chunks and their proofs.

## Anti-equivocation and chain acceptance

The finalizer durably journals the network, PoA public-key identity, height,
parent, and block ID before signing. The journal stores its identity binding and
one current intent, so storage does not grow with chain height. Startup accepts
the finalized tip only when it equals the journaled block or its direct parent;
the latter permits retrying the same prepared-but-unfinalized block. A new
intent requires the previous journaled block to be finalized, the next height,
and its exact block ID as parent. Recovery fails closed if journal and finalized
history disagree; a journal conflict can never be cleared by normal startup.
The journal contains no private key. The runtime wires both protections into
canonical block acceptance and block production. `PoaFinalizer` verifies the
supplied operator recovery entropy against the genesis key, retains it only in
RAM, checks canonical block serialization, persists the intent, and only then
signs. The state store sends each verified finality certificate to
`PoaConflictDetector` before chain-tip checks, so a valid conflicting
certificate is detected even when its block is on a different branch. The
detector durably records observations and permanently halts with both
certificates on equivocation. Runtime status exposes the halt, operation
submission and block production stop, and a read-only API returns revalidated
evidence.

If a node verifies two valid PoA signatures for different blocks at the same
height and parent, it enters a permanent safety halt. It does not select a fork,
retry with a mini-BFT protocol, or recover automatically. The operator must
investigate and perform an explicit network recovery procedure outside normal
startup. `PoaConflictDetector::ReadSafetyEvidence` returns both revalidated
certificates for an equivocation halt; a corruption halt is reported without
inventing certificate evidence. This read-only inspection does not clear the
halt. A rolled-back or conflicting anti-equivocation journal also halts.

## Genesis and cutover

The network definition contains the genesis block/state, protocol parameters,
and hybrid PoA public key. It contains no initial ValidatorSet commitment or
separate Operator Authority key. The Central Authority remains an ordinary
protocol Identity; only its distinct genesis-bound `POA_FINALIZER` role signs
blocks. This role does not derive from the Authority score.

The running DEV chain is a development testnet; no production or Beta network
exists. It has not yet completed the bootstrap/desktop-finalizer architecture
cutover. Routine code deployments preserve its current test state and PoA key.
The planned cutover defined in `04_NETWORK_BOOTSTRAP_AND_GENESIS.md` replaces
that testnet with a new genesis and target topology after acceptance gates.
Runtime compatibility and old-state import are not required for that new
genesis.

\n
