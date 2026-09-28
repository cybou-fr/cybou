# RootPublication operation

Status: frozen architecture target, wire profile 2. RootPublication replaces
Mail-specific network delivery and Files-specific consensus roots after the
coordinated protocol cutover.

## Public operation boundary

`AuthorizedRootPublication` is a first-class typed `ProtocolOperation`. Its
outer Identity authorization binds the canonical body, account nonce, key
epoch, and operation kind; it is submitted through the existing Identity
operation coordinator. Finalized blocks commit the operation bytes, but the
publication itself creates no persistent per-root consensus-state object.

The body contains only generic fields:

```text
RootPublication {
    root_chunk_id: ChunkID
    chunk_authorization_root: Hash
    chunk_count: bounded integer
    authorized_stored_bytes: per-publication provider ceiling
    recipient_capsules: bounded list
}
```

The body is a core-deterministic CBOR map with integer keys in ascending order:

```text
0: wire profile version (2)
1: root_chunk_id (32-byte string)
2: chunk_authorization_root (32-byte string)
3: chunk_count (unsigned integer)
4: authorized_stored_bytes (unsigned integer)
5: recipient_capsules (array)
```

Each capsule is `[KEM_profile, recipient_key_epoch, encapsulation,
wrapped_content_key]`. Profile `0x647a` is X-Wing draft-05 for DEV. Fields are
respectively a 16-bit profile value, unsigned epoch, 1,120-byte X-Wing
ciphertext, and 60 bytes containing a 12-byte nonce followed by encrypted
32-byte content key and 16-byte tag. The capsule has no public recipient
AccountID or KEM-package commitment: the latter is publicly linked to
AccountID in Identity state and would reveal the recipient. The outer Identity
authorization supplies the sender AccountID, nonce, key epoch, and required
Ed25519 + ML-DSA-65 signature.

For capsule wrapping, HKDF-SHA256 uses the X-Wing shared secret as input key
material, NetworkID as salt, and `CYBOU/ROOT-CAPSULE-KEY/v1 || root_chunk_id ||
sender_account_id || sender_nonce_u64be || sender_key_epoch_u64be ||
recipient_key_epoch_u64be` as info. ChaCha20-Poly1305
uses the resulting 32-byte key and the stored random nonce. Its AAD is
`CYBOU/ROOT-CAPSULE-AAD/v1 || NetworkID || root_chunk_id || sender_account_id
|| sender_nonce_u64be || sender_key_epoch_u64be || recipient_key_epoch_u64be`.
Thus a capsule is bound to the network, root, sender authorization context,
and recipient KEM epoch without identifying the recipient publicly. Failed
unwrap means `NOT_FOR_ME`; the client discards the result instead of retaining
a negative outcome record.

Profile 2 limits the CBOR body to 128 KiB, the full operation including
Identity authorization to 144 KiB, capsules to 1–32, chunk count to
1–4,294,967,295, and the declared provider ceiling to 1 PiB. The body does not
carry the chunk-ID set, so it does not publish a directory-like list of every
opaque chunk. Capsule count and recipient key epochs are public; this profile
accepts those metadata leaks and does not pad the capsule list.

`chunk_authorization_root` is BLAKE3-256 over a Merkle tree. Leaves are ordered
by the publisher's durable staging order: DATA chunks in source order, generated
INDEX chunks when staged, and ROOT last. A local provider index records the
leaf index at staging time. Each leaf hashes
`CYBOU/CHUNK-AUTH/LEAF || ChunkID || stored_size_u64be`; internal pairs hash
`CYBOU/CHUNK-AUTH/NODE || left || right`. An unpaired final child is duplicated.
Empty trees and duplicate ChunkIDs are invalid. An inclusion proof has at most
32 siblings, ordered leaf to root, and carries the leaf index and chunk count.
The proof binds one ChunkID and its stored size to the committed tree. A
provider checks the count against the publication, checks that the individual
chunk fits the declared ceiling, and enforces the ceiling against bytes it
accepts for that publication. This proof does not establish the total size of
all leaves across all providers: `authorized_stored_bytes` is a declared
per-publication ceiling, not a consensus-verified leaf sum. The publisher
must include proofs for every staged chunk, including `root_chunk_id`; the
client can validate these locally before submitting the publication.

The wire-profile vector stages ID `11` repeated 32 bytes with size 4,096,
then ID `42` repeated 32 bytes with size 16,384, then ID `93` repeated 32
bytes with size 1,100. Its expected Merkle root is
`2117d08ee55b42e9b8aa63af00a2ec57cd862bf9cb7b6b08ab6c7cceb6941e19`.

There are no `MAIL`, `FILE`, `BACKUP`, or `FILES_ROOT_UPDATE` operation kinds.
Mail and Files schemas are discovered only after a recipient decrypts a root.
Consensus has generic publication limits and deterministic byte-based fees;
it cannot enforce a Mail/day quota while Mail type is hidden. The fee is four
CYBOU per started KiB of full canonical operation bytes, with each four-unit
fee split as three Security and one Onboarding. Priority fees are disabled.
The fee is debited from the sender's System Balance and enters the ordinary
pending fee pool. Authorization nonce advancement and fee accounting are part
of the deterministic operation transition.

## Authorization and replay

Identity authorization binds the canonical RootPublication body, including
the chunk authorization root, capsules, and declared provider ceiling.
Existing account nonce and identity key_epoch validation remain deterministic.
The operation ID uses the repository's consensus hash over NetworkID and full
canonical operation bytes. Chunk count is checked by each inclusion proof;
the byte ceiling is an authenticated sender declaration enforced by providers.

## Publication and availability semantics

Finality authorizes chunks that prove inclusion for storage; it does not prove
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
