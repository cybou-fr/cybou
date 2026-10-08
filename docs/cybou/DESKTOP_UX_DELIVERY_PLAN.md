# Desktop UI/UX delivery plan

Remaining-work review (2026-10-06): the current prioritized backlog, fresh fixture
audit scope and acceptance gaps are in
[DESKTOP_UX_REMAINING_WORK_2026-10-06.md](DESKTOP_UX_REMAINING_WORK_2026-10-06.md).
The dated delivery records below remain historical evidence, not a current-HEAD
full acceptance statement.

Status: Level 4 implementation plan, source review 2026-10-05 at HEAD `cc6c18e`;
first implementation slice based on HEAD `77a29d2` is recorded below. This plan is
not a claim that its acceptance gates have passed. Product requirements live
in docs 72/73/78/81–85 and NETWORK_AND_ADVANCED_UX; architecture remains governed
by AGENTS and higher-level documents.

## Outcome and release scope

Deliver an Identity-centered productivity tool with Gmail-like Mail, Drive-like
Files and clear Wallet review. Routine tasks have predictable outcomes, stable
screens, recoverable errors and honest progress. Advanced functionality adds
inspectable evidence without complicating onboarding.

First close reliability and data-loss risks, then improve shared interaction
and appearance, then add scoped Network/operator tools. Basic Network overview
is a target Beta addition; global observation/ranking, richer explorer, console
and active benchmarks are staged extensions, not prerequisites invented for
every Beta action. Stronger reliability/deletion claims require backend evidence
and governance, not a visual redesign.

## Initial source review, gap and decision

This table records the inspected baseline; the delivery record below identifies
which gaps have since been addressed in the working tree.

| Area | Source evidence in current tree | Delivery decision |
|---|---|---|
| Live Mail/Files | CybouCoreApplicationAdapter owns encrypted Application DB and three live core services; R6 history indexing and R7 projection/scheduler split are implemented | Reuse these boundaries; do not plan a second data plane or worker architecture |
| Archive/trash | ApplicationService::MoveMail commits local mailbox state; adapter moveMail is void and reports generic commandFailed; EmailPage::moveMessagesTo shows success immediately | Correlated local durable acknowledgement before success; measure queue latency, no PoA wait |
| Draft save/send | Adapter optimistically inserts drafts before SaveDraft; MailCompose::saveDraftAndClose clears after request; DesktopModel::requestSendMail deletes the old draft before preparation finishes | Save acknowledgement, debounced autosave and loss-safe ownership transfer before deleting drafts |
| Mail dragging | EmailPage::eventFilter and startIdDrag already implement ID drag/drop | Reproduce the reported native mouse failure with real row children/selection/DPI; repair routing, not duplicate a DnD subsystem |
| File details | StoragePage filesChanged rebuilds details; rebuildDetails creates Advanced hidden | Persist presentation state by semantic ID, retain through pending-to-indexed mapping; clear on lock/removal |
| Protection evidence | FilesProjection::FilesSnapshot fills counts in finalized-without-current-job branch but not current-job branch; details treats unknown count as absent network storage | One complete scoped evidence projection for both branches; unknown is not zero; safe blocker/freshness fields |
| Files → Mail | MainWindow callback and attachmentFromFile exist; Protected-only private reference path, toolbar/context menu share gate | First validate working Protected path and explain unavailable actions; pending-file compose is a separate gated slice |
| File destination | StoragePage::promptMove resolves labels.indexOf(choice) | Use unique destination IDs and breadcrumb labels; test duplicate names and invalid descendants |
| Activity stability | HomePage::refresh clears/recreates activity on multiple signals; activity DTO lacks a stable event ID | Stable event IDs and incremental/coalesced updates, refresh freshness UI |
| Blocking work | StoragePage::uploadPaths recursively enumerates folders on UI; Identity worker shares maintenance/commands and shutdown joins | Move enumeration off UI; instrument queue/task/join before changing scheduler semantics |
| Network map | Node diagnostics expose local peers/advertised heights; Geo provides France country admission, not cities/uptime/global membership | Honest schematic map of observed connections; wider telemetry is research, no fictitious top 300 |
| Authority | NetworkAuthorityPage already has finalizer controls, candidate/peer/chain summaries and one-period settlement | Extend existing controls with indexed verified explorer and evidence readiness; no endpoint-based authority |
| Wallet | Review, asynchronous coordinator, pending ledger and irreversible SystemLock already exist | Refine review/recovery/fee estimates; no duplicate wallet service or optimistic spendable balance |
| Durability/deletion | Remote copy/audit/repair and revocation/purge mechanisms exist; canonical state has no placement reliability; DEV replicas share a VPS | Distinct StorageIds/accounts do not prove independent failure domains; per-object purge/global uptime claims remain gated |

Relevant source paths are under `src/qt/`: cybouapplicationbackend.h,
cyboudesktopmodel.{h,cpp}, cyboucoreapplicationadapter_{mail,files,storage,
identity_session,session_scheduler}.cpp, pages/{emailpage,mailcompose,storagepage,
homepage,networkauthoritypage}.cpp and cyboumainwindow.cpp. Core facts are in
`src/cybou/application_service.cpp`, storage service, node diagnostics and P2P.
Function anchors above are preferable to line numbers as this tree is changing.

