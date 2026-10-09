# Identity publication discovery and recovery

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Every client scans canonical finalized RootPublications and builds only the
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
not recovery requirements.

Provider placement need not be recovered. A client may query discovered storage
peers by ChunkID and re-establish current durability after recovery.
