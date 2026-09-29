# Desktop → core integration

The desktop reaches Mail and Files only through `CybouApplicationBackend`
(`src/qt/cybouapplicationbackend.h`). The live implementation is
`CybouCoreApplicationAdapter`, created by `CybouDesktopController` and bound
to the unlocked Identity. Core types never reach pages, and Qt types never
reach core.

## Connected

- **Session:** while the Identity is unlocked the adapter owns one worker
  thread with the Identity's encrypted Application DB
  (`<datadir>/identities/<AccountID>/app`), a staging store, and the core
  `StorageService`, `PublicationService` and `ApplicationService`. Every core
  call runs on that worker; results reach the GUI as product snapshots.
  Locking joins the worker and drops the private projection.
- **Live text-only Mail:** send to a `.cybou` name (or a full AccountID),
  Inbox/Sent discovery from finalized history, read/star/archive/trash as
  local mailbox state, retry. State mapping:

  | Core job phase | Desktop state |
  | --- | --- |
  | QUEUED, WAITING_FINALITY | Waiting for confirmation |
  | SECURING (finalized, below remote target) | Securing |
  | PROTECTED (remote replica target met) | Protected → "Sent" |
  | NEEDS_ATTENTION | Needs attention |

  Sent mail rebuilt from history without a local job shows Protected only
  when StorageService reports the target met.
- **Live Files:** create folder, upload (streamed from disk into encrypted
  chunks), rename, move, copy (same protected content), trash, restore and
  delete, each as one `FILES_MUTATION_BATCH` publication; download streams
  verified content through StorageService into `<destination>.part`, renamed
  only on success. A pending change shows immediately and stays visible until
  history reflects that exact publication; a change made while another
  Identity operation is unconfirmed queues behind it. Trash does not keep the
  old location, so Restore returns items to My files. Starred and
  "Available offline" are device-local.
- **Attachments:** new local files are encrypted as child trees of the Mail
  publication (Compose keeps a device-local source path that is never shown
  or published); a Files item attached by reference (`ref-<file>`) reuses its
  protected root and key without re-upload. Received attachments download
  through StorageService, and "Save to Files" publishes a Files entry that
  references the same content. An attachment is shown as saved when a Files
  item references its content.
- **Capabilities:** `mail` and `files` turn on only once the adapter session
  has opened the core services.
- **Restore progress:** the Mail row follows the application scan.
- **Recovery phrase rotation:** `CybouDesktopModel` asks the backend to
  `prepareIdentityRotation` first. The adapter publishes the RecoveryBridge,
  waits until it is PROTECTED, verifies it with the new phrase, and only then
  lets `RotateIdentitySync` run. Locking or any failure keeps the current
  phrase active. Without remote storage providers the rotation waits.

## Not connected yet

- Drafts are device-local compose state held in memory by the adapter.
- Re-securing Sent/Files rebuilt from history after the Application DB was
  lost (their placement leaves are not reconstructed yet).
- Authority (read-only) once core implements it.
