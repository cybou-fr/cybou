# 90 — Files key recovery

Status: architecture target; not implemented. The local encrypted Storage Key
Ring is durable on one installation only. This document defines the account
key recovery path needed before Files can claim clean-machine recovery. It
does not introduce machines as protocol identities.

## Current behavior and boundary

The local `StorageKeyRing` is a password-protected CYBV2 sidecar bound to
AccountID. It holds contiguous random 256-bit Storage Master Key (SMK) epochs,
retains old epochs, and rotates by durable compare-and-replace. It is not in the
Identity vault and is not recovered from the 24-word phrase. Recovering the
Identity alone therefore does not yet restore Files access.

Identity publishes one current account KEM capability per `key_epoch`. Every
installation restored from the same phrase derives the same Identity roles;
there is no protocol-level installation enrollment, per-installation key
wrap, or per-installation revocation. Do not put SMKs, recovery wraps, or Files
contents in consensus state. Consensus may commit a bounded envelope-root
identifier and sequence needed to validate updates and locate the current
finalized envelope set.

## Key and envelope model

Each epoch uses a fresh random 256-bit SMK. A bounded canonical envelope set
contains one account KEM envelope and one recovery envelope for every retained
SMK epoch:

```text
StorageKeyEnvelopeSet {
    account_id
    network_id
    generation
    previous_envelope_root
    entries[] {
        epoch
        destination_kind       // account KEM or recovery
        destination_id         // key_epoch or recovery-key commitment
        profile_id
        nonce_or_encapsulation
        ciphertext
        commitment
    }
}
```

The finalized profile determines exact fields and byte encodings. The set must
reject duplicate or missing epoch/destination entries and cover every retained
epoch. Each entry wraps one SMK; one failed entry cannot partially activate a
set. Secret keys never appear in the set in plaintext.

For account KEM entries, wrap each SMK to the current Identity-published KEM
package in `89_IDENTITY_KEM_PUBLICATION.md`. Bind the wrap to NetworkID,
AccountID, epoch, envelope generation, Identity key_epoch, package commitment,
and the `CYBOU/STORAGE/KEYWRAP` purpose. Use a reviewed standard construction
and AEAD; never use signing keys or a custom key combiner.

For recovery entries, derive a dedicated wrapping key from recovery entropy
using a separately domain-separated, reviewed KDF. It must not reuse either
Recovery signing key as encryption material. Bind the AEAD to NetworkID,
AccountID, epoch, envelope generation, and recovery purpose. Freeze the KDF,
AEAD, encoding, and cross-implementation vectors before implementation.

Each ciphertext and complete canonical set receive commitments under
dedicated domains. The finalized account-level envelope-root transition binds
NetworkID, AccountID, monotonic generation, previous root, new root, current
SMK epoch, and authorized update. Clients accept only the latest finalized
root and verify the retrieved set and every entry against it. This is one
account-level update, not an operation for each file or installation.

## Publication and restore lifecycle

1. **Initial Files key set:** create the first random SMK, build account KEM
   and recovery envelopes, durably upload the encrypted set, submit the root
   commitment, and wait for finality before enabling Files writes. Preserve the
   prior local key ring until the finalized set is verified and a new local
   sidecar has been saved and reopened.
2. **Clean-machine restore:** derive Identity keys from the 24-word phrase,
   locate AccountID by RecoveryKeyID, verify the current finalized Identity
   key set and KEM commitment, retrieve the finalized envelope set, unwrap
   every retained epoch through the recovery envelope, save and reopen the
   AccountID-bound sidecar, then enable Files. Identity restore itself has no
   consensus mutation.
3. **Rotate an SMK:** generate a new random epoch key; retain earlier epochs
   for existing objects; wrap the full retained set to the current account
   KEM capability and recovery capability; durably upload and commit the
   complete replacement set before writing objects under the new epoch.
4. **Rotate Identity keys:** use `IdentityRotate` to atomically replace
   Recovery, Authorization, and KEM roles and advance `key_epoch`. Rebuild
   account KEM envelopes for the current retained SMKs and commit the new
   envelope root before enabling writes under the new capability. Keep local
   vault promotion gated on verified IdentityRotate finality.

When an epoch or Identity key set changes, publish the complete replacement
set atomically by commitment. Missing epochs, duplicate destinations, stale
generations, stale previous roots, failed unwraps, or unverified finality fail
closed. Exact operation bytes use the shared reconciliation/journal model;
admission acknowledgment alone never advances the active local ring.

## Availability and security limits

Envelope objects are small Storage objects, but their availability is a hard
dependency of clean-machine restore. Current bounded placement is only a client
component over connected providers; it is not a durability guarantee. Before
Files recovery can be advertised, Storage needs explicit retention,
replication, repair, and retrieval guarantees for the latest envelope set and
required historical generations. Clients must surface unavailable envelopes
as incomplete recovery, never as an empty key ring or successful restore.

Providers see only opaque identifiers, ciphertext sizes, and commitments. They
do not learn filenames, plaintext keys, recovery entropy, or file contents.
The envelope index itself must use the bounded ciphertext-only Storage
boundary. Lookup privacy, traffic analysis, retention, and loss recovery remain
part of Storage review.

A phrase-derived Identity is account-wide. Anyone who obtains the phrase can
restore that Identity, and a previously obtained SMK or plaintext cannot be
revoked. Identity rotation changes future account capabilities but does not
retroactively protect old object keys already copied. Re-encrypting old objects
under a new epoch is a separate operation with bandwidth, durability, and
crash-recovery behavior; do not claim that ordinary rotation retroactively
protects old objects.

## Implementation gates

Before implementation, review together:

- finalized account KEM package and standard wrapping profile (`89`);
- domain-separated recovery KDF and standard AEAD with published vectors;
- envelope-set canonical encoding, bounds, commitments, and atomic generations;
- one account-level finalized root update and replay/conflict rules;
- vault/sidecar promotion and journal behavior across crashes;
- complete retained-epoch clean-machine restore and IdentityRotate flows;
- Storage availability and privacy guarantees for envelope objects;
- UI states for recovery pending, key unavailable, and Files-ready;
- DEV test/cutover plan after PQ consensus and names integrate.

Until these gates pass, the local Storage Key Ring remains installation-local,
Files is not clean-machine recoverable, and the client must say so.

## Related authority

- `76_IDENTITY_VAULT_RECOVERY.md` — recovery phrase and local vault.
- `86_IDENTITY_SECURITY_SUBSTRATE.md` — Identity key ownership.
- `88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md` — Files key hierarchy and object model.
- `89_IDENTITY_KEM_PUBLICATION.md` — account KEM capability publication.
- `11_STORAGE_OBJECTS.md` — provider storage, placement, and durability.
- `87_IDENTITY_OPERATION_COORDINATOR.md` — durable operation submission and reconciliation.
