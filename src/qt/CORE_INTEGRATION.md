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
  verified content through StorageService into a QSaveFile temporary file and
  atomically commits it only on success (no direct-write fallback). Failed
  verification/commit preserves an existing destination. A pending change shows immediately and stays visible until
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
- **Drafts:** device-local compose state stored in the encrypted Application
  DB (`ApplicationService::SaveDraft/ListDrafts/DeleteDraft`), including local
  attachment paths and Files references; never published and not rebuilt from
  history. Commands issued just before locking still complete.
- **FeatureAvailability:** `mail` and `files` turn on only once the adapter session
  has opened the core services.
- **Restore progress:** the Mail row follows the application scan.
- **Recovery phrase rotation:** `CybouDesktopModel` asks the backend to
  `prepareIdentityRotation` first. The adapter publishes the RecoveryBridge,
  waits until it is PROTECTED, verifies it with the new phrase, and only then
  lets `RotateIdentitySync` run. Locking or any failure keeps the current
  phrase active. Without remote storage providers the rotation waits.

- **Rotation in a running session:** after a finalized IdentityRotate the
  adapter reopens the session (drafts carried), and the Application DB is
  rebuilt from history, the RecoveryBridge and provider-held placement
  proofs. A rotation finalized while the desktop was closed rebuilds the
  same way on next unlock, but drafts from before it cannot be decrypted.
- **Durability audit:** the worker audits a few chunks of one Protected
  publication every few ticks; lost copies return it to Securing and the next
  pass repairs it.

## Connected operator surface and remaining UX work

The existing NetworkAuthorityPage already binds finalizer controls and a
one-period settlement action through the desktop model/controller. It is not
an absent read-only service waiting for a new core. A richer verified explorer,
scoped map/uptime observations and own-content console remain product targets.

Mail draft save/send preparation and mailbox move now report Running /
Committed / Failed through GUI callbacks correlated by the model's local task
ID/session generation. Committed is local durable work, never Sent/finality.
Compose autosaves, waits before clearing on close/send and retains failed text.
An encrypted draft-to-message binding survives restart/stale compose replay;
publication retry resumes that job rather than preparing a second publication.
Successful handoff retains the binding while deleting the draft; explicit
discard removes it. Shell rebuilds follow pending send tasks through the model.
A private content fingerprint excludes save timestamps and refuses edited
recipient/text/attachments as a resume of an existing publication job.

Files Advanced expansion is retained during details refresh, active-job replica
counts are populated and identical semantic snapshots suppress replacement
signals. Home uses stable activity IDs and cached rows, reads current local
semantic indexes through a coalesced worker refresh and scopes its timestamp to
that local view. Late/interrupted requests and lock are handled; this read does
not force sync/history scans/provider audits/publications. Long Home content
scrolls vertically and activity labels wrap as plain text. Incremental Mail
rows, ID handoff, evidence freshness, native drag/drop
acceptance and broader command acknowledgements remain delivery work.
The worker/session split already exists; profile
queue and stop/join latency before changing scheduling or ownership.

See `docs/cybou/DESKTOP_UX_DELIVERY_PLAN.md` and the product contracts for
source-reviewed gaps, dependencies and acceptance. This note records source
integration, not current-worktree build or live Beta validation.

Files creation commands share one random private item ID across pending and
indexed state. Folder imports discover a bounded tree off the GUI thread before
any creation command: 10,000 entries, depth 64, no symbolic links. GUI staging
yields between batches (up to 8 commands / an 8 ms scheduling budget); a single
filesystem/model call can exceed that budget. Cancel, lock, readiness loss and
page destruction stop future staging, without rolling back accepted jobs. A
filesystem call already in progress can delay discovery-worker cancellation;
page teardown does not join that worker on the GUI thread. Discovery paths remain
in private memory and no discovery manifest/log is persisted.

