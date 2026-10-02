# PoA finality target

Status: **Active PoA protocol**. Finality is single-operator hybrid-PQ PoA
under the current Network-Root-authorized operational key `P_epoch`. It is not
BFT and has no ValidatorSet, vote, quorum or staking weight.

Genesis defines the network and initial state. The active signing key comes
from a verified root-signed Authority assignment `{epoch, activation_height,
P}`. Each historical block is verified against the assignment effective at
its height. Assignments must be monotonic and non-overlapping; a key cannot
appoint its successor. `R` authenticates assignments but never signs blocks.

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

The signatures are 64 and 3,309 bytes respectively. The verifier resolves
`P_epoch` for the block height from the authenticated assignment history and
checks both signatures. The PoA key role remains separate from Identity
Recovery, Authorization, KEM, Network Root, Release Signing and Treasury.

## Signing journal and conflict halt

The finalizer durably records an intent keyed by `NetworkID`, `authority_epoch`,
PoA key ID, height and parent, including the intended BlockID, before signing.
It must fail closed if journal recovery, rollback or signer exclusivity is
uncertain. One operational key has only one active signer.

If two distinct valid certificates exist for the same network, height and
parent, observing full nodes record verified evidence and enter a permanent
safety halt. They do not branch-hop or automatically select a winner. Rotation
does not erase or merge the signing history of prior epochs.