The user screenshot shows a finalized file at height 293, local offline
availability, 1/2 remote copies and disabled Download/Send. That combination
needs an explanation; it does not prove a stuck finalizer. Auto-closing Advanced,
activity flicker and failed dragging are user reports; Advanced recreation and
activity replacement have source evidence, while the exact drag failure still
needs native reproduction.

## Work packages, in dependency order

### W0 — Baseline and measurements

Capture a reproducible current GUI build and record worktree/revision and test
environment. Keep independent network edits intact. Add bounded timing for
command enqueue/start/commit, projection building, folder enumeration and UI
refresh; no private text/paths or keys in diagnostics. Reproduce archive, draft
failure, Advanced refresh, activity flicker, duplicate folder names and native
drag/drop. Compare with existing tests, do not treat their earlier passes as
current-worktree acceptance.

Exit: each confirmed defect has a minimal scenario, source path and measurable
or observable pass condition. Profiling decides whether task priority/budget
changes are needed; slower refresh alone is not the assumed cure.

### W1 — Durable local commands and draft safety (release priority)

Extend desktop backend/model results with command correlation, item IDs and
session generation. Preserve compatibility of core service boundaries, not an
obsolete API path. Serialize conflicting moves and Undo. Model queued/running/
committed/failed local states separately from publication progress. Present slow
archive inline and in a non-blocking task panel with redacted details; success
only after Application DB commit. Handle per-item batch failures/retry.

Add draft Saving/Saved/Failed and debounced asynchronous autosave. Keep compose
content on save failure; transfer ownership to a durable outgoing job before
draft deletion. Handle recipient-resolution/preparation failure and lock/close
without loss or duplicate publication. Preserve exact-byte reconciliation for
network delivery uncertainty. Define cancellation only at actually safe phases.

Exit: injected SaveDraft/MoveMail failures never show success or lose content;
slow queue is visible and responsive; Undo ordering is correct; restart/lock
tests recover acknowledged drafts and pending outgoing work. Preserve shared
Identity nonce coordination across Wallet/Names/publications.

### W2 — Stable models and Files evidence

Give activity semantic stable event IDs; preserve file pending/indexed identity
mapping. Emit changed snapshots/rows rather than unconditional replacement.
Keep details expansion, selection, scroll and focus through updates, theme and
language changes; lock clears private views. Coalesce hot signals; hidden pages
avoid presentation work, urgent completion/failure still arrives. Add Last
updated and bounded manual Refresh without audit/rebuild side effects.

Populate both Files projection branches with measured minimum remote copies,
target, last observation and safe blocker when available. Keep confirmation,
protection, local availability and retrieval separate. Missing evidence is
unknown, not zero; don't infer a root cause from elapsed time. Existing provider
diagnostics may not explain all blockers: add safe typed reasons at the owning
service boundary as needed rather than deriving them in Qt.

Exit: repeated unchanged snapshots cause no flicker; Advanced remains open;
1/2, unknown, stale, repair and failure fixtures are distinct; pending-to-finalized
reconciliation retains user context. Failure-domain evidence remains explicitly
unavailable unless measured.

### W3 — Complete daily Mail/Files workflows

Repair existing mouse drag/drop using one shared move command for context menu,
toolbar, keyboard and drop. Provide target hover feedback and partial-batch
results. Verify real row widgets, multi-selection and DPI, not only synthetic
MIME events. Fix destination picker IDs/breadcrumbs. Enumerate folders off the
GUI thread with bounded progress and cancellation before staging.

Exercise Protected Files → Mail → durable draft → send → receive → Save to
Files end to end. Consistent gates show the exact unavailable reason; callback
failure cannot silently do nothing. Verify reusable references, authorization,
retention and source deletion without unnecessary duplicate upload.
Separately evaluate local verified Open/Download when network protection is
incomplete; enable only with core-reported safe availability and verification.
Review verified-download destination replacement: DownloadContent currently
removes an existing destination before renaming the .part file. Preserve the
previous file if replacement fails and show an explicit overwrite decision;
verification success alone is not successful destination commit.

Exit: the same action behaves consistently across entry points; folder names
cannot select wrong IDs; foreground interaction stays responsive; verified
attachment reuse and post-source-removal retention pass with real services.

### W4 — Shared interaction and visual polish

Refine shell, search scopes, density, list columns, responsive details drawer,
shortcuts, empty/error states and shared task panel. Use Identity readiness as
the common entry point. Improve theme contrast, selected/checked states,
disabled-action explanations, focus and French localization. Preserve compose
and selection across theme/language changes. Home layout is chosen by readability
and task completion, not strict conformity with earlier vertical-card drawings.

Wallet review shows final recipient, amount, fee and irreversible lock; service
budget forecasts state lease/rent assumptions or stay hidden. Onboarding
explains explicit local capacity and recovery scope without requiring completed
peer sync or a node-role choice.

Exit: main flows need no Advanced panel, all primary actions work with keyboard,
both themes and supported DPI/window sizes have no clipping or ambiguous states.

### W5 — Scoped Network overview and schematic France map

Add bounded diagnostics DTOs and a Network page/list/map with explicit source,
scope and freshness. Initial map uses only locally observed connections and
illustrative placement. Include connected peers, local verified chain state,
own protection summary and capacity aggregates. Separate private/LAN/unknown
observations; no invented city or census. Slow/polling refresh cannot blink or
interrupt a selected peer. Local observational history needs bounded retention.

