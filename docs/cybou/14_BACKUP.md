# 14 — Backup service

Backup is a later application of the general durable CYBOU Object Storage layer.

CYBOU Email ships first.

Backup begins only after:

```text
native Email works
+
Object Storage works
+
erasure/repair works
+
storage accounting is sufficiently stable
```

## Goal

A user selects normal desktop folders and CYBOU protects encrypted versions over distributed storage.

## Pipeline

```text
scan
-> detect changes
-> compress when useful
-> chunk
-> pad
-> encrypt locally
-> erasure encode
-> place opaque shards
-> verify placement
-> commit encrypted manifest
```

## Requirements

- no plaintext to storage peers;
- encrypted paths/names;
- explicit manifest versioning;
- safe resume;
- local delete does not silently erase remote history;
- restore original/alternate path;
- verify integrity before plaintext return.

## Manual disaster test

1. back up a real test folder;
2. verify remote placement;
3. remove local test copy;
4. restart `cybou.exe`;
5. restore;
6. compare hashes/content.

All-keys-lost recovery is not claimed solved yet.
