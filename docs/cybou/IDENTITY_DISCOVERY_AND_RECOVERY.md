# Identity publication discovery and recovery

Status: CURRENT
Scope: Encrypted-content/KEM and application recovery source audit at 531dc0da, 2026-10-09. Existing component regressions are identified; no fresh C++ suite, live acceptance or standards conformity is claimed.

ApplicationService scans canonical finalized RootPublications and builds only the
private application state accessible to its Identity.

## Scanner

Persist a last-scanned finalized height.

For each new finalized block:

```text
for each AuthorizedRootPublication:
    use outer authorization as publisher truth
    inspect recipient capsules
    try only locally recoverable KEM epochs
```

Failure to open a capsule:

```text
discard
```

Do not create permanent `NOT_FOR_ME` records.

Successful unwrap yields:

```text
RootChunkID
main ContentKey
publisher AccountID/nonce/key_epoch
OperationID / finalized height
```

Persist the positive accessible-publication record.

Checkpoint advancement and discovered records must be crash-safe/idempotent.

## Retrieval decoupling

An accessible publication whose root is temporarily unavailable must not block
scanning later blocks.

ApplicationService may maintain transient states:

```text
DISCOVERED
FETCHING
DECRYPTED
INDEXED
TEMPORARILY_UNAVAILABLE
```

## Application routing

After authenticated decryption, inspect the private application schema:

```text
MAIL_MESSAGE
FILES_MUTATION_BATCH
IDENTITY_RECOVERY_BRIDGE
```

and update the encrypted per-Identity Application DB.

Generic child file/attachment content needs no separate application type.

## Self publications

Recoverable owner content uses a self capsule, so the same scanner can rebuild
Sent Mail and Files.

## Clean-machine recovery

From the current mnemonic and verified network history:

1. restore and verify the current Identity;
2. rebuild Wallet/Names/canonical state;
3. first scan own accessible publications to find Files/Sent content and a
   RecoveryBridge if needed;
4. recover/verify historical KEM epochs;
5. scan all finalized RootPublications using all recoverable epochs;
6. rebuild Inbox/shared private content;
7. retrieve large child content on demand.

The old Application DB, old chunk cache and old provider-placement metadata are
not required inputs. Recovery still requires available canonical history, an
accessible capsule and retrievable encrypted chunks. A self capsule does not
recreate deleted/lost ciphertext. After rotation, unavailable or absent bridges
can prevent recovery of publications addressed to old KEM epochs.

Provider placement need not be recovered. A client may query discovered storage
peers by ChunkID and re-establish current durability after recovery.

## Current implementation and component evidence

[ApplicationService](../../src/cybou/application_service.cpp) uses the sparse
finalized-publication index, starting no earlier than Identity creation. Accessible
records and relevant-block checkpoints commit together in encrypted DB batches.
Unopenable capsules create no permanent negative record. Temporarily unavailable
roots are retried with bounded backoff while later publications continue scanning.
An imported bridge triggers another pass over earlier publications; subsequent
Scan calls continue bounded work. Revoked publications are skipped on clean
recovery; previously indexed local content is a separate cache concern.

A bridge is accepted only for this Identity's own publication. Each historical
seed must reproduce the canonical package at its epoch before being imported;
current mnemonic alone does not derive earlier seeds. PublicationService builds
a bridge with current self and future-epoch capsules and verifies opening with
the proposed new mnemonic. Finality/durability must precede irreversible rotation
under [the recovery contract](76_IDENTITY_VAULT_RECOVERY.md).

Existing [application regressions](../../src/test/cybou_application_service_tests.cpp)
cover offline Mail/Sent rebuild, unavailable roots, Files rebuild, interrupted
indexing, clean rotation bridge recovery and bridge construction after Application
DB loss. These are component fixtures, not proof of live multi-host durability,
full clean GUI acceptance or recovery after every possible data loss.