Exit: no-data/offline/quiet-chain cases are honest; list alternative and keyboard
navigation work; a small peer sample stays small; no endpoint/Identity linkage
or storage proof is added solely for display. Top100/200/300 ranking waits for
real observations and a declared window; it never affects randomized placement.

### W6 — Authority explorer and operational depth

Extend the existing Authority page with paginated locally verified blocks,
operations, account values, publications, leases and settlements where APIs
exist. Distinguish local candidates and off-chain evidence. Add worker/queue
timings, safe errors, settlement completeness and authorization explanations.
Finalize/pause/settle keep current local key and signing-journal safety checks.

Exit: ordinary public read access cannot activate a signer; no peer name/IP
confers authority; explorer has no plaintext content; canonical/off-chain facts
remain distinct; missing evidence aggregation is visible. Network-wide storage
reliability is not claimed from local samples.

### W7 — Own-content inspector and read-only console

Resolve only unlocked Identity-owned semantic file/publication references.
Expose bounded chunk trees and verification/retrieval evidence without scanning
foreign provider objects. Add a restricted read-only grammar and help for
implemented commands, cancellation and output limits. Redact exports and clear
private output/history on lock. Initially no shell, mutation or signing commands.

Exit: access-boundary and lock-race tests show no keys/foreign objects/private
residual output; large graphs stay bounded and cancellable; console absence
does not block routine file management.

### W8 — Assurance, recovery, deletion and measured Beta acceptance

Add plain-language per-object evidence and deletion phase explanations backed
by real APIs. No universal RGPD/crypto-erasure badge. If per-object provider
purge outcomes are desired, define their privacy/evidence contract first.
Run clean-machine restore, rotated historical-KEM recovery, unavailable-provider,
shared-reference purge and injected unlink-failure/restart scenarios.

Beta durability acceptance uses two measured independent remote failure domains,
not the two colocated DEV peers as proof. Passive performance summaries precede
any optional active DEVNET test-build panel. BUILD_TESTS loadgen/smoke/soak stay
outside production features. Live benchmarking requires a bounded run plan,
not an automatic page-open action.

Exit: recorded scope, environments and source revision support every displayed
assurance; unresolved legal/governance or evidence gates stay open. Attach
failure/performance results, not screenshots alone, to Beta acceptance.

## Deferred design decisions

1. Compose while a Files publication is pending: durable reference ownership,
   source retention, restart and duplicate-staging semantics before enabling.
2. Wider network observation feed: source integrity, minimization, retention,
   session/endpoint churn and clearly scoped uptime before ranking.
3. Failure-domain independence: evidence and provider-selection design remain
   in higher-level storage/security gates, never improvised by Qt.
4. Per-object remote purge outcomes and stronger erasure: evidence/privacy
   design and recovery interaction; no promise from absent acknowledgements.
5. Production active diagnostic checks versus DEVNET-only loadgen UI: define
   resource/cost/authorization limits before adding any command.

## Validation and delivery slices

Implementation pass based on HEAD `77a29d2` (2026-10-05): local Mail progress
callbacks/tasks, draft autosave/acknowledgement, durable send binding and shell
rebuild follow-up are implemented in the working tree. Archive batch feedback
waits for commit and Undo operates on successful items. Reader toolbar actions
and Delete use the same acknowledgement rule. The worker no longer
drops the rest of a dequeued command batch after one exception.
The send binding's private content fingerprint rejects altered payload as a
resume of an already saved job; the edited draft is kept for a new message.

W2/W3 partial delivery: identical semantic snapshots suppress signals, Home
uses stable activity IDs and cached rows with in-place updates; Files Advanced
and scroll survive details refresh, active-job copy counts are filled, unknown counts are explicit,
folder destinations use IDs/breadcrumbs, Files-to-Mail gates explain protection
and downloads commit atomically without deleting a previous destination first.
Mail row children now pass mouse input to the drag/select viewport. These changes
include a coalesced manual local-view refresh, scoped refresh timestamp,
failure/interruption handling and lock cleanup. Home scrolls vertically at
small sizes and renders wrapping activity labels as plain text. Full Qt and
focused Windows checks are recorded against `da6d6b9` plus the UI worktree.
Remaining gates include physical mouse drag acceptance, incremental Mail/Files
Mail row work and ID handoff,
evidence freshness and W5–W8 extensions. Validation results
are recorded in `26_IMPLEMENTATION_STATUS.md`.

Deliver small reviewable changes in the order above. W1 precedes final DnD/
batch success/Undo work; W2 precedes new frequently refreshed pages; W3 baseline
references precede pending-file compose; W5 DTOs precede wider ranking; W6 safe
read APIs and W7 privacy gates precede console/test extensions. W4 can follow
stable state components incrementally; W8 evidence work runs throughout and
closes release claims. Do not turn these packages into a single rewrite.

Use focused core/Qt failure and session tests for behavioral changes, native
mouse/DPI checks for drag/drop, fixture coverage for visual states, and real
two-desktop/storage acceptance for distributed behavior. Record timing and
responsiveness with realistic datasets; set budgets from W0 measurements.
Earlier 50-case Qt results at the committed baseline are historical regression
evidence, not validation of current dirty sources or this documentation edit.

Files identity delivery (2026-10-05): upload, create-folder, copy and save-from-Mail
now receive a single private item ID from the desktop model. The core adapter
uses it unchanged through pending projection and encrypted catalog indexing;
the temporary-ID alias map is removed. The live-core regression checks folder
and upload return IDs and preserves the open Advanced drawer across finality.
The bounded asynchronous folder-enumeration slice is recorded below.

