# PoA finality target

Status: **Active PoA protocol**. Finality is single-operator hybrid-PQ PoA
under the genesis-authorized operational key `P`. It is not BFT and has no
ValidatorSet, vote, quorum or staking weight.

## Operational model

Genesis defines the network, initial state, initial Authority baselines, and the
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
parent, observing full nodes record verified evidence and enter a permanent
safety halt. They do not branch-hop or automatically select a winner.

## Conflict resolution with provisional Validation

PoA is the sole canonical truth:
- If provisional Validation state agrees with PoA: promote provisional state to finalized.
- If provisional Validation state conflicts with PoA: discard provisional state,
  rollback provisional effects, and adopt PoA-finalized state unconditionally.
- There is no validator fork-choice, no BFT voting, and no validator quorum override.
