# RootPublication operation

Status: frozen architecture target; canonical encoding, fee schedule, and
limits are not yet frozen. RootPublication replaces Mail-specific network
delivery and Files-specific consensus roots after the coordinated protocol
cutover.

## Public operation boundary

The operation is authorized through the existing Identity operation
coordinator and contains only generic fields:

```text
RootPublication {
    root_chunk_id: ChunkID
    chunk_authorization_root: Hash
    chunk_count: bounded integer
    authorized_stored_bytes: bounded integer
    recipient_capsules: bounded list
}
```

Each capsule contains a reviewed KEM ciphertext and wrapped root key; it has no
public recipient AccountID. The capsule encoding binds NetworkID,
RootPublication context, root ChunkID, sender authorization, and recipient
key_epoch without exposing the recipient. Failure to decrypt means
`NOT_FOR_ME`; the client records that result locally.

Capsule count and approximate capsule sizes are still public in the operation.
Decide whether to pad the capsule list to size buckets or accept recipient-count
leakage before freezing the wire profile. Do not describe recipient identities
as hidden until that metadata leakage is evaluated.

There are no `MAIL`, `FILE`, `BACKUP`, or `FILES_ROOT_UPDATE` operation kinds.
Mail and Files schemas are discovered only after a recipient decrypts a root.
Consensus has generic publication limits and deterministic byte-based fees;
it cannot enforce a Mail/day quota while Mail type is hidden.

## Authorization and replay

Identity authorization binds the full canonical operation bytes, including
the chunk authorization root, all capsules, and accounting fields. Existing
account nonce and identity key_epoch validation remain deterministic. Exact
operation ID, replay, size, capsule-count, byte-accounting, and fee rules must
be frozen before implementation reaches DEV cutover.

## Publication and availability semantics

Finality authorizes the listed chunks for storage; it does not itself prove
that providers possess them. After finality, the client submits chunks with
admission proofs and waits for the frozen durability threshold before
reporting a publication as available/sent. The client retains its encrypted
copy and retries if storage is incomplete. Recipient clients tolerate a
finalized publication whose chunks are temporarily unavailable.

This is a deliberate refinement of the prior Beta send gate: consensus
publication precedes distributed storage admission. Beta readiness still
requires a tested durability threshold, retry, repair, and a clear UX state for
finalized-but-unavailable roots.

\n