Validation of the permanent Files ID slice: isolated GUI/test build passed;
55 Qt results passed, and 5 focused Windows results passed (including setup
and cleanup). Logs: `artifacts/uiux-implementation-20261005/qt-files-id-full.txt`
and `qt-files-id-windows.txt`. Source baseline: HEAD `1ee870f` plus worktree.

Folder-import W3 delivery (2026-10-05): bounded discovery runs on a read-only
worker before staging. A nonmodal progress dialog counts discovered/queued items
and offers Cancel. The GUI submits small batches into the existing publication
path; error/limit rejection creates nothing, partial cancellation reports only
accepted items. Lock, account change, Files unavailability and page destruction
stop future staging. Hidden files and empty folders retain their tree positions;
symbolic links are skipped. No folder-import persistence or protocol entity is
introduced. Remaining W2/W3 work includes incremental Mail rows, evidence freshness,
large-catalog measurements and physical mouse/DPI acceptance.

Validation of folder import: isolated review build passed; full Qt suite
56 passed/0 failed (`qt-folder-import-full.txt`), focused Windows suite
5 passed/0 failed including setup/cleanup (`qt-folder-import-windows.txt`).
The native progress screenshot was inspected. Regression coverage includes
structure/hidden files, early and partial cancellation, discovery failure,
over-limit selection, duplicate start, Identity lock and page destruction.
A blocking filesystem call cannot be forcibly interrupted; no slow-volume or
10,000-item latency acceptance claim is made. Baseline `1ee870f` plus worktree.

Files row W2 delivery (2026-10-05): table and grid reconcile by private item ID,
retain unchanged objects/chips and update metadata/progress in place. Sorting
uses retained ranked items; removed/filtered objects lose selection. View-mode
switches transfer visible selection/current item. Table insertion preserves its
visible anchor; updates preserve scroll. Lock clears rows and private navigation.
Folder child counts use one catalog pass, and identical glyph icons are reused.

Native Windows synthetic 2,000-file measurement: construction 796 ms; ten
one-file updates median 11 ms, max 12 ms (`qt-file-rows-final-windows.txt`).
These measure synchronous fixture model/page work, excluding network, storage
I/O and frame-completion latency. Construction remains a material UX gap: next
reduce per-item widget cost using viewport delegates/lazy presentation, retaining
accessibility and selection/DnD behavior. Measure first visible frame, scroll and
10,000-item catalogs before setting release budgets. Mail incremental rows,
evidence freshness and physical mouse/multi-DPI acceptance remain open.

Validation of Files rows: final isolated build passed
(`build-file-rows-icons.txt`); full Qt suite 58 passed/0 failed
(`qt-file-rows-final-full.txt`), focused native Windows suite
7 passed/0 failed including setup/cleanup (`qt-file-rows-final-windows.txt`).
Coverage includes retained row/grid/chip objects, metadata/progress, multiple
selection/current item, mode transfer, sorting/rename, insertion anchor,
removal/filter/lock cleanup, plus existing folder-import/Advanced/drop routing.
Baseline `1ee870f` plus worktree. Physical mouse and multi-DPI acceptance remain
separate; no live storage or smooth-frame guarantee follows from fixture timings.

Files viewport-status delivery (2026-10-05): status cells now use one Qt delegate
instead of one QLabel/widget layout per row. Display/accessibility roles and
full-text tooltips remain semantic; native selection/focus backgrounds, lock and
pending/error markers are preserved. Qt accessibility-table API regression reads
the complete status after an update. This is API evidence, not a physical screen
reader acceptance pass.

Native Windows synthetic timing: 2,000-item construction 90 ms; ten single-file
updates median 8 ms/max 9 ms (`qt-file-delegate-windows.txt`). The additional
10,000-item run measured construction 527 ms and update median 43 ms/max 49 ms
(`qt-file-delegate-10000.txt`). These exclude network/storage I/O and completed
frame latency; the latter run overlapped the offscreen regression process.
Full Qt suite 58 passed/0 failed (`qt-file-delegate-full.txt`); focused Windows
7 passed/0 failed, plus large-catalog run 3 passed/0 failed including setup and
cleanup. Isolated GUI/test build passed; native list/upload screenshots reviewed.
Baseline `1ee870f` plus worktree.

The previous per-row-widget bottleneck is removed. Remaining performance work is
10,000-item snapshot/coalescing cost, completed-frame and scroll measurement.
Next product slice: incremental Mail rows and stable draft/message ID handoff;
retain acknowledgement/draft recovery/drag behavior while changing presentation.


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

Batch 1 — R0 / R1 delivery slice (2026-10-06):
- R0 Acceptance baseline: reproducible native Windows build containing all 63 test
  slots.
- F1 Search work and scope: dynamic search placeholder/tooltip updates indicating
  current view scope (Files view vs Mail/global); search index rebuild debounced
  (150 ms) to avoid churn on rapid data changes, with immediate synchronous build
  on initial empty model and instant clearing on vault lock; completion model bounded
  to 150 mail items and 150 file items.
- F3 Refresh churn elimination:
  - WalletPage activity rows are retained by ID in `m_activity_widgets` with in-place
    label updates; disconnected unrelated `mailChanged` and `filesChanged` signals.
  - NetworkAuthorityPage permanent section rows (totals, chain) initialized once;
    2-second age ticker (`updateAgeLabel`) decoupled from full layout refresh;
    signatures prevent layout churn when candidate queue, recent ops, or peers
    are unchanged.
