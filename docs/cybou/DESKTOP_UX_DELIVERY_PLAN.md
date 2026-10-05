# Desktop UI/UX delivery plan

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

W2/W3 partial delivery: identical semantic snapshots suppress signals, Home
skips unchanged activity presentation rebuilds, Files Advanced/scroll survive
details refresh, active-job copy counts are filled, unknown counts are explicit,
folder destinations use IDs/breadcrumbs, Files-to-Mail gates explain protection
and downloads commit atomically without deleting a previous destination first.
Mail row children now pass mouse input to the drag/select viewport. These changes
do not complete native drag acceptance, all incremental row work, ID replacement,
evidence freshness, folder enumeration or W5–W8 extensions. Validation results
are recorded in `26_IMPLEMENTATION_STATUS.md` after the test run.

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
