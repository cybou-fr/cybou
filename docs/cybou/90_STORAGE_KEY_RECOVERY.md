# 90 — Files key recovery and device distribution

Status: architecture design gate; not implemented. The local encrypted Storage
Key Ring is durable on one installation only. This document defines the
recovery and distribution contract needed before Files can claim multi-device
or clean-machine recovery. It does not freeze operation bytes or a storage
availability policy.

## Current behavior and boundary

The local `StorageKeyRing` is a password-protected CYBV2 sidecar bound to
AccountID. It holds contiguous random 256-bit Storage Master Key (SMK) epochs,
retains old epochs, and rotates by durable compare-and-replace. It is not in
the identity vault, is not distributed to devices, and is not recovered from
the 24-word phrase. A Files object created under an unavailable epoch cannot
be decrypted, even when Identity recovery succeeds.

The target keeps Identity as the sole authority for authorized devices and
uses the canonical Storage layer for opaque encrypted envelope objects. Do not
put SMKs, device wraps, recovery wraps, or Files contents in consensus state.
Consensus may commit a bounded envelope-root identifier and sequence needed
to validate updates and discover the current finalized envelope set. Exact
operation and state fields require review alongside the Files root update.

## Key and envelope model

Each epoch uses a fresh random 256-bit SMK. The local key ring remains a
password-protected cache; the portable distribution object contains one
independently authenticated envelope per epoch and destination:

```text
StorageKeyEnvelopeSet {
    account_id
    network_id
    generation
    previous_envelope_root
    entries[] {
        epoch
        destination_kind       // authorized device or recovery
        destination_id         // activation-bound device ID, or recovery ID
        profile_id
        nonce_or_encapsulation
        ciphertext
        commitment
    }
}
```

This is a conceptual schema only. The finalized profile determines exact
fields and byte encodings. The set must be bounded, canonical, reject duplicate
or missing epoch/destination entries, and cover every retained epoch through
the current epoch. Each entry wraps one SMK; one failed entry cannot partially
activate a set. Secret keys never appear in the set in plaintext.

For device entries, wrap each SMK independently to an active device's
Identity-published KEM package from `89_IDENTITY_KEM_PUBLICATION.md`. Bind the
wrap to NetworkID, AccountID, epoch, envelope generation, device key ID,
activation, package ID, and the `CYBOU/STORAGE/KEYWRAP` purpose. Use the
finalized standard construction and AEAD; never use signing keys, a local
hybrid combiner, or a service-specific encryption primitive.

For recovery entries, derive a dedicated recovery wrapping key from the
recovery entropy using a separately domain-separated, reviewed KDF. It must
not reuse either Recovery Root signing key as encryption material. Bind the
resulting AEAD to NetworkID, AccountID, epoch, envelope generation, and the
recovery purpose. The recovery key is re-derived from the 24-word phrase on a
clean machine and is never published. Freeze the KDF, AEAD, encoding, and
cross-implementation vectors before implementation.

Each ciphertext and the complete canonical set receive commitments under
dedicated domains. The finalized account-level envelope-root transition binds
NetworkID, AccountID, monotonic generation, previous root, new root, current
epoch, and the authorized update. Clients accept only the latest finalized
root and verify the retrieved set and every entry against it. The exact
operation kind, authorizer, and state representation remain open; this is one
account-level update per key-set change, not a chain operation for each file.

## Publication and restore lifecycle

1. **Initial Files key set:** create the first random SMK, build its device and
   recovery envelopes, durably upload the encrypted set, submit the root
   commitment, and wait for finality before enabling Files writes. Preserve the
   prior local key ring until the finalized set is verified and a new local
   sidecar has been saved and reopened.
2. **Add or restore a device:** verify the finalized Identity record and
   DeviceAdd activation; retrieve the finalized envelope set; unwrap every
   retained epoch addressed to the recovery capability; save and reopen the
   reconstructed AccountID-bound sidecar; then publish device envelopes for
   all retained epochs and wait for the new envelope-root update to finalize
   before enabling Files on that device.