- F6 Relative time localization: `relTime()` wrapped in `QCoreApplication::translate`
  with French translations added to `cybou_fr.ts` and compiled resource `.qm`.
- Automated test coverage: `searchScopeAndIncrementalIndex`,
  `walletAndAuthorityPreserveRowsWithoutChurn`, `relativeTimeLocalization`.
- Full native Qt test suite: 63 passed, 0 failed in 33.3s. Running desktop and nodes
  preserved without interruption.

Batch 2 — R2 / R3 delivery slice (2026-10-06):
- R2 (F2 Context preservation on reloadAppearance / theme / language change):
  - `EmailPage`: exposed `searchText()`, `currentMessageId()`, and `isDetailOpen()`.
  - `StoragePage`: exposed `searchText()`, `isDetailsVisible()`, `isDetailsAdvanced()`, `setDetailsAdvanced(bool)`, and overloaded `showDetails(id, advanced)`.
  - `CybouMainWindow::reloadAppearance()` now snapshots and restores:
    - Active page.
    - Global search query text.
    - Mail state: active view (folder), search query, draft message or open reader message ID.
    - Files state: active view, current folder ID, search query, list/grid mode, open details item ID, and Advanced disclosure state.
- R3 (F4 Protection explanations & offline download gating; F5 Responsive layout):
  - F4: Enabled `Download` action in file details and context menu whenever `item->available_offline == true`, even if network replication is still progressing (`state == CybouContentState::Securing`), permitting immediate export of verified local copies.
  - F4: Clarified network replication progress in file details: when `min_remote_replicas < remote_replica_target`, explicitly displays `"Replicating to network"` (`"Réplication réseau en cours"` in French localization) next to replica count.
  - F5: Adjusted responsive column collapsing thresholds (`ModifiedColumn` hidden below 560px, `SizeColumn` preserved down to 400px instead of former 750px/610px limits) and dynamic details panel width (250px–300px), ensuring the `Size` column remains visible at 1040×720 window size with details open.
- Localization: Added French translations for `"Replicating to network"` in `cybou_fr.ts` and compiled resource `cybou_fr.qm`.
- Automated test coverage:
  - `appearanceSwitchPreservesFullContext`: validates state preservation across `reloadAppearance()` for Mail folder, search, reader open message, and Files folder, search, grid mode, details pane, and Advanced disclosure state.
  - `filesProtectionAndOfflineDownload`: validates offline file download button enablement during Securing state, replication progress notice, and responsive Size column preservation at 1040×720 with details pane open.
- Full native Qt test suite: 65 passed, 0 failed in 22.5s. All running processes (desktop `cybou.exe`, background nodes) preserved without interruption.

Batch 3 — R4 / R5 delivery slice (2026-10-06):
- R4 (F6 Wallet forecast, transfer review, and copy clarity):
  - In `WalletPage`, clarified `m_system_hint` text: replaced deceptive "remaining operations" claim with explicit fee-only estimate (`tr("Pays network fees · ~%1 standard fees (excludes storage rent)")`).
  - In `WalletPage`, clarified `m_review` text for transfers: distinguishes Available Balance source, System Balance network fee, and finality of payments (`tr("<b>Send %1 to %2</b><br>Paid from Available Balance · Network service fee: %3 from System Balance<br>Payments are final and cannot be reversed.")`).
- R5 (F7 Assurance copy & protocol jargon elimination; F5 Restore responsiveness & keyboard flow):
  - In `OnboardingView`, eliminated protocol jargon: replaced welcome button `"Restore from mnemonic"` with `"Restore with recovery phrase"`.
  - In `OnboardingView`, scoped cryptographic claims: `"Post-quantum protected"` -> `tr("Hybrid post-quantum encryption")`.
  - In `HomePage`, scoped cryptographic claims: `"Mail is end-to-end encrypted and post-quantum protected."` -> `tr("Mail is end-to-end encrypted using hybrid post-quantum cryptography.")`.
  - In `MailReader`, scoped security info: `"Protected end to end • Post-quantum protected • Network confirmed"` -> `tr("End-to-end encrypted • Hybrid post-quantum • Network confirmed")`.
  - In `IdentityPage`, scoped assurance copy: `"Post-quantum protection: Active"` -> `tr("Post-quantum encryption: Hybrid ML-KEM active")`; `"Recovery phrase: Secured"` -> `tr("Configured in vault")`; `"Ed25519 + ML-DSA-65 · secured"` -> `tr("Ed25519 + ML-DSA-65 · configured")`.
  - In `StoragePage`, removed internal protocol jargon `"recovery capsules"` from encryption details: updated to `tr("Encrypted before sending · Recoverable with your account recovery phrase")`.
  - In `OnboardingView` (F5), tightened `CenteredCard` margins and word grid vertical spacing (4px / 6px) to fit 720px window heights comfortably. Connected `returnPressed` on recovery word 23 to focus `m_restore_password`, on `m_restore_password` to focus `m_restore_confirm`, and on `m_restore_confirm` to trigger `submitRestore()`.
- Localization:
  - Synchronized `src/qt/translations/cybou_fr.ts` with `lupdate`.
  - Added French translations for all new and modified strings.
  - Recompiled `src/qt/translations/cybou_fr.qm` with `lrelease`.
