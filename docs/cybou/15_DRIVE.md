# 15 — Drive service

Implementation status: Drive is a future service. No operational Drive
client or distributed object-storage backend exists in the DEV runtime.

CYBOU Drive is a user-facing filesystem view over encrypted manifests and object storage.

Drive comes after Backup.

## Logical model

```text
directory manifest
    -> encrypted object references
file
    -> encrypted chunks/shards
versions
    -> encrypted history
```

The storage network does not contain a plaintext filesystem tree.

## Desktop goal

Expose a convenient mounted/virtual drive on supported platforms while retaining the same underlying object model used by Backup.

## Requirements

- atomic/transactional manifest update semantics;
- crash-safe local cache;
- conflict handling for multiple devices;
- partial/range retrieval where useful;
- local cache is not the sole copy;
- offline edits have deterministic reconciliation rules.

Do not implement Drive before object durability, repair and Backup restore are proven.