3. **Add an ordinary device:** require its finalized activation and KEM
   package; wrap every retained epoch to that activation; publish the updated
   envelope set and commitment; enable Files only after verified finality.
4. **Rotate an SMK:** generate a new random epoch key; retain earlier epochs
   for existing objects; wrap the full retained set to every currently
   authorized device and the recovery capability; durably upload and commit
   the complete replacement set before writing objects under the new epoch.
5. **Revoke a device:** after finalized revocation, omit it from future
   envelope sets, generate a fresh epoch, wrap that epoch only to current
   devices and recovery, and commit before new writes. Historical envelopes
   remain available for evidence and old object recovery. A revoked device may
   have copied prior keys; revocation cannot erase that knowledge.
6. **Rotate the Recovery Root:** derive the new, domain-separated recovery
   wrapping key from the candidate phrase. First publish and finalize an
   overlap envelope set addressed to both the current and candidate recovery
   capabilities. Then submit RecoveryRotate using the existing finality-gated
   vault journal. Once RecoveryRotate is finalized, the candidate phrase can
   recover the retained epochs; promote its vault only after verifying that
   finalized root and the overlap envelope commitment. A later envelope-root
   update may remove the old recovery destination from the current set, while
   preserving its historical envelope bytes. If either operation has uncertain
   delivery, retain both vaults and retry only the journaled bytes.

When any epoch or recipient changes, publish the complete replacement set
atomically by commitment. Missing epochs, duplicate destinations, stale
generations, wrong activation IDs, stale previous roots, failed unwraps, or
unverified finality fail closed. Exact operation bytes must use the shared
reconciliation/journal model; admission acknowledgment alone never advances
the active local ring.

## Availability and security limits

Envelope objects are small Storage objects, but their availability is a hard
dependency of clean-machine restore. The current bounded three-peer placement
is only a client component over already connected providers; it is not a
durability guarantee. Before Files recovery can be advertised, the Storage
layer must provide an explicit retention, replication, repair, and retrieval
contract for the latest envelope set and required historical generations.
Clients must surface an unavailable envelope as an incomplete recovery, never
as an empty key ring or a successful restore.

Providers see only opaque identifiers, ciphertext sizes, and commitments.
They do not learn filenames, plaintext keys, recovery entropy, or file
contents. The envelope index itself must use the existing bounded,
ciphertext-only Storage boundary. Public lookup privacy, traffic analysis,
retention and loss recovery remain part of Storage review.

Rotation blocks future access by an excluded device to newly created SMK
epochs. It cannot revoke keys or plaintext that a device already obtained.
Re-encrypting old objects under a new epoch is an explicit user or policy
operation with separate bandwidth, durability, and crash-recovery behavior;
do not claim that ordinary rotation retroactively protects old objects.

## Implementation gates

Before implementation, review together:

- finalized device KEM package and standard wrapping profile (`89`);
- domain-separated recovery KDF and standard AEAD with published vectors;
- envelope-set canonical encoding, bounds, commitments, and atomic generations;
- one account-level finalized root update and replay/conflict rules;
- vault/sidecar promotion and journal behavior across crashes;
- complete retained-epoch restore, add-device, revoke, and root-rotation flows;
- Storage availability and privacy guarantees for envelope objects;
- UI states for recovery pending, key unavailable, and Files-ready;
- DEV test/cutover plan after PQ consensus and names integrate.

Until these gates pass, the local Storage Key Ring remains single-installation
custody, Files is not cross-device recoverable, and the client must say so.

## Related authority

- `76_IDENTITY_VAULT_RECOVERY.md` — recovery phrase and local vault.
- `86_IDENTITY_SECURITY_SUBSTRATE.md` — Identity key ownership.
- `88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md` — Files key hierarchy and object model.
- `89_IDENTITY_KEM_PUBLICATION.md` — authorized device KEM packages.
- `11_STORAGE_OBJECTS.md` — provider storage, placement, and durability.
- `87_DEVICE_OPERATION_COORDINATOR.md` — durable operation submission and reconciliation.