- Automated test coverage:
  - `walletForecastAndTransferReview`: verifies fee-only wording in system hint (excluding storage rent) and explicit breakdown in transfer review.
  - `assuranceAndRestoreResponsiveness`: verifies welcome button wording without protocol jargon, Return key keyboard navigation through recovery words into password fields, and removal of "recovery capsules" in file details.
- Full native Qt test suite: 67 passed, 0 failed in 27.1s. All running processes (desktop `cybou.exe`, background nodes) preserved without interruption.

Batch 4 — R6 / W5 delivery slice (2026-10-06):
- R6 (F8 User Network page and schematic France map):
  - Created dedicated `NetworkPage` (`src/qt/pages/networkpage.h` / `.cpp`) with stable shell navigation:
    - Added `CybouPage::Network` to `CybouPage` enum and sidebar navigation with `NavIcon::Network`.
    - Integrated top metric summary cards: Connectivity status, verified PoA tip height, observed direct mesh peers count, local storage capacity (`V`) and provider obligations, own encrypted publication protection aggregates (`protected` vs `securing` items).
    - Scope, provenance, and honest disclaimer banner: explicitly discloses local node observation source, active sample count, and disclaimer: `"Schematic illustrative map for observed peer connections. Locations are schematic illustrations, not physical node geolocation or network-wide census."`
  - Interactive Schematic France Map (`SchematicFranceMap`):
    - Vector QPainter drawing of Metropolitan France hexagonal geometry and Corsica island, rendered using canonical palette tokens (`SURFACE`, `BORDER_MEDIUM`, `BRAND_TEAL`, `MINT_SOFT`).
    - 12 deterministic regional anchors across France (Île-de-France, Hauts-de-France, Grand Est, Auvergne-Rhône-Alpes, PACA, Occitanie, Nouvelle-Aquitaine, Bretagne, Pays de la Loire, Normandie, Centre-Val de Loire, Bourgogne-Franche-Comté) mapping public peers deterministically by endpoint hash to prevent jitter across refreshes.
    - Explicit dedicated inset container for LAN / Local network peers (`127.0.0.1`, `::1`, RFC 1918 `10.x`, `192.168.x`, `172.16-31.x`, link-local, IPv6 ULA), isolating private addresses from public geography without synthesizing fake city coordinates.
    - Interactive marker selection with visual highlight halo and two-way synchronization with the peer table.
  - Accessible Observed Peer List:
    - Full keyboard-navigable `QTableWidget` (Endpoint, Classification, Advertised Height, Lag, StorageId).
    - Synchronized selection: table row selection highlights map node and updates details drawer; map click selects corresponding table row.
    - Honest empty states when offline or zero peers observed (`"No peer connections observed"`).
  - Selected Peer Details drawer:
    - Displays full un-truncated StorageId, endpoint, classification (France schematic vs LAN), advertised height announcement, lag relative to local verified tip, and TLS pinned transport observation note.
  - Zero Protocol Jargon & Compliance:
    - Cleaned up storage summary labels: replaced forbidden protocol term `"Provider held"` with user-facing `"Held for others"`.
  - Localization:
    - Full French translations for `NetworkPage` and `SchematicFranceMap` in `cybou_fr.ts` and compiled resource `cybou_fr.qm`.
  - Automated test coverage:
    - Added test slot `networkPageAndSchematicFranceMap` verifying page navigation, metric cards, France vs LAN peer classification, selection synchronization between map and table, full StorageId inspection in details drawer, and honest empty/offline states.
    - Updated `mainWindowStarts`, `darkAppearanceResolvesTokens`, and `languageSwitchRebuildsShell` to accommodate the 9-page shell layout.
- Full native Qt test suite: 68 passed, 0 failed in 27.4s. All running processes (desktop `cybou.exe`, background nodes) preserved without interruption.

Batch 5 — R7 / W6 delivery slice (2026-10-06):
- R7 (F8 Central Authority explorer and operational workspace):
  - Paginated Verified Explorer (`src/qt/pages/networkauthoritypage.h` / `.cpp`):
    - Added in-memory paginated index (10 items/page) of candidate operations and locally verified blocks/operations.
    - Integrated search filter input (`authorityExplorerFilter`) with realtime debounce, filtering by OperationID, block height, classification, or status.
    - Paginated navigation controls: `Previous`, `Next`, and item count label (`Page X of Y (N items)`).
    - Responsive table (`authorityExplorerTable`) with columns: Phase / Height, Identifier (monospace ShortHex), Classification, and Status.
    - Interactive details drawer (`authorityExplorerDetail`): selection presents full un-truncated OperationID / BlockID (selectable for copying), verification status, height, and cryptographical state-root verification notes without leaking plaintext or guessing foreign ownership.
    - Bounded execution invariant: operates strictly on local in-memory diagnostic snapshot (`NodeDiagnosticsSnapshot`), avoiding GUI-thread full-history scans.
  - Separation of Concerns:
    - Explicitly separated Canonical state root commitments (totals committed by latest finalized state root), volatile Candidate operations pool, and Off-chain evidence & signer safety.
  - Honest Idle Chain Reassurance:
    - Replaced generic empty queue note with explicit idle chain copy: `"Idle chain: Pool is empty (0 candidates). Blocks are produced on demand as operations arrive, not on an idle empty-block timer."` Prevents mistaking normal demand-driven block intervals for network outages.
  - Off-Chain Evidence & Signer Safety Cards:
    - Added safety journal verification row: `"Fail-closed durable append-only journal active. Equivocation conflicts resolved by min(BlockID)."`
    - Added storage escrow settlement readiness row: `"Off-chain replica service receipts and audit confirmations tracked. Settlement transactions execute upon period close."`
  - Public Read-Only Guard:
    - Restricted operator controls (`Pause`/`Resume`, `Finalize one block`, `Settle storage period`) to proven, authorized PoA key sessions (`a.proven && a.signer_enabled`).
    - Added dynamic console mode notice: displays `"Authorized signer: Genesis-authorized PoA signing key is active. Finalization and settlement controls are enabled."` when unlocked, and `"Read-only console: Active signing key is not unlocked or authorized for this network. Finalization and settlement actions are restricted."` otherwise.
  - Localization:
    - Added full French translations for all new explorer, safety, and evidence strings in `cybou_fr.ts` and compiled resource `cybou_fr.qm`.
  - Automated test coverage:
    - Added test slot `authorityExplorerAndEvidenceWorkspace` in `src/qt/test/cyboushelltests.cpp`: validates explorer item indexing (candidates, verified ops, block tip), pagination forward/backward, selection revealing un-truncated identifiers, filtering, off-chain evidence presence, idle chain reassurance, and read-only console button gating.