Files table/grid reconciliation now retains objects by private item ID instead
of clearing both views on every snapshot/status update. Progress captions update
in place; sort and insertion retain selection/current items and table anchors.
List/grid switches carry the visible selection, removed/filtered items are
dropped, lock clears private navigation/rows. Folder counts are aggregated once;
icons are reused per glyph. The 2,000-item Windows fixture measures initial
construction at 796 ms and ten one-file updates at median 11 ms/max 12 ms.
Initial per-item widget cost and completed-frame/large-catalog evidence remain
open; these timings exclude live runtime/storage work.

The subsequent Files status delegate removes per-row QLabel construction.
Statuses stay in plain display/accessibility roles and tooltips; Qt table-cell
accessibility reads them directly. Native 2,000-item fixture construction/update
now measures 90 ms / median 8 ms, max 9 ms; 10,000 measures 527 ms / median 43 ms,
max 49 ms. This supersedes the earlier per-widget timing, but excludes completed
frames and live storage. Large snapshot/coalescing, scroll/frame and physical
screen-reader/mouse/DPI acceptance remain open.


Mail reconciliation slice (2026-10-05): visible list items are retained by ID,
unaffected row widgets are reused, and only changed rows are repainted/replaced.
Selection, current item and scroll anchor survive metadata updates/insertion;
filtered/removed selections are dropped and lock clears private row caches/search.
Folder targets/count labels are retained. Outgoing Archive/Trash rows show the
recipient. The model adopts a replacement outgoing ID before removing the old
projection; list and reader transfer their current ID/selection to it. Move
acknowledgement and durable draft/send ownership rules remain in force.

Isolated review build passed; full Qt suite 59 passed/0 failed
(`qt-mail-rows-full.txt`), focused Windows suite 6 passed/0 failed including
setup/cleanup (`qt-mail-rows-windows.txt`). Regression covers a 100-message list,
retained unrelated widgets and folder target, badge count changes, multiple
selection/current/anchor, ID replacement with an open reader, insertion,
removal/filter/lock and existing failure/commit/context-menu paths. Native Inbox
fixture screenshot was inspected. Baseline `1ee870f` plus worktree; no deployment.
Large mailbox construction still creates row widgets and needs profiling/delegate
work; physical mouse/multi-DPI and broader assurance acceptance remain open.


Mail reader context and privacy slice (2026-10-05): unrelated Mail/status updates
preserve body selection, scroll and unchanged attachment widgets. Attachment
retrieval changes update controls; switching messages resets selection/scroll.
Subjects and bodies render as plain text. Lock or a missing message clears
private labels and attachment caches; scoped Security Details closes on loss of
access, while unrelated changes keep it open. Details are an opening-time
inspection, not live evidence. Outgoing direction survives Archive/Trash.
Replaced Mail row widgets are hidden immediately before Qt deferred deletion,
preventing transient overlapping text noticed in native fixture capture.

Remaining acceptance: large mailbox construction/delegate profiling, completed
frame/scroll timings, physical mouse and multiple DPI settings. Runtime rotation
must retain an honest record of timing failures; a passing rerun does not erase
the initial failed run. No user desktop restart or network deployment occurred.

Validation: initial isolated reader build passed (`build-mail-reader.txt`).
Native Windows focused suite: 5 passed/0 failed including setup/cleanup
(`qt-mail-reader-windows.txt`). Full Qt run: 59 passed/1 failed
(`qt-mail-reader-full.txt`); the failure was the rotation completion deadline
in `rotationKeepsLiveSessionWorking`, outside reader presentation. Its isolated
rerun passed, 3/0 including setup/cleanup (`qt-mail-reader-rotation-recheck.txt`).
The initial failure remains recorded; suite-wide clean acceptance is not claimed.
Native reader fixture was inspected and revealed deferred row-widget overlap,
subsequently corrected with an immediate hide and regression assertion.

Final overlap fix validation: isolated GUI/test build passed
(`build-mail-reader-final.txt`); Windows reader/list/command focused suite
5 passed/0 failed, including setup/cleanup (`qt-mail-reader-final-windows.txt`).
The list regression asserts that a replaced row is already hidden before Qt
processes deferred deletion. Source hashes are recorded in source-manifest.json;
concurrent repository commits are represented by its current HEAD plus worktree.
