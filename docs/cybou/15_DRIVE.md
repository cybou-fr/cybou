# 15 — Drive concept (superseded by Files)

Status: historical product terminology. The CYBOU product surface is **Files**
and is part of Beta; see `81_BETA_PRODUCT_SCOPE.md` and
`83_STORAGE_UI_UX.md`. This document remains as background for filesystem
semantics only. It does not define a separate post-Beta Drive milestone.

CYBOU Drive is a user-facing filesystem view over encrypted manifests and object storage.

The current application order is Storage Core, Beta Files, then post-Beta
Backup. A mounted/virtual drive, if later justified, remains a Files client
surface over the same object model.

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
- conflict handling for concurrent catalog updates;
- partial/range retrieval where useful;
- local cache is not the sole copy;
- offline edits have deterministic reconciliation rules.

Do not implement a mounted/virtual drive before object durability and repair
are proven. The Files product itself is a Beta requirement.
