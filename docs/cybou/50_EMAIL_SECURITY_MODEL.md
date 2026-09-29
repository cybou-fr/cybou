# 50 — Mail and Files content security

Mail and Files content is application data inside the shared encrypted payload
tree. Generic RootPublication exposes only the fields required to authorize
publication and provider admission. Recipient capsules do not reveal recipient
AccountID. Filenames, folder structure, Mail schema, subject, body, and
application indexes remain encrypted.

## Protected assets

- plaintext Mail and Files content;
- Identity recovery and authorization secrets;
- recipient KEM private material and content-encryption keys;
- private recipient, filename, path, and sharing metadata;
- integrity and origin of finalized publications;
- local indexes, drafts, and decrypted caches.

## Trust boundaries

The PoA operator controls ordering and can censor or stop finality. It cannot
forge Identity authorization or decrypt payload content. Full nodes validate
the same canonical operations and state transitions. Storage providers receive
opaque ciphertext, content addresses, finalized-publication references, and
Merkle proofs; they do not receive plaintext schemas or keys.

Finality authorizes chunk admission but does not prove availability or
durability. Clients verify each content address and AEAD tag, retain local
staging until the full tree verifies, and expose data only after successful
decryption. Product states must distinguish finalized, available, retrievable,
and protected.

## Threats and controls

- **Key substitution:** accept recipient KEM capabilities only from verified
  finalized Identity state and bind capsules to network, publication, sender
  authorization context, and recipient key epoch.
- **Replay or nonce reuse:** serialize account operations through the durable
  Identity coordinator; reconcile uncertain delivery before retrying.
- **Malformed content:** bound canonical-CBOR parsing, ciphertext sizes, chunk
  depth/count, and output; do not execute active content.
- **Provider loss or corruption:** verify BLAKE3 content addresses and Merkle
  admission proofs; treat replication and repair as measured storage
  guarantees, not as consensus finality.
- **Metadata analysis:** keep Mail and Files schemas, names, paths, and recipient
  identities inside authenticated encryption. Public capsule count and key
  epochs remain acknowledged metadata leaks.
- **Long-term ciphertext exposure:** use the reviewed hybrid-PQ KEM profile and
  keep mail/content keys separate from Identity signing keys.

Local plaintext protection remains the desktop client's responsibility after
decryption. Clean-machine recovery and key-loss behavior are specified in
`IDENTITY_DISCOVERY_AND_RECOVERY.md` and `76_IDENTITY_VAULT_RECOVERY.md`.
