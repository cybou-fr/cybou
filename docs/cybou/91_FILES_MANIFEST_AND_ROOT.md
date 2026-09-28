# 91 — Encrypted Files manifest and root updates

Status: target protocol and client architecture; not implemented. This document
defines how private file-manager metadata is synchronized without making the
chain or Storage providers a plaintext directory. It depends on the key
distribution and recovery gates in `89_IDENTITY_KEM_PUBLICATION.md` and
`90_STORAGE_KEY_RECOVERY.md`.

## Ownership and privacy

Identity owns authorization. Storage carries opaque encrypted objects. Files
owns the encrypted catalog that maps user-visible entries to those objects.
The desktop decrypts the catalog and shows original filenames and folders to
the account owner. Providers, validators, and public consensus state never
receive plaintext filenames, MIME types, paths, or file actions.

The chain stores only the latest account-level manifest sequence, opaque
manifest object locator, and commitment required to validate an update and
retrieve the finalized catalog. It does not store an entry per file or folder.
Rename, move, star, trash, and restore update the encrypted catalog and publish
one new root for the resulting batch.

## Encrypted catalog

The canonical catalog is a bounded, canonical tree. Its conceptual entries
are:

```text
FilesManifest {
    account_id
    network_id
    sequence
    previous_manifest_root
    entries[] {
        file_id
        parent_folder_id
        entry_kind
        display_name
        mime_type
        logical_size
        modified_time
        starred
        trash_state
        object_id
        object_commitment
        key_epoch
    }
}
```

The exact encoding, entry limits, name validation, timestamps, and conflict
rules require freeze review. `file_id` is a random stable local/product
identifier independent from `ObjectID`; paths are derived from parent links,
not stored as provider-visible names. Mail ownership and Files ownership are
separate references even when they safely reuse one encrypted object.

Encrypt the complete canonical manifest locally under a dedicated Files
manifest key derived with a standard, domain-separated HKDF from the current
Storage Master Key epoch. Use the selected standard AEAD with associated data
binding NetworkID, AccountID, manifest sequence, epoch, and format/profile
identifier. Public commitment construction, KDF labels, nonces, bounds, and
vectors must be frozen before implementation. No custom cipher or key
combiner is allowed. Only encrypted manifest bytes and opaque object IDs are
uploaded.

## Root update operation

The Identity-authorized operation named `FILES_ROOT_UPDATE` is a target name,
not a frozen API or wire value. Its reviewed semantic payload is:

```text
FilesRootUpdate {
    account_id
    previous_sequence
    previous_root
    new_sequence
    encrypted_manifest_object_id
    encrypted_manifest_commitment
    key_epoch
}
```

Require `new_sequence == previous_sequence + 1`, an exact match with finalized
state's current root, a non-null object ID and commitment, and a currently
current Identity key_epoch. The existing `IdentityOperationCoordinator`
allocates the nonce, signs, journals exact operation bytes, and reconciles
uncertain delivery. State updates atomically to the new sequence, root,
manifest locator, and key epoch. The operation's authorization binds the full
payload. Consensus never parses the encrypted entry list.

Only finalized roots become current. An admission acknowledgment does not
publish a Files change. Historical root updates and encrypted manifest objects
remain available for audit, evidence, and concurrent-update recovery subject
to Storage retention rules.

## Update lifecycle and crash handling

For an upload or catalog edit:

1. Read the latest verified finalized root and decrypt its catalog locally.
2. Create the new catalog and encrypt it under the current key epoch.
3. Persist a Files update journal containing the old root, candidate sequence,
   encrypted object locator/commitment, key epoch, and exact operation bytes
   once built. The journal is local and encrypted.
4. Upload the encrypted catalog through `StorageService` and placement; verify
   the required durability threshold before submitting the root update.
5. Submit through `IdentityOperationCoordinator` and wait for BFT finality.
6. On finality, atomically promote the local current catalog/root and clear the
   journal. On known rejection, keep the previous catalog current and safely
   release the candidate object under Storage retention rules. On uncertain
   delivery, retry the exact operation bytes and reconcile by OperationID.

If the process restarts, it must reconcile the root operation before creating
another Files update. An uploaded catalog whose root update did not finalize is
an unreferenced encrypted object eligible for later Storage cleanup; it must
not appear as a finalized Files item. If the root finalized but local
promotion was interrupted, reconstruct the committed view from verified state
and the committed encrypted catalog.

When two clients edit the same root concurrently, at most one update can match
the finalized previous root. The client whose update loses fetches the winning
finalized catalog, merges non-conflicting changes locally, presents conflicting
rename/move decisions, and submits a new sequence. It never replaces a newer
root based on local time or silently drops edits.

## Product behavior

The Files page shows decrypted names, folders, logical size, modified time,
star state, and honest protection status. It does not show object IDs in the
normal list. Search is local over decrypted metadata. Recent may remain a
local activity view; recent activity is local unless an explicitly designed encrypted account
catalog later includes it. Trash and deletion wording must follow Storage retention
semantics and must not promise immediate provider erasure.

Upload reaches `Protected` only after both the configured Storage durability
threshold and the Files root update reach verified finality. Download verifies
the encrypted object and catalog commitments before releasing plaintext.
When a file's epoch is unavailable, show an explicit key recovery state; never
hide it as a missing file or create a replacement empty catalog.

## Cutover and implementation gates

Before code changes to consensus state, freeze and review:

- canonical tree encoding, bounds, IDs, and merge/conflict semantics;
- manifest key derivation, AEAD context, commitments, and vectors;
- operation serializer, validation, state snapshot, and state-root changes;
- Storage durability threshold and unreferenced-object cleanup;
- exact journal/reconciliation behavior across upload, submission, and crash;
- concurrent-update conflict, clean-machine restore, IdentityRotate, and historical-root behavior;
- Qt Files list/actions, progress, protection, and key-unavailable states;
- coordinated DEV cutover after PQ consensus and names integrate.

There is no legacy decoder, automatic import, or dual operation path. The
desktop currently supports a provisional single-installation encrypted local
index and Storage transfers. Until these gates pass, this path must not imply a
finalized persistent Files catalog, account-level synchronization, or Beta durability.

## Related authority

- `83_STORAGE_UI_UX.md` — canonical Files interactions and presentation.
- `88_ENCRYPTED_OBJECT_AND_KEY_MODEL.md` — object and content-key privacy.
- `89_IDENTITY_KEM_PUBLICATION.md` — account KEM capability binding.
- `90_STORAGE_KEY_RECOVERY.md` — key distribution, recovery, and epochs.
- `87_IDENTITY_OPERATION_COORDINATOR.md` — shared Identity nonce and operation journal.
- `81_BETA_PRODUCT_SCOPE.md` — Beta durability and user-experience gates.