- Full native Qt test suite: 69 passed, 0 failed in 28.0s. All running processes (desktop `cybou.exe`, background nodes) preserved without interruption.

Batch 6 — R8 / W7 delivery slice (2026-10-06):
- R8 (F8 Own-content inspector and bounded console):
  - Own-file publication and chunk-tree inspection:
    - In `StoragePage` (`src/qt/pages/storagepage.cpp`), enhanced the `Advanced` details section for own files:
      - Chunk count computation (`ceil(logical_size / 512 KiB)`).
      - Integrity evidence disclosure (`"Content-addressed BLAKE3 Merkle tree. Each chunk verified on retrieval."`).
      - Authorized reference note (`"Recoverable via owner self-capsule. Authorized by finalized RootPublication."`).
      - Added `"Inspect chunk tree"` button (`inspectChunkTreeButton`), directly opening `CybouConsoleDialog` and pre-filling the chunk tree analysis for the selected item.
  - Restricted Bounded Diagnostic Console (`CybouConsoleDialog`, `src/qt/cybouconsoledialog.h` / `.cpp`):
    - Added dedicated dialog accessible from `DiagnosticsPage` via `"Open Read-Only Console"` button (`readOnlyConsoleButton`) and file inspection links.
    - Strictly bounded read-only command set: `help`, `status`, `storage`, `files [filter]`, `file <id|name>`, `chunks <id|name>`, `peers`, `jobs`, `clear`.
    - `chunks <id|name>`: decomposes file into 512 KiB chunk boundaries with exact byte ranges and BLAKE3 verification status.
    - Safe output limits: bounded to 500 lines (`kMaxLines`) with automatic truncation of older lines to prevent UI freezes or unbounded memory growth.
    - Interactive command history with Up/Down arrow navigation.
    - Strict security invariants:
      - ZERO shell execution (`rm`, `sh`, etc. strictly rejected).
      - ZERO arbitrary SQL or scripting execution (`exec`, `select`, `drop`, etc. rejected).
      - ZERO mutation or signing commands.
      - Queries strictly confined to the currently unlocked Identity session.
      - Instant zeroing of output and session history upon vault lock (`identity_state != Active`), displaying `"Vault locked. Private session history and output cleared."`.
  - Localization:
    - Updated `src/qt/translations/cybou_fr.ts` and compiled resource `cybou_fr.qm` with full French translations for console, chunk inspection, and integrity evidence.
  - Automated test coverage:
    - Added test slot `ownContentInspectorAndBoundedConsole` in `src/qt/test/cyboushelltests.cpp`: validates chunk count, BLAKE3 integrity evidence, authorized reference in `StoragePage`, Diagnostics page console launch button, console commands (`help`, `status`, `storage`, `files`, `file`, `chunks`, `peers`, `jobs`), command restriction against shell/SQL attempts, and instant session history wipe on vault lock.
- Full native Qt test suite: 70 passed, 0 failed in 38.5s. All running processes (desktop `cybou.exe`, background nodes) preserved without interruption.

