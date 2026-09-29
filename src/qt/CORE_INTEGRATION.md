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
- **Capabilities:** `mail` turns on only once the adapter session has opened
  the core services. `files` stays off.
- **Restore progress:** the Mail row follows the application scan.

## Not connected yet

- Files (catalog, upload, download) — core `PublishFiles`,
  `ApplicationService::ListFiles` and `StorageService::Fetch` exist.
- Mail attachments and Mail ↔ Files reuse — core supports both.
- Drafts are device-local compose state held in memory by the adapter.
- Rotation: the Identity page must call `PublishRecoveryBridge`, wait for
  PROTECTED and `VerifyRecoveryBridge` before `RotateIdentitySync`.
- Re-securing Sent/Files rebuilt from history after the Application DB was
  lost (their placement leaves are not reconstructed yet).
- Authority (read-only) once core implements it.
