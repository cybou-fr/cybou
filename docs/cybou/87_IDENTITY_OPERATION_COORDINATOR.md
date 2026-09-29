# 87 — Identity operation coordinator

Status: canonical cross-service authorization and retry contract.

## Ownership rule

All account-authorized operations pass through the runtime-owned `IdentityOperationCoordinator`. Identity has one shared account nonce and one current key_epoch. Services MUST NOT independently read, advance, or reserve nonces, construct signatures, replace bytes after uncertain delivery, or treat admission acknowledgements as PoA finality.

A service supplies an operation kind, payload commitment, and operation builder that receives `IdentityOperationAuthorization`. The coordinator checks the current finalized Identity record against the unlocked Recovery, Authorization, and KEM material, signs with the current Authorization key, constructs canonical operation bytes, computes OperationID, durably journals those exact bytes, then submits.

## Lifecycle and uncertainty

```text
PREPARED → SUBMITTING → ACCEPTED → FINALIZED(height)
                    ↘ UNCERTAIN
any unresolved state → CONFLICT on nonce/history disagreement
```

`ACCEPTED` means admission or pending status only. It is not finality. `UNCERTAIN != REJECTED`. On uncertainty, preserve the exact serialized bytes, OperationID, AccountID, shared nonce, key_epoch, payload commitment, and NetworkID. Check local pending/finalized/rejected status and verified state. If status remains unknown while the nonce is unchanged, retry byte-for-byte. If the nonce advanced without the journaled OperationID in available history, enter `CONFLICT`; do not silently sign another operation.

## Atomic key rotation

IdentityRotate is journaled through the same single unresolved operation slot. It binds current nonce and epoch plus the next key_epoch and complete replacement key set. The old Recovery key authorizes the transition and new Recovery/Authorization keys prove possession. After finality, the client promotes the pre-saved candidate vault only after matching the full finalized Identity record; then it clears the rotation journal.

## Durable journal and concurrency

The runtime stores one unresolved Identity operation at `identity-operation.cyiop`. The checksummed, atomically replaced journal contains exact operation bytes and binding metadata, never private keys. One unresolved account operation is permitted at a time across Wallet, Names, Mail, Files, and rotation. A subsequent operation reads the next finalized shared nonce only after finality or known rejection is reconciled.

Future parallel outstanding operations require ordered durable nonce reservations and explicit replacement rules; they cannot use service-local counters.

## Service integration

Wallet and Names use the coordinator. Mail and Files mutations must use it before they are enabled. Identity restore is local and does not create an operation. Rotation uses the dedicated atomic IdentityRotate path.

Qt pages request domain actions and render immutable operation status. They do not allocate nonces, sign, retry wire bytes, or infer finality. See [73 — Core/Desktop contract](73_CORE_DESKTOP_CONTRACT.md).