Batch 7 — R9 / W8 delivery slice (2026-10-06):
- R9 (F4, F7 Assurance, Deletion Lifecycle, Recovery Guidance, and Final Beta Gates):
  - Per-Object 4 Assurance Pillars in `StoragePage` Advanced file details:
    - Confidentiality assurance: `"Hybrid post-quantum encryption before upload (ML-KEM-768 + X25519). Plaintext, filenames, and folder structures are never transmitted to providers or network."`
    - Integrity assurance: `"Content-addressed BLAKE3 Merkle tree. Each chunk verified on retrieval against authorized RootPublication commitment."`
    - Availability & durability scope: `"Measured copies: %1 of %2 target. Replica deduplication is by StorageId; does not prove independent physical host failure domains."`
    - Recovery assurance: `"Recoverable on any node using your account recovery phrase via owner self-capsule. Historical capsules preserved across rotation."`
  - Explicit 5-Phase Deletion Lifecycle in Trash & Delete Forever dialog:
    - Added dedicated `trashLifecycleCard` (`QFrame`) displayed in file details when viewing trashed items, breaking down deletion into 5 distinct phases:
      - Phase 1: Local catalog removal (immediate).
      - Phase 2: Finalized publication revocation (stops new admissions on-chain).
      - Phase 3: Storage lease closure (after billing period close).
      - Phase 4: Provider chunk purge (remote acknowledgements unconfirmed/absent; network cannot prove erasure of uncooperative or offline copies).
      - Phase 5: Retained copies (downloaded, shared recipient, or external backups remain unaffected).
    - Enhanced `Delete forever` confirmation dialog (`QMessageBox::question`) with this exact 5-phase breakdown.
  - Honest Recovery Guidance & Scoped Security Claims in `IdentityPage`:
    - Added explanatory guidance under Recovery: `"Your 24-word recovery phrase restores this Identity on another device. Phrase presence in this vault is not a substitute for a tested restore."`
    - Added scoped description under Security: `"Hybrid ML-KEM-768 with X25519 for messaging and storage capsules. Quantum-resistant against future decrypt-later attacks."`
  - Scoped Session Status in `HomePage`:
    - Replaced generic `"Protected"` pill on Identity hero with honest session state `"Active"` (and `"Needs attention"` on sync error), preventing misleading impressions of whole-system or device-level certification.
  - Zero Unqualified Compliance Claims:
    - Zero claims of universal RGPD/GDPR conformity, residency, or crypto-erasure anywhere in UI.
  - Localization:
    - Updated `src/qt/translations/cybou_fr.ts` and compiled resource `cybou_fr.qm` with full French translations for assurance pillars, 5-phase deletion lifecycle, and recovery/security guidance.
  - Automated test coverage:
    - Added test slot `assuranceLifecycleAndRecoveryGates` in `src/qt/test/cyboushelltests.cpp`: validates Active status pill on Home, recovery/PQ guidance on Identity, 4 assurance pillars in StoragePage Advanced details, 5-phase Deletion Lifecycle card on trashed items, and action buttons (`fileRestore`, `fileDeleteForever`).
- Full native Qt test suite: 71 passed, 0 failed in 28.9s. All running processes (desktop `cybou.exe`, background nodes) preserved without interruption.




Files observation follow-up (2026-10-08): the earlier Batch 4 wording
"Replicating to network" is superseded by "Remote copy target not reached".
Replica deficit alone does not establish active repair. Details now report
session-local placement/full-audit/partial-audit attempt time and typed reasons;
reads do not renew freshness, restart makes time unknown, and old observations
do not revoke verified local access. Remaining work includes a common Mail/Files
task view and live outage/recovery acceptance; see the dated implementation
status and scoped evidence in docs 83 and 85.

Shared task view follow-up (2026-10-08): Mail and acknowledged Files mutations
now share the existing header Activity panel and one correlated application task
journal. Saved local intent, finalized publication progress and remote durability
remain separate. The panel retains stable rows, focus and scroll, explicitly
limits display to 100 tasks, and clears private cached labels on lock. The journal
is volatile; durable application ownership remains unchanged. Next delivery work
is large-Mail presentation profiling and the remaining physical/live acceptance
gates, rather than another task API or a generic mutation replay mechanism.

Mail viewport follow-up (2026-10-08): measured 2,000/10,000-message construction,
completed Qt viewport rendering, ten scrolls and ten one-message updates before
and after replacing per-message widget trees with one delegate. Styled native
Windows results and their scope are recorded in implementation status. Stable
selection, anchor, search, IDs, handoff and semantic accessibility remain; short
metadata clipping found during visual review was corrected. Large-mailbox
presentation profiling/delegate work is delivered at component scope. Remaining
gates include physical input/mixed-monitor DPI, screen-reader use, broader
FR/EN acceptance and clean-machine/live outage/recovery scenarios. Independent
remote failure-domain evidence remains separate from UI rendering performance.

Files keyboard scope follow-up (2026-10-08): F2 and Delete now belong to the
list/grid instead of the whole page, protecting retained selections when
navigation or another control has focus. Trash excludes these selection
shortcuts. Native Windows FR/EN coverage verifies rename/Trash acknowledgment,
failed-save retention, search text editing and Mail search/compose shortcuts.
The test dismisses completion before returning focus to files and distinguishes
background projection refresh from file mutation commands. This closes the
scoped shortcut-routing defect; physical keyboard/tab order, focus visuals,
screen readers and wider FR/EN/live acceptance remain separate gates.

Tab/focus follow-up (2026-10-08): component regression now walks complete Tab and
Shift+Tab cycles through Compose fields/dynamic attachment removal, Reader
buttons and Files list/grid actions in FR/EN and light/dark themes. Send and
Security details receive explicit visible focus borders because their local
styles suppressed the common focus treatment; reserved border space avoids
movement on focus. Native focused-control and panel captures support visual
review. Physical keyboard, other pages/dialogs, screen readers, mixed-monitor
DPI and live recovery/outage gates remain open.

Keyboard menu/dialog follow-up (2026-10-08): Mail and Files anchor keyboard
context requests to the current item, independently of pointer position. Mail
retains multi-selection; Files promotes current items outside retained selection.
FR/EN component coverage exercises menu navigation/activation, Compose Space and
picker cancellation, Rename cancellation, acknowledged Move with injected failure
and focus return. Move/picker controls receive explicit accessible names using
existing translated strings. Synthetic nested input settles QWidget activation;
ordinary Escape behavior remains unchanged. Physical Menu/Shift+F10 and native
OS file dialogs remain separate acceptance gates.
