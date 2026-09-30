# Future Validation architecture

Status: design only. No C++ implementation, wire profile or Beta dependency.
[VALIDATION.md](VALIDATION.md) is the behavioral authority. The interfaces below
are proposed boundaries, not active protocol types or frozen serialization.

## Independent paths

IdentityOperationCoordinator journals and submits exact authorized operation
bytes. Submission continues directly to the PoA finalizer. An optional
ValidationService sidecar may inspect the same bytes. PoA ignores its results
and independently executes its candidate batch before signing finality.

ValidationService MUST NOT use OperationPool::Admit or its candidate overlay.
OperationPool can accept nonce n+2 after pending n+1; Validation v1 accepts
only n+1 against finalized nonce n. Validation never reserves canonical funds,
advances a nonce, changes Authority or admits remote chunks.

## Proposed C++ boundaries

```cpp
struct FinalizedValidationBase {
    uint256 network_id;
    uint64_t height;
    uint256 block_id;
    uint256 state_root;
    // Immutable state snapshot captured atomically with the fields above.
};
struct ValidationReservationKey {
    AccountId account_id;
    uint64_t nonce;
    uint256 network_id;
    uint64_t base_height;
    uint256 base_block_id;
    uint256 base_state_root;
};
enum class ValidationCheck { VALID, INVALID, CONFLICT, BASE_CHANGED, UNAVAILABLE };
struct ValidationAttestation {
    uint256 network_id;
    uint256 operation_id; // Recomputed from canonical exact operation bytes.
    uint64_t base_finalized_height;
    uint256 base_block_id;
    uint256 base_state_root;
    NodeID validator_node_id; // Future separately specified service identity.
    uint32_t profile_version;
    ValidationCheck result;
    // Bounded dedicated-node-key signature; suite/encoding not yet selected.
};
class ValidationState {
    // Atomic, durable first-writer reservation BEFORE signing.
    // Same OperationID is idempotent; a different id is CONFLICT.
    ReserveResult Reserve(const ValidationReservationKey&, const uint256& operation_id);
};
class AttestationStore {
    // Bounded verified evidence, separate from canonical state and Qt models.
    StoreResult PutVerified(const ValidationAttestation&);
};
class ValidationService {
    ValidationCheck ValidateAgainstFinalizedBase(
        const ProtocolOperation&, const FinalizedValidationBase&) const;
    AttestResult CheckReserveAndAttest(const ProtocolOperation&);
    VerifyResult VerifyAttestation(const ValidationAttestation&) const;
};
```

NodeID and result wrapper types are placeholders requiring separate design.
ValidateAgainstFinalizedBase uses deterministic operation checks on a disposable
copy of ONE finalized snapshot with no pending operations. It checks exact
encoding, NetworkID, authorization, key epoch, nonce, fees, commitments and all
operation-specific rules. It never invokes canonical commit or storage PUT.
Operations without a specified Identity nonce profile remain out of v1 scope.

CheckReserveAndAttest captures the latest base atomically, checks the operation,
reserves durably, then verifies that the base has not changed before signing.
Concurrent same-nonce requests must serialize through the reservation store.
After a crash, replay of the same exact operation is idempotent; unreadable,
corrupt or uncertain reservation state forbids positive attestations. A base
change produces BASE_CHANGED and requires a fresh check. Historical evidence
never qualifies as validation against a newer base. Retention, cleanup bounds
and power-loss durability details require specification before implementation.

## NodeID and eligibility

A future canonical binding associates a dedicated NodeID/key with AccountID.
It is neither a device registry nor an Identity credential. It gives no access
to Recovery, Authorization, KEM or private application data. ProviderID proves
a storage role only and does not establish NodeID eligibility. Binding,
rotation/revocation, immutable Authority eligibility and resource limits must
be specified before any validator daemon or network message is introduced.

## Verification and projection

Attestation verification checks the future signature domain/profile, dedicated
node signature, network, exact OperationID, finalized base, binding and local
trust policy. Distinct NodeIDs must not silently count as independent Accounts.
Signature validity alone never proves an operation valid. Client policy starts
OFF; thresholds, expiry and transport remain unresolved.

CybouOperationStatus is only a presentation projection. Evidence belongs in
AttestationStore and reservations in ValidationState. Verified finality always
supersedes local assessment. Failed forbids informational retry for the exact
id; uncertain delivery remains Submitted. Remote storage starts only after a
verified finalized RootPublication, and Protected requires remote durability.

## Required future acceptance cases

- Finalized nonce 10: nonce 11 valid, nonce 12 rejected despite pool overlay.
- Same nonce/different bytes: conflict, including concurrent calls and restart.
- Same exact bytes: idempotent reservation; no changed OperationID on retry.
- Wrong network/base/key epoch/signature: no positive attestation.
- Base advances during checking: no attestation against a purported latest base.
- Corrupt/unavailable reservation journal: fail closed.
- Replayed, expired or unbound-node evidence: no Validated projection.
- Validation OFF or unavailable: direct PoA submission still works.
- Validated never changes balances, Authority, admission or durability.

Authority enforcement additionally requires immutable NetworkID-bound policy
and canonical accumulators or a fully specified consensus-deterministic model.
A persisted Authority preview cache satisfies neither requirement.
