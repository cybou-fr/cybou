# RootPublication operation

Status: active DEV protocol, wire profile 3. RootPublication is the only
application-content publication operation; Mail and Files payload schemas are
private encrypted content.

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
    recipient_capsules: bounded list
}
```

The body is a core-deterministic CBOR map with integer keys in ascending order:

```text
0: wire profile version (3)
1: root_chunk_id (32-byte string)
2: chunk_authorization_root (32-byte string)
3: chunk_count (unsigned integer)
4: recipient_capsules (array)
```

Each capsule is `[KEM_profile, recipient_key_epoch, encapsulation,
wrapped_content_key]`. Profile `0x647a` is X-Wing draft-05 for DEV. Fields are
respectively a 16-bit profile value, unsigned epoch, 1,120-byte X-Wing
ciphertext, and 60 bytes containing a 12-byte nonce followed by encrypted
32-byte content key and 16-byte tag. The capsule has no public recipient
AccountID or KEM-package commitment: the latter is publicly linked to
AccountID in Identity state and would reveal the recipient. The outer Identity
authorization supplies the sender AccountID, nonce, key epoch, and required
Ed25519 + ML-DSA-44 signature.

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

Profile 3 limits the CBOR body to 128 KiB, the full operation including
Identity authorization to 144 KiB, capsules to 1–32, chunk count to
1–4,294,967,295. The body does not
carry the chunk-ID set, so it does not publish a directory-like list of every
opaque chunk. Capsule count and recipient key epochs are public; this profile
accepts those metadata leaks and does not pad the capsule list.

`chunk_authorization_root` is BLAKE3-256 over a Merkle tree. Each leaf commits
only to the full ChunkID; ChunkID already commits to all stored encrypted
bytes. Leaves are ordered by the publisher's durable staging order: DATA
chunks in source order, generated INDEX chunks when staged, and ROOT last. A local provider index records the
leaf index at staging time. Each leaf hashes
`CYBOU/CHUNK-AUTH/LEAF || ChunkID`; internal pairs hash
`CYBOU/CHUNK-AUTH/NODE || left || right`. An unpaired final child is duplicated.
Empty trees and duplicate ChunkIDs are invalid. An inclusion proof has at most
32 siblings, ordered leaf to root. The proof is only the leaf index and sibling
hashes; PUT_CHUNK supplies ChunkID and finalized RootPublication supplies
chunk_count. The proof binds that ChunkID to the committed tree. Providers enforce local
physical capacity; publication-level byte declarations are not part of the
protocol. The publisher must include proofs for every staged chunk, including
`root_chunk_id`; the client can validate these locally before submitting the publication.

The wire-profile vector stages the IDs `11`, `42`, and `93`, each repeated 32
bytes. Its expected Merkle root is
`957fe7e324d7a8e3c69d84d20f1294ee6a406c8e3dc92b151607708cd6395c4e`.

There are no `MAIL`, `FILE`, `BACKUP`, or `FILES_ROOT_UPDATE` operation kinds.
Mail and Files schemas are discovered only after a recipient decrypts a root.
Consensus has generic publication limits and deterministic fees; it cannot
enforce a Mail/day quota while Mail type is hidden. The fee is computed from
immutable genesis-bound protocol parameters:
`root_publication_fee_per_started_kib` times the number of started KiB in the
full canonical operation, plus `root_publication_fee_per_chunk` times the
authorized chunk count. DEV currently sets both rates to four CYBOU; Beta and
Mainnet freeze their own values in their network definitions. Each four-unit
fee splits as three Security and one Onboarding. Priority fees are disabled.
The fee is debited from the sender's System Balance and enters the ordinary
pending fee pool. Authorization nonce advancement and fee accounting are part
of the deterministic operation transition.

## Authorization and replay

Identity authorization binds the canonical RootPublication body, including
the chunk authorization root and capsules.
Existing account nonce and identity key_epoch validation remain deterministic.
The operation ID uses the repository's consensus hash over NetworkID and full
canonical operation bytes. Chunk count is checked by each inclusion proof.

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
