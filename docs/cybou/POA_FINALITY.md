# PoA finality target

Status: **Active PoA protocol**. Finality is single-operator hybrid-PQ PoA
under the genesis-authorized operational key `P`. It is not BFT and has no
ValidatorSet, vote, quorum or staking weight.

## Operational model

Genesis defines the network, initial state, initial AUTH allocations, and the
authorized PoA public key `P`.
The Central Authority executes candidate operations independently from its
desktop, trusts no validator or peer state, and publishes finalized blocks.

Every full node independently checks certificate signatures, height, parent,
timestamp, operation execution and deterministic state root. A signature alone
never validates an invalid transition.

## Certificate

PoA requires both Ed25519 and ML-DSA-65. There is no classical-only fallback.
The certificate binds:

```text
version_u8 || NetworkID || BlockID || height_u64le || parent_block_id ||
Ed25519_sig || ML-DSA-65_sig
```

The signatures are 64 and 3,309 bytes respectively. The verifier checks both
signatures against the genesis-authorized PoA key `P`. The PoA key role remains
separate from Identity Recovery, Authorization, KEM, Network Private Key,
Release Signing and Treasury.

## Signing journal and conflict halt

The finalizer durably records an intent keyed by `NetworkID`, PoA key ID, height
and parent, including the intended BlockID, before signing. It must fail closed
if journal recovery, rollback or signer exclusivity is uncertain. One operational
key has only one active signer.

If two distinct valid certificates exist for the same network, height and
parent, observing full nodes record verified equivocation evidence and
deterministically select the canonical winner min(BlockID_1, BlockID_2),
re-orging if the newly observed certificate has a strictly smaller BlockID,
or rejecting it if not. Corrupt storage halts fail closed.

## Independent execution invariant

```text
PoA MUST independently execute candidate operations.
Validation signatures are never sufficient for finalization.

operation -> own validation/execution -> state root -> block -> PoA signature
```

PoA produces blocks from its node's ordinary candidate pool. It may receive
Validation signatures but never relies on them. There is no validator
fork-choice, no BFT voting, and no validator quorum override. Validation creates
no state, so PoA finality never needs to roll back provisional effects.

## Genesis and signer boundary

Runtime and StateStore accept VerifiedNetworkGenesis, not an unsigned network
summary. The genesis specification digest verified by its network signature
is height-zero finalized tip, first-block parent and durable journal anchor.
CybouNodeRuntime executes candidates and constructs the final block;
PoaFinalizer checks the genesis key, persists intent and signs. No producer
wrapper, dedicated transport role or special pending state exists. A signer
whose journal conflicts with the selected canonical history fails closed.
