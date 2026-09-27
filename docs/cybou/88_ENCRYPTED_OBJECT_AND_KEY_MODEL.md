# 88 — Encrypted object and key model

Status: canonical target for Files objects and Mail attachments. This is a
security architecture contract, not a claim that KEM publication, Storage, or
attachment delivery is implemented. `11_STORAGE_OBJECTS.md` owns provider
placement, leases, audits, repair, and accounting.

## Security boundary

Bulk content is not encrypted with a public-key algorithm. Hybrid PQ key
agreement establishes or wraps content keys; a standard symmetric AEAD
protects object and message bytes.

```text
hybrid KEM → establish/wrap content key
symmetric AEAD → encrypt bulk bytes
hybrid signature → authorize operations and manifest/root changes
```

Do not invent a KEM combiner, AEAD, or key-derivation primitive. The
interoperable X25519 + ML-KEM-768 profile and exact AEAD parameters remain
subject to `49_EMAIL_E2EE_HPKE_PQ.md`, implementation review, and test vectors.

## Identity key hierarchy

```text
CYBOU Identity
  ├── Recovery Root: Ed25519 + ML-DSA-65
  ├── Device signing: Ed25519 + ML-DSA-44
  └── Separate device key agreement: X25519 + ML-KEM-768 (target)
```

Signing keys MUST NOT be converted or reused as Mail, Files, or Storage
encryption keys. Authorized device KEM public keys need identity binding,
activation, rotation, historical authorization evidence, and downgrade
protection before the registry publishes them. Private KEM material remains on
authorized clients.

## Files key hierarchy

The target Files model uses a random Storage Master Key per key epoch. It is
wrapped independently to each authorized device using the reviewed hybrid
key-agreement profile. Revocation and recovery rotate future key access; they
cannot erase content a previously authorized device already decrypted.

```text
Identity
  └── Storage Master Key, epoch N
        ├── hybrid-wrapped to authorized device A
        ├── hybrid-wrapped to authorized device B
        └── hybrid-wrapped to authorized device C
```

An object key is derived for one opaque object using a standard HKDF with a
random per-object salt and a distinct context:

```text
ObjectKey = HKDF(StorageMasterKey, random_object_salt,
                 "CYBOU/STORAGE/OBJECT" || ObjectID || key_epoch)
```

The exact input encoding and KDF/hash suite are freeze points. Never derive
ObjectID from a plaintext filename, path, AccountID, or an unsalted predictable
plaintext hash.

## Object encryption and manifests

```text
local file
→ bounded chunking
→ AEAD-encrypt each chunk with unique nonce/context
→ opaque ciphertext chunks
→ distributed Storage
```

Authentication data binds the ciphertext to the network, ObjectID, chunk
position/count, and selected profile. Exact serialization is specified with
the Storage wire contract. A private encrypted Files manifest contains
filename, MIME type, logical size, folder, ObjectID, salt, key epoch, and
private UI state. Manifest/root mutations are device-authorized through the
coordinator.

Provider-visible data is limited to what placement and durability require:
opaque ObjectID and ChunkIDs, ciphertext lengths/commitments, and lease/audit
metadata. Providers receive no plaintext filename, MIME type, path, folder,
Mail subject, content key, or reusable plaintext hash. Ciphertext size and
traffic timing may still reveal information and require explicit padding and
traffic-analysis analysis.

## Mail content and attachment keys

Mail content uses a fresh random content-encryption key (CEK), wrapped to each
verified authorized recipient device under the Mail KEM context. The sender's
hybrid signature authorizes the Mail operation; it is not the encryption key.

Each Mail attachment uses a fresh random 256-bit AttachmentKey, separate from
the Files Storage Master Key:

```text
file → standard AEAD with AttachmentKey → opaque Storage object
```

The encrypted Mail payload carries an attachment descriptor containing
ObjectID, AttachmentKey, filename, MIME type, logical size, and required
integrity metadata. Only after decrypting the Mail payload can the recipient
obtain the AttachmentKey. Attachment bytes remain outside MailTx and consensus
state. Mail signing, Mail KEM, Storage key wrapping, and object encryption use
separate authenticated contexts, including the target domains:

```text
CYBOU/DEVICE/SIGN
CYBOU/MAIL/KEM
CYBOU/STORAGE/KEYWRAP
CYBOU/STORAGE/OBJECT
```

## Save to Files and retention ownership

`Save to Files` MUST NOT require download, decryption, and re-upload when an
existing protected object can safely be reused.

```text
Storage Object
  ├── Mail retention reference
  └── independent Files retention reference
```

After the recipient has decrypted the Mail attachment descriptor, the client
may reuse the protected ObjectID and ciphertext, then wrap/reference the
AttachmentKey under the recipient's Files key domain and add a private Files
manifest entry. This creates independent Files retention ownership. Deleting
or expiring the Mail reference MUST NOT remove an object while a Files
reference remains active. If authorization, retention, or key-domain rules
make safe reuse impossible, the client may create a separately protected Files
object.

Retention claims are Storage-layer obligations, not permanent per-object
consensus records. Their lease/accounting aggregation is defined in docs 11–13.

## Open protocol freeze points

- Standardized hybrid KEM/key-package format and transcript binding;
- AEAD suite, chunk size, nonce construction, and associated-data encoding;
- Storage Master Key persistence, rotation, device addition, and revocation;
- opaque ObjectID/ChunkID construction and privacy properties;
- manifest format, signature/authorization, and conflict behavior;
- replication/coding profile, placement, lease, audit, repair, and retention;
- safe reference reuse and release behavior for Mail-to-Files ownership.

These open parameters do not weaken the frozen boundaries: separate signing
and encryption keys, ciphertext-only providers, no attachment bytes in
consensus state, and independent Mail/Files retention ownership.
