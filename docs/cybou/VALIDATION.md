# VALIDATION — Advisory validation

Status: **Active Level 2 normative protocol specification**.

Validation is optional, advisory pre-finalization evidence. It allows peers to
gain confidence in candidate operations before canonical PoA finality.
Validation NEVER produces canonical state and NEVER overrides PoA finality.

## Core invariant

```text
Validation is provisional evidence,
not an alternative finality mechanism.
```

If Validation conflicts with PoA:
- discard provisional state;
- rollback provisional effects;
- adopt PoA-finalized state unconditionally.

There is NO voting against PoA, NO validator fork-choice, NO validator quorum
finality, and NO merge of conflicting provisional state.

## Eligibility

Validation attestations are valid only if signed by an eligible Identity.
Eligibility is deterministic and evaluated strictly against the latest
**PoA-finalized** state:

```text
validation_eligible(identity) :=
    authority_from_latest_PoA_finalized_state(identity) > 1,000,000
```

Signatures from Identities whose Authority in provisional state exceeds 1,000,000,
but does not exceed 1,000,000 in the latest PoA-finalized state, are invalid
and MUST be rejected.

Authority metric never grants PoA finalization power.

## Validation attestation structure

An eligible validator issues a signed attestation for a candidate operation
or block candidate:

```text
ValidationAttestation:
    network_id: uint256
    operation_id: uint256
    finalized_base_block_id: uint256
    validator_account_id: AccountId
    validator_signature: Ed25519 + ML-DSA-44 (Identity Authorization Key)
```

The signature is created using the Identity's active Authorization Key role.
Validators require no special validator-only key role.

## Local acceptance policy

Full nodes configure their validation policy locally:

```text
validation.enabled = true / false (peer preference)
validation.min_signatures = N (default = 1)
```

- A peer with `validation.enabled = false` waits exclusively for PoA finality.
- A peer with `validation.enabled = true` and `min_signatures = 1` may treat
  operations with at least 1 valid eligible signature as `Validated` / provisional.
- Even if a candidate has 10, 100, or 1000 Validation signatures, a single valid
  PoA block rejection or conflict unconditionally terminates and rolls back
  the provisional state.

## Provisional state and mandatory rollback

Provisional effects:
- UI displays operation status as `Validated` (informational);
- balances and spendable funds remain UNCHANGED until PoA finality;
- storage providers with provisional policy enabled may optionally cache/stage
  authorized chunks;
- on PoA inclusion: promote provisional state to `Finalized`;
- on PoA rejection or conflict: purge provisional cached chunks, rollback
  provisional state, and adopt canonical PoA state.
