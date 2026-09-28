# RootPublication operation

Status: frozen architecture target, wire profile 1. RootPublication replaces
Mail-specific network delivery and Files-specific consensus roots after the
coordinated protocol cutover.

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

The body is a core-deterministic CBOR map with integer keys in ascending order:

```text
0: wire profile version (1)
1: root_chunk_id (32-byte string)
2: chunk_authorization_root (32-byte string)
3: chunk_count (unsigned integer)
4: authorized_stored_bytes (unsigned integer)
5: recipient_capsules (array)
```

Each capsule is `[KEM_profile, recipient_key_epoch, package_commitment,
encapsulation, wrapped_content_key]`. Profile `0x647a` is X-Wing draft-05 for
DEV. Fields are respectively a 16-bit profile value, unsigned epoch, 32-byte
commitment, 1,120-byte X-Wing ciphertext, and 60 bytes containing a 12-byte
nonce followed by encrypted 32-byte graph key and 16-byte tag. The capsule has
no public recipient AccountID. The outer Identity authorization supplies the
sender AccountID, nonce, key epoch, and required Ed25519 + ML-DSA-65 signature.

For capsule wrapping, HKDF-SHA256 uses the X-Wing shared secret as input key
material, NetworkID as salt, and `CYBOU/ROOT-CAPSULE-KEY/v1 || root_chunk_id ||
sender_account_id || sender_nonce_u64be || sender_key_epoch_u64be ||
recipient_key_epoch_u64be || package_commitment` as info. ChaCha20-Poly1305
uses the resulting 32-byte key and the stored random nonce. Its AAD is
`CYBOU/ROOT-CAPSULE-AAD/v1 || NetworkID || root_chunk_id || sender_account_id
|| sender_nonce_u64be || sender_key_epoch_u64be || recipient_key_epoch_u64be
|| package_commitment`. Thus a capsule is bound to the network, root, sender
authorization context, and recipient KEM epoch without identifying the
recipient publicly. Failed unwrap means `NOT_FOR_ME`; the client records that
result locally.

Profile 1 limits the CBOR body to 128 KiB, the full operation including
Identity authorization to 144 KiB, capsules to 1–64, chunks to 1–2,048, and
authorized stored bytes to 512 MiB. Each stored chunk must be at least 1,089
bytes. Capsule count and recipient key epochs are public; this profile accepts
that leakage and does not pad the capsule list.

There are no `MAIL`, `FILE`, `BACKUP`, or `FILES_ROOT_UPDATE` operation kinds.
Mail and Files schemas are discovered only after a recipient decrypts a root.
Consensus has generic publication limits and deterministic byte-based fees;
it cannot enforce a Mail/day quota while Mail type is hidden. The fee is four
CYBOU per started KiB of full canonical operation bytes, with each four-unit
fee split as three Security and one Onboarding. Priority fees are disabled.

## Authorization and replay

Identity authorization binds the canonical RootPublication body, including
the chunk authorization root, all capsules, and accounting fields. Existing
account nonce and identity key_epoch validation remain deterministic. The
operation ID uses the repository's consensus hash over NetworkID and full
canonical operation bytes. Chunk count and authorized stored bytes must equal
the authenticated inclusion set; they are never caller-selected estimates.

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
