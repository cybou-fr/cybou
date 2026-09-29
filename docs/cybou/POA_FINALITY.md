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

The operator supplies a dedicated 24-word recovery phrase for the PoA finalizer.
Its entropy derives the `POA_FINALIZER` hybrid key purpose through the existing
HKDF-SHA256 key derivation, with the `POA_FINALIZER` purpose label. Its public
key is committed by the network definition at genesis. It is independent of
every CYBOU Identity, Validator, Operator Authority, Release Signing, and
Treasury key. Private signing material is never persisted and is cleansed when
the finalizer session ends.
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
field with the independently reconstructed canonical block. Cross-implementation
key-derivation, digest, signature, and encoding vectors remain required before
cutover.
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
The journal contains no private key. The current implementation provides the
pre-sign intent and canonical-tip check; finalizer runtime integration and
independent-node detection of conflicting valid certificates remain open.

If a node verifies two valid PoA signatures for different blocks at the same
height and parent, it enters a permanent safety halt. It does not select a fork,
retry with a mini-BFT protocol, or recover automatically. The operator must
investigate and perform an explicit network recovery procedure outside normal
startup. A rolled-back or conflicting anti-equivocation journal also halts.

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
