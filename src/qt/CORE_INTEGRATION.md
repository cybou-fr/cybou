# Desktop → core integration request

The desktop reaches Mail and Files only through `CybouApplicationBackend`
(`src/qt/cybouapplicationbackend.h`). The live implementation,
`CybouCoreApplicationAdapter`, is blocked until core exposes the services
below. This file lists what the adapter needs; it does not prescribe core
internals, wire formats or storage design. Core types never reach pages, and
Qt types never reach core.

Status: **blocked**, core has no `ApplicationService`, `PublicationService`,
`StorageService` or restore scan yet.

## Threading and lifetime

- Calls may block; the adapter runs them off the GUI thread.
- Progress arrives through callbacks or a pollable snapshot. Either works;
  the adapter marshals onto the GUI thread.
- One service session per unlocked Identity: open with the unlocked vault,
  close on lock. Closing must drop plaintext Mail/Files state and indexes.

## ApplicationService (semantic Mail/Files state, per Identity)

Snapshots of the Identity's encrypted Application DB, as plain data:

| Need | Fields the desktop renders |
| --- | --- |
| Mail list | stable id, folder, from/to `.cybou` names, subject, preview, body, time, unread, starred, draft, attachments |
| Attachment | stable id, name, logical size, saved Files reference |
| Files list | stable id, name, parent id, folder flag, logical size, modified, starred, trashed |
| Change feed | "item changed / removed" events, or a revision number to re-snapshot |

Commands (client-supplied ids for created items are accepted or mapped):
save/delete draft, mark read, star, move folder, create folder, rename, move,
copy, trash, restore, delete forever.

Mail/Files search reads this data only, never the ChunkStore.

## PublicationService (outgoing content)

- `send(message, attachments)` and `upload(local path, parent)`, where an
  attachment is a local path or an existing protected Files reference (reuse,
  with no re-upload).
- Per-item lifecycle events matching the desktop states: Preparing,
  WaitingForConfirmation (submitted, not finalized), Securing (finalized,
  below the remote replica target, with optional percent), Protected (target
  met), NeedsAttention (with a user-facing reason), TemporarilyUnavailable.
- `retry(id)` for NeedsAttention.
- Advanced details only: operation id, finalized height, root id.

"Protected" means the durability target is met, not just finality.

## StorageService (retrieval and local availability)

- `download(id, destination)` with Downloading, Verifying, Decrypting, Ready
  or failed-temporarily events.
- Per item: whether decrypted content is available on this device
  (`available_offline`).
- Aggregate storage used and quota for the Identity.

## Restore

After a mnemonic restore: Mail and Files rebuild state (Pending, Running,
Done), and items stream in as they are indexed, so the desktop is usable
before the rebuild finishes. Content bytes are not fetched by a restore.

## Availability

A flag per service saying whether it can do work now. The desktop turns
`capabilities.mail` and `capabilities.files` on only when it is true.

## Later: Authority (read-only)

Authority, tier and the three budget allowances (Protocol, Storage,
Bandwidth) for the Identity, computed by core. The desktop shows no
placeholder numbers until this exists.
