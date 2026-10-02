# PoA finality target

Status: **Active PoA protocol**.
This document defines the single-operator hybrid-PQ PoA finality model, Authority
key rotation, anti-equivocation journaling, and verification rules.

---

## 1. Trust model

Finality is single-operator hybrid-PQ PoA, operated from the Central Authority
desktop. Finality signifies that the active Authority key signed the canonical next block.
It is not Byzantine fault tolerance and carries no BFT consensus overhead.

Every full node independently verifies:
- signature validity against the active Authority key;
- block height, parent block ID, and timestamp;
- valid execution of all included operations;
- deterministic state root recomputation.

There is no ValidatorSet, staking weight, validator admission operation, BFT
round, vote, or quorum.

---

## 2. Authority key chain and monotonic rotation

Genesis binds the network to the initial Central Authority public key $K_0$.
Operational signing keys can rotate monotonically without resetting the network:

```text
Genesis:
    Initial Authority key K0

Heights 1 .. h_1:
    Signed by K0

Rotation record:
    K0 authorizes K1 (epoch 1)

Heights (h_1 + 1) .. h_2:
    Signed by K1
```

Derivation of the current active Authority key $K_{\text{epoch}}$ always traces
back to compiled $K_0$ via finalized, signed rotation records.

The PoA finalizer key role is separate from Identity recovery, authorization,
KEM, Release Signing, and Treasury keys. Private PoA material never leaves the
Central Authority operator machine.

---

## 3. Hybrid-PQ signing and certificate structure

PoA block signatures follow the hybrid post-quantum standard:
**Ed25519 AND ML-DSA-65** are both required and verified. No classical-only fallback
is permitted.

The certificate binds:
```text
version_u8 || NetworkID || BlockID || height_u64le || parent_block_id || Ed25519_sig || ML-DSA-65_sig
```

Signatures are 64 and 3,309 bytes, respectively. Verification checks both
cryptographic components against the active Authority public key and independently
validates the block contents.

---

## 4. Anti-equivocation and permanent conflict halt

The Central Authority maintains a durable local journal of signing intents:
`network, PoA key ID, height, parent, block ID`.

1. An intent must be committed to durable storage **before** signing.
2. If two distinct valid PoA certificates are ever observed for the same height and parent:
   - All observing full nodes enter an immediate, permanent safety halt.
   - The conflict detector records revalidated cryptographic evidence.
   - Nodes do not branch-hop, fork-choice, or attempt automatic recovery.
   - The operator must investigate the equivocation out-of-band.
