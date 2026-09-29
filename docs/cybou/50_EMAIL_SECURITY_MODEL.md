# 50 — Mail and Files content security

Mail and Files are private application data over generic RootPublication and
the shared encrypted chunk tree.

## Protected assets

- Mail subject/body/attachments;
- filenames/folder structure;
- content keys;
- recipient relationships;
- private application indexes;
- drafts and decrypted cache state;
- Identity secrets.

## Network-visible boundary

RootPublication exposes only generic publication fields required for consensus,
recipient key wrapping and provider admission.

Providers receive opaque encrypted chunks, ChunkIDs, finalized-publication
references and admission/accounting metadata.

They do not need plaintext application type, filename, folder or Mail subject.

## Sender attribution

For Mail, authoritative sender AccountID comes from the outer
`AuthorizedRootPublication` authorization.

Do not trust a decrypted application field to redefine the sender.

## Self recovery

Recoverable publisher content uses a self capsule.

For one-recipient Mail:

```text
recipient capsule
self capsule
```

This allows clean reconstruction of Sent Mail after loss of the previous
Application DB.

## Attachments

New attachment trees and the Mail main root may be staged under one
RootPublication authorization Merkle tree.

The encrypted Mail root privately contains attachment RootChunkID/ContentKey
references.

Existing protected Files content may be referenced without ciphertext
re-upload when authorization/retention permits.

## Local security

The common ChunkStore contains encrypted bytes only.

The per-Identity Application DB is encrypted at rest and becomes readable only
after Identity unlock.

The GUI consumes the semantic Application DB/model and never enumerates foreign
provider chunks.

## Finality and durability

```text
PoA-finalized
!=
Sent / Protected
```

Beta `Sent` requires the publication's required chunks to reach two
independent remote replicas. The local encrypted copy does not count.

## Recovery

Clean-machine recovery starts from the current mnemonic and verified public
history. Historical KEM material required after rotation is recovered through
the private RecoveryBridge flow, then old accessible publications are scanned.

## Threat controls

- key substitution -> finalized Identity KEM lookup and bound capsules;
- malformed content -> bounded canonical parsing/tree limits;
- chunk corruption -> BLAKE3 + AEAD verification;
- provider loss -> remote replication + audit/repair;
- metadata leakage -> encrypted application roots;
- local disk exposure -> ciphertext ChunkStore + encrypted Application DB.
