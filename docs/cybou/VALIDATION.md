# VALIDATION — Eligible-Identity Validation signatures

Status: **Active Level 2 normative protocol specification**.

## Principle

Validation is not validation instead of the node. It is an additional
signature produced after a node has independently validated an operation.

```text
Every full node independently validates every candidate operation.
An Identity with finalized AUTH > 1,000,000 may additionally sign an
operation its own node has independently validated.
That signature is pre-finalization evidence.
Every receiving node still independently validates the operation.
PoA also independently validates the operation.
Only PoA finalization is canonical.
```

## Processing a candidate operation

```text
receive Operation
-> canonical decode, Identity signature and authorization checks
-> execute against local finalized state + locally accepted candidates
-> invalid: reject, do not relay, never sign
-> valid:   keep in bounded volatile pool, relay
-> local Identity unlocked and finalized AUTH > 1,000,000?
   yes: may sign and relay a ValidationAttestation
```

## Receiving a ValidationAttestation

```text
receive Operation + ValidationAttestation
-> independently validate the Operation (as above); invalid -> discard both
-> validator AccountID exists in local finalized state
-> its finalized AUTH > 1,000,000
-> hybrid Authorization signature verifies under its current Authorization key
-> keep as Validation evidence
```

A signature never authorizes relay of an otherwise invalid operation.

## Eligibility

```text
validation_eligible(identity) :=
    latest_finalized_state.accounts[identity].authority > 1,000,000 AUTH
```

999,999 and 1,000,000 AUTH are not eligible; 1,000,001 is. There is no
validator registry, ValidatorSet, `CAP_VALIDATOR`, validator key or staking.

## ValidationAttestation

```text
ValidationAttestation:
    version                  1
    network_binding          NetworkBinding (32-byte binding of NetworkID)
    operation_id             OperationID
    finalized_base_block_id  BlockID of the finalized state the operation was executed on
    validator_account_id     AccountID
    signature                Ed25519 + ML-DSA-44 (Identity Authorization key)
```

The signature covers domain `CYBOU/VALIDATION/V1`, NetworkBinding, OperationID,
finalized base BlockID and validator AccountID. It is stored beside the
operation and never changes its OperationID. Exact wire encoding is defined
with the implementation.

Nodes keep Validation in a bounded volatile store keyed by OperationID, with
one signature per AccountID, and drop entries when the operation finalizes or
fails.

## Status

```text
Validated := operation locally valid
             + at least one valid eligible ValidationAttestation
```

`Validated` is informational. It never changes balances, AUTH or any state.

## PoA

Validation signatures may accompany candidate operations. PoA does not trust
them; it independently executes every operation, and only a valid PoA-finalized
block is canonical. If PoA does not finalize a Validated operation, nothing
needs rolling back because Validation created no state.

## Invalid Validation

A ValidationAttestation over an operation that is invalid against the stated
finalized base block is verifiable evidence. Automatic AUTH penalties for it
are not frozen; see [`57_IDENTITY_AUTHORITY.md`](57_IDENTITY_AUTHORITY.md).
