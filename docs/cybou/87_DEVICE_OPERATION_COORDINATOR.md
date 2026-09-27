# 87 — Device operation coordinator

Status: canonical cross-service authorization and retry contract. The
coordinator is implemented for Name, Wallet, recovery's root-authorized
DeviceAdd and DeviceRevoke, and RecoveryRotate; this document also defines the
migration target for Mail, Files, and future device-authorized services. See
`26_IMPLEMENTATION_STATUS.md` for the exact code boundary.

## Ownership rule

All device-authorized user operations pass through the runtime-owned
`DeviceOperationCoordinator`.

Services MUST NOT independently:

- read `device.next_nonce`;
- choose or advance a device nonce;
- construct or sign `DeviceAuthorization`;
- create replacement bytes after uncertain delivery;
- treat an acknowledgment as BFT finality.

The service supplies only:

```text
operation kind
payload commitment
operation builder(DeviceAuthorization) -> ProtocolOperation
```

The coordinator is the single shared serialization point for a device's
authorization sequence. It reads the finalized nonce, signs the canonical
authorization using the authorized device key, builds and serializes the
operation, computes OperationID, durably journals it, and submits it.

## Lifecycle

The implementation's enum names are retained below so code and contract stay
aligned. `UNCERTAIN` is the user-visible **Delivery uncertain** state;
`CONFLICT` is a reconciliation conflict.

```text
PREPARED
   ↓ exact operation durably journaled
SUBMITTING
   ├── accepted by local/remote admission → ACCEPTED
   ├── reply lost or delivery ambiguous   → UNCERTAIN
   └── explicit rejection                → REJECTED

ACCEPTED → FINALIZED(height)
any unresolved state → CONFLICT on nonce/history disagreement
```

`ACCEPTED` means admission or pending status only. It does not mean the
operation is finalized. Finality is confirmed from verified local state and
the finalized OperationID index.

## Delivery uncertainty invariant

```text
UNCERTAIN != REJECTED
```

When delivery is uncertain, the coordinator MUST preserve:

- the exact serialized operation bytes;
- the OperationID computed from those bytes;
- the device, activation, and nonce binding;
- the payload commitment and network binding.

It MUST NOT build a replacement operation or reuse that nonce for a different
request. It first checks local pending/finalized/rejected status and finalized
device state. If status remains unknown while the finalized nonce is unchanged,
retry is byte-for-byte with the same OperationID. If the finalized nonce has
advanced and the OperationID is absent from available history, the coordinator
enters `CONFLICT`; it does not silently sign another operation at that nonce.

An explicit rejection is distinct from a lost acknowledgment. The service may
offer a corrected request only after the rejection is known and the finalized
device nonce remains available. An unavailable history lookup is not proof of
rejection.

## Durable journal and status lookup

The runtime stores one unresolved device operation at
`device-operation.cydop` in its data directory. The journal is checksummed and
atomically replaced; it contains the exact serialized operation and binding
metadata, never the device private key. The journal is written before network
submission so a restart can continue reconciliation.

`CybouNodeRuntime::GetOperationStatus(OperationID)` reports the runtime's
known local state:

| Status | Meaning |
|---|---|
| `UNKNOWN` | No current local evidence |
| `LOCAL_PENDING` | Present in the local authority admission pool |
| `ACCEPTED_REMOTE` | A remote peer acknowledged admission/pending |
| `FINALIZED(height)` | Found in verified local finalized history |
| `REJECTED_KNOWN` | A recent explicit rejection is known to this runtime |
| `HISTORY_UNAVAILABLE` | Required finalized history cannot be checked |

Recent remote admission/rejection knowledge is in memory and may be lost on
restart. The durable coordinator journal is therefore authoritative for
resuming its own request. A caller must not interpret `UNKNOWN` as rejection.

## Concurrency and nonce reservation

The current coordinator supports one unresolved operation in the runtime's
shared journal. While that operation is accepted, uncertain, or otherwise
unresolved, a different operation is not signed. A repeated identical request
retries or returns the saved result. After finality or known rejection is
reconciled, a subsequent operation reads the next finalized nonce. This
deliberately serializes all coordinated operations in one runtime, including
actions from multiple services.

Future support for multiple outstanding operations requires a durable ordered
nonce reservation journal and explicit cancellation/replacement rules. It must
not be approximated with independent per-service counters.

## Service integration status

| Service operation | Current coordinator path |
|---|---|
| Wallet payment | Integrated |
| Wallet System Balance lock | Integrated |
| Name commit | Integrated |
| Name reveal | Integrated |
| Recovery DeviceAdd | Integrated; root nonce and exact operation bytes are journaled |
| Device revoke | Integrated; root nonce and exact operation bytes are journaled |
| Mail | Not integrated; sending remains fail-closed without recipient hybrid keys |
| Files manifest operations | Not implemented |
| Recovery-root rotation | Integrated; candidate vault is saved before broadcast and promoted only after verified finality |

No Mail/Files capability may claim this contract is operational until its
mutations call the coordinator and the restart/uncertain path is tested.

## Desktop boundary

Qt pages request domain actions and render immutable operation-state snapshots.
They do not allocate nonces, sign operations, select suites, retry wire bytes,
or infer finality. Domain services call the coordinator asynchronously; the
runtime owns submission and status lookup. See `73_CORE_DESKTOP_CONTRACT.md`.
