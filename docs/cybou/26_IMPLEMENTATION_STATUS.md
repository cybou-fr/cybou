# Implementation status

Status: code/evidence reviewed on 2026-10-04 with dated 2026-10-05 runtime and
desktop updates below; deployment statements retain their stated scope.

## Verified operation observation windows (2026-10-08)

Successful runtime block commits record operation counts in a fixed one-second
ring supporting 1/5/15-minute windows. Local production and directly accepted
announcements count in the observed series; batch/default history imports stay
separate. Duplicate/rejected commits never increment; replacing a canonical
height resets windows/totals. Zero-operation blocks are not transaction activity.
Collection uses the existing chain mutex with no history scan or GUI dependency.

Network Advanced shows observed finalized op/min over a complete minute.
Console `metrics` shows all windows, local-produced contribution, history counts
and totals since observation reset. Startup/reset windows are Unknown; complete
local idle windows may be zero. No creation timestamp exists in blocks, so old
announcements can arrive later. Observation speed is not a measurement of current
global production, global freshness or a throughput ceiling. The stale timestamp
check claim in `POA_FINALITY.md` is corrected to the actual block layout.
History charts, resource metrics and remote aggregation remain open.

Ordinary MinGW desktop, core and Qt test binaries rebuild successfully.
Runtime suite: 20 cases / 280 assertions passed, including window boundaries,
source separation, duplicate commit, actual local production and canonical
replacement behavior. Full P2P suite: 37 cases / 1,178 assertions passed.
Remaining core cases are filtered; no full-core-suite or live throughput claim.
Logs: `artifacts/network-finalization-20261008/`.

Full offscreen Qt: 92 passed, 0 failed/skipped. Coverage includes the observed
op/min tile, Unknown on missing/uninitialized samples, incomplete longer windows,
separate history counts and EN/FR `metrics` output. Manifest/diff checks pass.
Desktop SHA-256: `9e7529e864f849f48bdbe31e3418b268ff7b9e6ab783110574ab392c2b2e455f`.
No desktop/signer or VPS service is restarted; these are component observations.

## Passive local traffic observation (2026-10-08)

The shared runtime meter counts actual CYBOU frame-stream bytes at successful
TLS application read/write progress. Inbound sessions, mesh outbound and pooled
storage connections use the same counter. Partial/retried/service/invalid-input
bytes count as traffic; TLS record/handshake and TCP overhead do not. It does not
measure unique delivered content, finalized operations or network-wide traffic.

One fixed 61-slot ring and a short mutex provide the preceding 60 complete
seconds, excluding the current partial second. Startup rates remain Unknown;
complete idle intervals yield measured zero. Lifetime totals survive connection
churn, reset on runtime restart, and are not reset by diagnostic reads. A delayed
writer cannot overwrite a newer ring interval. Collection has no GUI dependency,
per-peer labels, chain scan, new wire message or storage proof request.

Network Advanced Overview displays the shared received/sent B/s observation;
read-only `metrics` also exposes exact lifetime totals and UTC sample time.
FR/EN translations are included. Resource measurements, operation rates,
PUT/GET payload breakdown, charts and cohort consolidation remain open.

Ordinary MinGW desktop/core/Qt binaries rebuild. The passive window case passes
14 assertions (startup, complete-window boundaries, partial-second exclusion,
expiry, idle, restart, concurrent writers and delayed-writer ring collision).
The full P2P peer-manager suite passes 37 cases / 1,178 assertions, including
actual TLS handshake/ping traffic observed through the runtime meter. Other
core cases are filtered; this is component transport evidence, not DEVNET load.
Logs: `artifacts/network-traffic-20261008/`. Desktop SHA-256:
`a06e7d50bdd8a4c95080dd803d363b2d144465a3c5ecb10aabf582f941648277`.

Full offscreen Qt: 92 passed, 0 failed/skipped; shared Network rates, missing
observation reset and EN/FR `metrics` totals/rates are covered. Manifest/diff
checks pass. No running desktop/signer or VPS service is restarted or deployed.

## Network naming and observability scope correction (2026-10-08)

### Local passive observation follow-up

Runtime diagnostics now collect real UTC observation time, monotonic uptime and
actual candidate-pool count/serialized bytes under the existing chain lock.
This separates pending load from bounded recent finalized/rejected history.
Existing background collection continues when Network is hidden. Advanced
Overview presents the shared uptime/sample/queue values and read-only `health`
reports them in Console without requiring an unlocked Identity or local signer.
Absent sample/uninitialized pool stays Unknown; FR translations are supplied.
No chain scan, new P2P report, network total, measured op/min, resource-use
estimate or history/chart collector is introduced. O1 remains partially open.

Ordinary MinGW/Qt desktop, Qt tests and core-test binaries rebuilt successfully
in `build_cybou_qt_mingw/bin/`. Full offscreen Qt: 92 passed, 0 failed/skipped,
including sample absence/uninitialized reset, real queue values, French `health`
and locked-session access. Focused core: 3 cases / 35 assertions passed; the
remaining cases were filtered. Coverage checks empty/queued/drained pool bytes,
monotonic uptime and unchanged status-log suppression. Logs are retained under
`artifacts/network-local-observation-20261008/`. Desktop SHA-256:
`a369b2b72d33d7cd7a7a78898647cf1ca202f17137f309f4339729469b89f37b`.
These are component/build results; no deployed network aggregate or physical
acceptance is claimed. Existing desktop/signer processes were not restarted.

### Earlier naming/contract package

Navigation/header now use Network / Réseau; the map heading uses NETWORK /
RÉSEAU. Active DEVNET and France admission remain scoped details. Built-in
collection/consolidation of actual service bytes, finalized op/min, transfer
rates, resource load and capacity is required by `NETWORK_OBSERVABILITY_PLAN.md`.
The desktop plan no longer substitutes a benchmark-first sequence for passive
monitoring. Existing diagnostics expose local storage/session/finality, not a
remote resource feed. No passive collector, cohort totals, new Console command
or capacity estimate is claimed implemented by this contract/naming package.

Isolated MinGW desktop and Qt-test builds pass, including regenerated embedded
French translations. Five existing navigation/language/Network scenarios pass
offscreen (7 results including setup/cleanup, 0 failed/skipped). Manifest and
diff checks pass. Logs and binaries: `artifacts/network-monitoring-contract-20261008/`.
This is naming/component evidence, not a telemetry or live network acceptance
result. The running desktop and signer are not restarted.

## Core CI signed-size build repair (2026-10-08)

Authenticated log for GitHub core job `113302547455` (run `37774708866`,
source `1f5417fb`) identifies `-Werror=sign-compare` in POSIX
`ReadSecretFile`: signed `stat::st_size` was compared with unsigned `size_t`.
Linux `-Wall -Wextra -Werror` syntax checking reproduces the failure. C++20
`std::cmp_greater` now compares the integers without narrowing or suppressing
warnings. Positive-size, one-MiB policy, private ownership/permissions, regular
file and link rejection remain in force. The existing regression adds too-small,
zero and over-policy size limits while retaining the exact-limit successful read.

A fresh isolated WSL Ubuntu 26.04/GCC 15.2 headless build uses CI's
RelWithDebInfo `-O2 -g0`, BUILD_TESTS, test hooks and WARNINGS_AS_ERRORS settings;
all five CI targets build. It uses local OpenSSL/BLAKE3 dependencies, not the
Ubuntu 24.04/GCC 13.3 CI image: this is local strict-build evidence. The full
core suite passes 266 cases / 98,757 assertions. Official DEVNET help/info/doctor
checks pass; doctor leaves the supplied absent data directory absent.

Isolated MinGW desktop/core/Qt builds pass. Full offscreen Qt: 92 passed,
0 failed/skipped. Three native Windows Qt scenarios: 5 results including
setup/cleanup, all passed. The Windows secret-file case passes 7 assertions
(the remaining 258 cases are intentionally filtered, not a Windows full-suite
claim). Python battle/benchmark/report discovery passes 6 tests. Build cache
output is restored. Logs, original failing CI log and binary/source provenance
are under `artifacts/ci-integrity-20261008/`.

This repairs the diagnosed source failure; green remote core/desktop CI for the
published corrected revision is still required before closing P0. The earlier
desktop jobs were in vcpkg setup at the status check. No running desktop,
signer, VPS, accepted network material or signing history was changed.

## Acceptance planning review (2026-10-08)

Active remaining work is [DESKTOP_BETA_ACCEPTANCE_PLAN.md](DESKTOP_BETA_ACCEPTANCE_PLAN.md),
reviewed against `1f5417fb6d6c4d418b474b856a23bdb21486c2d6`. Public GitHub Actions
API confirms [core run 37774708866](https://github.com/cybou-fr/cybou/actions/runs/37774708866)
failed at binary build; protocol/CLI steps were skipped. Compiler cause remains
unverified. [Desktop run 37774708869](https://github.com/cybou-fr/cybou/actions/runs/37774708869)
was in progress at review. Earlier local core passes and the current 92 Qt/6
native Windows results retain their exact package scope; they do not establish
green remote core CI. No CI fix or new live acceptance is claimed here.

The implementation-first backlog is superseded by revision integrity, prepared
independent topology, live recovery/outage/operator acceptance and physical/
assistive-technology checks. Sustained capacity is P1. Files profiling does not
close completed-frame/scroll/live acceptance; Mail's component optimization
avoids an unsupported further architecture rewrite. Historical same-host
benchmark remains a scoped op/min reference, never capacity. No network,
provisioning, signer or deployment change is authorized by this planning update.

## Recovery/Console keyboard package (2026-10-08)

Recovery phrase replacement explicitly defaults to No at its first confirmation.
The recovery word display and two distinct numbered confirmation fields expose
translated accessible names; confirmation labels are associated with their
fields. The existing phrase/password gate and confirmation checks remain in use.

Console allows ordinary Tab traversal from an empty command input. Tab still
opens/accepts suggestions for nonempty commands, and Ctrl+Space remains available
on an empty input. Command and output fields expose translated names; output
reserves a contrasting focus border.

FR/EN and light/dark component coverage uses synthetic words only. It exercises
password/acknowledgment gating and cancellation without revealing a phrase,
Return on the default No before rotation proceeds, wrong/correct confirmation
words, Escape and text cleanup in the rotation phrase dialog. Console coverage
checks empty-input Tab/Shift+Tab, command execution and Up/Down history, Ctrl+F,
search Escape without closing, Ctrl+L and final Escape. This does not establish
clean-machine restore, real key rotation/finality, physical keyboard,
screen-reader or mixed-monitor acceptance.

Validation: isolated GUI/test builds succeeded; full offscreen Qt passed
92 results and four focused native Windows scenarios passed 6 results including
setup/cleanup, with no failures/skips. The initial native test sent focus before
page event processing; settling the page before activation fixed the test.
Evidence and source/binary provenance: `artifacts/ux-recovery-console-keyboard-20261008/`.
The build cache is restored; no live vault, signer or VPS was changed/restarted.

## Identity/Wallet/Network keyboard package (2026-10-08)

Wallet activity rows and folded fee groups now participate in Tab navigation,
expose translated semantic names/descriptions and activate on Enter or Space.
Keyboard expansion focuses the first individual fee; existing row retention and
command paths remain in use. Rows reserve a two-pixel focus border. The Network
map now repaints an explicit contrasting border on focus changes. Identity's
recovery password and Wallet's Network balance amount expose explicit names.

Component coverage walks complete forward/reverse Tab cycles through enabled
Identity, Wallet and Network actions in FR/EN and light/dark themes. It checks
map arrow selection, activity details cancellation and focus return, fee-group
expansion, and Escape after entering a valid Network balance amount without
submitting or changing balances. Synthetic dialog input settles QWidget
activation. Physical keyboard, screen readers, native OS file dialogs,
mixed-monitor DPI and live DEVNET acceptance remain separate gates.

Validation: isolated GUI/test builds succeeded. Full offscreen Qt: 91 passed,
0 failed, 0 skipped. Seven focused native Windows scenarios: 9 passed including
setup/cleanup, 0 failed, 0 skipped. Native focused-row captures in EN/light and
FR/dark and the FR/dark map were visually inspected; borders are visible.
Evidence and source/binary provenance: `artifacts/ux-account-network-keyboard-20261008/`.
The build cache is restored; no live desktop/signer or VPS was restarted.

## Mail/Files keyboard menus and dialogs package (2026-10-08)

Mail previously selected a context-menu row from pointer coordinates even for
a keyboard request. Mail and Files list/grid now handle keyboard context events
at the current item, scroll it into view and anchor the existing menu there.
Mail retains a multiple selection containing that row; Files replaces retained
selection when its current item is outside it. Mouse requests and the existing
command/acknowledgment paths remain unchanged. Move's folder selector and the
CYBOU Files attachment picker now expose explicit accessible names using
existing translated strings.

FR/EN regression delivers keyboard context events with coordinates away from
the current item, navigates menus with Home/Down/Return, cancels Mail's menu and
Rename with Escape, and opens Compose attachments with Space before cancelling
the CYBOU picker. Cancellation retains draft text and issues no Rename. Files
Move uses keyboard folder selection and acceptance; injected save failure
retains the original parent. Focus ownership after closure is checked.

The test uses separate menu/dialog drivers and explicitly settles QWidget
activation before nested synthetic input. It starts each language in Inbox,
because language rebuilding correctly preserves an open Compose and can hide
the list in two-pane mode. Investigation of a cancellation timeout found a
synthetic activation problem; ordinary application Escape behavior is unchanged.
These are Qt event-route checks, not physical OS Menu/Shift+F10 acceptance.

Validation: isolated GUI/test builds succeeded; the final full offscreen Qt
suite passed 90 results and seven focused native Windows scenarios passed nine
results including setup/cleanup, with no failures. Existing protected-file picker,
Mail menu/Undo, shortcut scopes, Tab/focus and compose rebuild regressions passed.
Evidence and source/binary provenance are under `artifacts/ux-menus-20261008/`.
The build cache is restored to its usual output; no live desktop/signer or VPS
was restarted/deployed. Physical keys, native OS file dialogs, screen-reader,
mixed-monitor DPI and wider dialog/live acceptance remain open.

## Mail/Files Tab and visible-focus package (2026-10-08)

Compose Send and Reader Security details used local borderless styles, suppressing
visible border treatment when focused. Both now reserve a transparent two-pixel
border and use the theme's primary text color for an explicit focused border.
The reserved space keeps focus transitions from changing geometry. Command,
publication, storage and Identity behavior are unchanged.

A new component regression walks complete Tab and Shift+Tab cycles through
Compose, Reader and Files list/grid in FR/EN and light/dark themes. It verifies
all currently visible enabled buttons are reachable both ways, icon-only
controls have accessible names, and Subject/Body are in the Compose cycle.
Compose includes a dynamic attachment-removal button; Reader includes received
attachment controls; Files includes the selection toolbar. Native page layout
and activity are settled after page replacement before walking focus. This is
reachability evidence, not a claim that every menu/dialog action was activated.

Validation: isolated GUI/test builds succeeded; the full offscreen Qt suite
passed 89 results and seven focused native Windows scenarios passed nine results
including setup/cleanup. Native control/panel captures of Send, Security details
and selection Trash were saved in all four language/theme combinations. Light
English and dark French Send/Reader panel captures were visually reviewed, with
clear focused borders; the dark Files Trash control was also inspected.
Native layout testing reported a Windows maximum-size clamp for the largest
requested geometry, so that run does not establish physical 1920x1080 acceptance.
Evidence and final source/binary provenance are under
`artifacts/ux-focus-20261008/`. The build cache is restored to its usual output;
no live desktop/signer or VPS was restarted/deployed. Other pages/dialogs,
physical keyboard, screen-reader and mixed-monitor DPI acceptance remain open.

## Mail/Files keyboard scope package (2026-10-08)

Files selection shortcuts were attached to the whole page: retained selection
could let F2/Delete act on files while navigation held keyboard focus. They now
belong to the list/grid with WidgetWithChildrenShortcut scope. Trash rejects
both shortcuts, consistent with its visible Restore/permanent-delete actions.
The acknowledged command and review paths are unchanged.

A new FR/EN regression covers retained selection with navigation focus, header
text deletion, actual list/grid rename and Trash commands, failed-save catalog
retention and the Trash guard. It also covers Mail search/compose shortcuts and
literal compose text containing shortcut letters and Delete. The test dismisses
search completion with Escape, explicitly reactivates the parent after a modal
dialog (required by offscreen Qt), and distinguishes projection/session traffic
from file mutations. Existing async unlock and language/theme compose-retention
scenarios remain covered.

Validation: isolated GUI/test builds succeeded; the full offscreen Qt suite
passed 88 results and five focused native Windows scenarios passed seven
results including setup/cleanup. Both final runs passed without failures.
Evidence and final source/binary provenance are under
`artifacts/ux-keyboard-20261008/`. The build cache is restored to its normal
output directory. No live desktop/signer or VPS was restarted/deployed.
These programmatic events do not close full tab-order/visible-focus, physical
keyboard, screen-reader, mixed-monitor DPI or wider FR/EN/live acceptance gates.

## Mail viewport performance package (2026-10-08)

Profiling confirmed that per-message widgets dominated initial presentation.
Mail now paints visible semantic rows through one QStyledItemDelegate; stable
QListWidgetItem IDs, rank, selection/current/anchor and replacement-ID handoff
remain. The delegate preserves avatar, direction, unread/star/attachment markers,
plain subject/preview, status/date, protection and support-rate explanations.
Accessible item text/description and escaped tooltips replace child-widget-only
labels. A visual review caught short metadata clipping; font source and text
padding were corrected, with light/dark status captures retained. Reader,
compose, encrypted index ownership, delivery and publication semantics are unchanged.

Native Windows component runs with the actual desktop stylesheet initialized,
1040x720 geometry and DPR 1.75 gave these observations (milliseconds):

| Synthetic mailbox | Construction before / after | First completed Qt render before / after | Scroll render median before / after | One-message update + render median before / after |
| --- | ---: | ---: | ---: | ---: |
| 2,000 messages | 972 / 27 | 1,252 / 112 | 9 / 5 | 26 / 13 |
| 10,000 messages | 12,171 / 144 | 17,304 / 219 | 32 / 5 | 117 / 44 |

Scroll/update columns each use ten samples, not long-run percentile estimates.
Separate unstyled native baseline logs are retained, including the earlier
7,440 ms first render at 10,000 messages; they are not mixed with the styled
comparison. The baseline row-widget counts after ten updates include ten
widgets awaiting Qt deferred deletion; final rows create zero mailRow widgets.
A synchronous viewport grab establishes completed Qt rendering, not physical
monitor presentation. These are synthetic local UI observations, excluding
network/storage/index discovery I/O and clean-machine/live acceptance.

Validation: isolated GUI/test builds succeeded; the final full Qt suite passed
87 results, and seven targeted native Windows scenarios passed (nine results
including setup/cleanup). Profiling runs each passed at 2,000 and 10,000 messages.
Regression covers semantic status/date/unread/star/attachment metadata, escaped
tooltips, zero message-row widgets, retained current/selection/anchor/search,
replacement IDs, lock clearing and existing acknowledged Mail/Files core-adapter
flows. Light/dark status captures and a French dark desktop Inbox fixture were
visually inspected. Evidence, baseline source patch/binary provenance, structured
measurements and final source/binary provenance are under
`artifacts/ux-mail-performance-20261008/`. The build cache is restored to its
normal output directory. No live desktop/signer or VPS was restarted/deployed.
Physical drag/mixed-monitor DPI and real screen-reader acceptance remain open.

## Shared Mail/Files task panel package (2026-10-08)

The existing header Activity panel now receives correlated local Files command
progress alongside Mail. Folder creation, rename, move, copy, Trash and Restore
report actual Queued/Running/Committed/Failed worker states; local acknowledgment
is distinct from finalized catalog/publication state and remote protection.
The single application task journal replaces the Mail-only API throughout the
model, compose/reader, Activity and Console jobs. Scope guards keep file commands
from changing Mail handoff state. Existing publication retry remains available;
failed mutations offer their item workflow without automatic intent replay.

The popup scrolls instead of silently stopping at 12 entries. Up to 100 stable
command/item rows are retained across refresh with button focus and scroll;
excess tasks receive an explicit notice. Status and tooltip output are bounded
and use plain text. Completed local saves leave the in-flight list. Terminal
history retains 32 entries without evicting active work, and pruning the final
old failure refreshes the indicator. Lock clears tasks and cached private popup
labels even when hidden; old-session callbacks cannot restore them. File events
no longer force unrelated Mail Reader refresh. FR copy covers new task actions,
shared-panel scope and Console task domains.

Validation: isolated GUI and Qt test builds passed. The final full Qt suite
passed 85 results; six targeted native Windows scenarios passed (eight results
including setup/cleanup), covering the common panel, local Mail/Files commit
and Undo, actual core-adapter Mail/Files flows and layout bounds. The shared-task
regression checks 105 queued tasks, explicit display limits, stable focus/scroll,
plain-text failure labels and item opening, stale/duplicate replies, hidden popup
lock cleanup and removal of the last retained failure. Evidence and binary/source
provenance are in `artifacts/ux-common-tasks-20261008/`. These are local component
and delivered Qt event checks, not physical input, live-network outage or
clean-machine acceptance. No live desktop/signer or VPS was restarted/deployed.
The build cache is restored to its normal output directory.

## Files local protection observations package (2026-10-08)

Files details distinguish saved remote replica counts from the last local
placement/audit attempt. Attempt time, partial/full audit scope and typed reasons
come from StorageService; raw endpoint-bearing errors never enter the semantic
projection. A bounded volatile cache retains up to 1,024 publications and drops
observation time on reopen/eviction without discarding persisted placement counts.
Reading those records does not refresh time or perform a network check.

Old observations receive a descriptive 24-hour age label, not fabricated replica
loss or a protection-state transition. Unknown counts remain unknown in Advanced.
An unmet target no longer advertises active replication. Disabled downloads
explain retrieval in progress or missing local/remote availability; verified local
copies remain downloadable regardless of remote observation age. FR translations
cover scope, reasons and the new details fields. These are local diagnostics, not
canonical reliability, continuous uptime or independent failure-domain evidence.

Validation: isolated GUI/core/Qt builds passed; all 259 core cases passed with
98,013 assertions, all 84 Qt results passed, and four targeted native Windows
scenarios passed (six results including setup/cleanup). Coverage checks read-only
freshness, reopen with unknown time, placement/full/partial scopes, safe reasons,
unknown/zero/one/two copies, age labels and local download eligibility. The first
full Qt run retained an obsolete replication-label expectation; its failing log
and the corrected full run are retained in
`artifacts/ux-files-observations-20261008/`. The build cache is restored to its
normal output directory. No live desktop/signer or VPS was restarted or deployed.

## Protected Files into Compose package (2026-10-08)

Compose now accepts internal Files drag IDs over its surface and text editors,
adding reusable references without a local source path. Duplicate sources are
deduplicated; all dropped references must currently be eligible. Pending,
missing, folders, content inside Trash and closed sessions are refused. Source
eligibility is rechecked on drop; send/close handoff blocks attachment changes.
Internal MIME takes precedence over downloaded URLs, so refusal never becomes
a local re-upload. Local file drops remain supported and dropped batches respect
the current 32-attachment schema bound. FR copy explains reuse and refusal.

Validation: isolated application/test build and the full Qt suite passed 83
tests. Native Windows targeted scenarios passed at Qt scale factors 1.00, 1.25,
1.50 and 2.00. UI regressions cover editor routing, duplicate/mixed payloads,
source changes, pending/Trash/missing items, busy/lock and draft acknowledgment.
The core adapter test persists a reference draft through session closure before
successful delivery and exact-byte download. Source removal before preparation
refuses sending while retaining the acknowledged draft. This is not a new draft
retention guarantee or physical OS/mixed-monitor drag acceptance. The first UI
test run exposed an incorrect test-double projection expectation; its original
log is retained alongside the corrected runs in
`artifacts/ux-compose-drops-20261008/`. No live desktop or signer was restarted.

## Files drop routing and Qt scale validation package (2026-10-08)

Files list/grid, breadcrumbs and navigation now share target validation with
the move command. They refuse missing items, unusable destinations, folder cycles
and closed Identity sessions, and revalidate at drop. Duplicate IDs issue one
command per item. Internal Files MIME takes precedence over downloaded-copy URLs,
so a refused move cannot become a duplicate import. Blank My files drops move
into the current folder; ordinary external local-file import remains available.
Non-local URLs alone do not count as an import.

Validation: isolated app/test build and the full Qt suite passed 82 tests.
Five targeted scenarios ran with the Windows Qt platform at measured device
pixel ratios 1.00, 1.25, 1.50 and 2.00, seven results including setup/cleanup per
profile. They cover Files list/grid targets, local import, mixed MIME refusal,
duplicate/stale IDs, lock between enter/drop, acknowledged Undo, Mail folder
event routing and layout bounds. These are delivered Qt events and configured
Qt scale factors, not physical OS drag-and-drop or mixed-monitor DPI acceptance.
Evidence and binary provenance are in `artifacts/ux-files-drops-20261008/`.
No running desktop/signer, network constants or deployment was changed.

## Acknowledged Files changes and Undo package (2026-10-08)

Folder creation, rename, move, copy, Trash and Restore report whether the
publication intent was saved locally. Failed publication staging retains the
existing item and reports failure; a failed new folder shows Needs attention.
Move/Trash batches give pending feedback, count partial results and offer Undo
only for saved changes. Undo queues an inverse move to the original folder,
including from Trash, before or after the forward projection arrives. Apparent
no-ops are checked against worker state, not a potentially older GUI snapshot.
Session replacement/lock invalidates delayed replies and retained Undo actions.
Acknowledgment does not imply network finalization or confirmed remote replicas.

Validation: isolated MinGW application/test builds and the complete Qt suite
passed 81 tests. Regression coverage exercises delayed/failed saves, partial
results, early Undo, unavailable Files and stale session callbacks. The actual
core adapter test queues a move and its inverse, and Trash and return to the
original folder, before finalization; it verifies both final catalog outcomes.
Native Windows focused tests also cover these two scenarios. FR strings cover
pending/local acknowledgment and new failure states. No live DEVNET signer or
node was restarted, and no network, genesis or deployment configuration changed.

## Acknowledged Mail deletion package (2026-10-08)

Draft discard and permanent mailbox deletion now reuse correlated Mail command
progress and session generation. The adapter removes private projected rows only
after the encrypted Application DB commits deletion. Exceptions or failed writes
retain the failed item and report failure. Permanent batch deletion reports each
committed/failed item; the reader stays open while deletion is pending. A durable
pending send cannot be cancelled by deleting its projected row or backing draft.
This concerns local mailbox state, not deletion of historical/recipient copies.

Compose keeps text while discard is queued/running and re-enables it on failure.
Discard is ordered after any queued autosave; its obsolete editor reply is ignored.
Theme/language rebuild follows an in-flight discard by draft/task identity. Lock,
session replacement and reentrant lock from a row-removal listener invalidate late
completion callbacks. Context-menu draft discard no longer announces success
before the local commit. FR strings cover the new progress/failure states.

Validation: isolated MinGW app/test builds passed and the complete Qt suite passed
80 tests. The added regression injects delayed/failed commits, partial deletion,
autosave/discard ordering, rebuilt composer and stale callbacks. The existing core
adapter integration waits for actual draft and permanent mailbox deletion and
checks absence after lock/unlock. Final native Windows checks passed both the
new regression and core adapter integration (4 results including setup/cleanup),
including the additional reentrant-lock guard. Logs and isolated binaries are under ignored
`artifacts/ux-mail-actions-20261008/`. Clean-machine Beta and physical accessibility
acceptance remain open; Files mutation acknowledgement/Undo is separate remaining
work. The running desktop binary and live PoA state are not replaced.

## Authority and desktop presentation QA package (2026-10-08)

Authority now occupies a separate Administration navigation section visible only
with local genesis-key proof. Operator controls also require an unlocked Identity
and active signer. Pause has a Cancel-default consequences review; resume is
direct. The page and unlock screen explain that locking the user Vault does not
stop an already active background PoA finalizer. That local runtime observation
does not grant permissions or announce a network role.

Storage settlement prepares actual payout entries on the application worker,
then reviews period/UTC interval, unique payout accounts, entry count, amount,
escrow and the first 100 exact entries. Missing evidence is not reported as zero
service. Lock, account/proof loss, halt, changed period or changed escrow closes
the review without submission. Confirmed entries are submitted on a worker after
rechecking unlocked key and runtime cursor. Submission is explicitly pending
PoA finality. Journal status is the available local observation, not an asserted
integrity audit. Explorer separates finalized diagnostics from volatile candidates
and keeps selection by identifier; peer deltas remain unverified announcements.

Validation: isolated MinGW GUI/test builds passed; complete Qt suite 79 passed,
0 failed. The final dialog sizing/keyboard adjustment also passed its focused
regression (3 including setup/cleanup). Native Windows synthetic fixtures were
captured in FR/EN, light/dark, at 1040×720, 1280×860 and 1920×1080, plus dark
1040×720 with Qt scale 1.25: 14 profiles. Contact sheets cover Home, Mail, Files,
Wallet, Identity/recovery, Network/Advanced, Monitor, Console and Authority;
full-size operator reviews and locked-Vault views were inspected. The pass fixed
translated details-button clipping. Updated reviews include expanded payout
entries and pause consequences. Language fixture selection does not persist real
user preferences.

Evidence and isolated binaries: ignored `artifacts/ux-authority-20261008/`.
Fixtures and component checks are presentation evidence, not live settlement,
independent remote durability, clean-machine Beta acceptance or screen-reader
certification. Physical mixed-monitor DPI and assistive-technology acceptance
remain open. This package does not replace the running PoA desktop binary.

## Monitor and Console package (2026-10-08)

Network Monitor now opens one reusable nonmodal window from Advanced, with
Peers, Operations and Own content tabs capped at 256 rows each. Refreshes
coalesce over 150 ms, retain selected object and scroll, and distinguish
unverified peer-ahead announcements. Pause freezes the view only. Own-content
rows clear synchronously on lock/account change even while paused.

Console now has a compact live scope header, icon toolbar and run control,
registry-derived grouped help and bounded command/argument completion. Own file
arguments require an unlocked Identity; Authority suggestions require current
key proof. Completion never executes. Ctrl+F searches output with wrapped
previous/next and match counts; Ctrl+L clears output/history. Private session
cleanup also clears search and completion. Both windows save geometry only;
screenshot fixtures do not read/write those preferences. Console stays read-only:
Authority signing controls and settlement submission remain on the Authority page.

The concurrent refactor at `0dd16099` required build repairs: the publication job
cache uses its actual nested `Job` type; the node builds its own PCH because core
has different offline test definitions; extracted Activity declarations forward
declare semantic types; the screenshot harness includes the actual network and
backend headers. Activity retains its original translation context.

Validation: 259 core cases / 97,796 assertions and 78 Qt tests passed. Native
Windows captures at 1040×720 were inspected in light and dark, including ordinary
and Authority console scopes. Evidence and isolated binaries are under ignored
`artifacts/ux-r6-20261008/`; no live signer or network deployment is required.

## Network reference and Advanced sections package (2026-10-07)

The main Network map now carries a compact accepted-reference card: finalized
op/min, UTC run date, workload/count and historical same-host simulation scope.
Missing/rejected/wrong-network evidence renders Unknown. Details opens the full
existing evidence in Overview, retaining its exact counts/window and provenance.
Opening Network or its reference does not run a benchmark. Spare horizontal map
space keeps Corsica clear of the lower-right card without shrinking/distorting
the silhouette or changing its geometry when Advanced opens.

Advanced now separates Overview, Peers, Storage and Technical into bounded
scrollable tabs. Overview holds local finality/mesh observations and reference;
Peers holds session observations and the one selected-peer card; Storage holds
local capacity and own-content protection, with an explicit mesh/storage evidence
boundary; Technical hosts the existing diagnostics and monitor/console launchers.
The selected peer survives drawer close and reference/technical navigation.
Selecting a peer while Advanced is open chooses Peers. The Identity/desktop
diagnostics entry point opens Technical directly. Unavailable capacity shows
Unknown instead of an apparent zero measurement.

Isolated MinGW GUI/test builds passed. Focused suite: 5/0 including setup/cleanup;
final complete Qt suite: 77 passed/0 failed. Native Windows light 1040×720 and
dark 1280×860 Network, selected-peer, reference and Storage captures were visually
inspected under `artifacts/ux-r5-20261007/`. The captures combine synthetic peer
observations with the genuine compiled historical reference; they do not form a
new live network measurement. No network reset, deployment or signer change was
performed. Professional Monitor/Console/Authority work and complete physical
DPI/accessibility/FR-EN acceptance remain subsequent packages.

## Recovery and Mail/Files interaction package (2026-10-07)

Identity preparation labels local encrypted opening and discovery/decryption as
two stages. Continue in background keeps the worker running and shows a persistent
strip with Recovery details. Progress remains a verified-history scan, capped at
99 until projection readiness, not a claim of downloaded/protected content.
Failures reopen the scoped dialog after background continuation; Ready/lock hide
the strip. Appearance rebuild preserves background mode and its details action.

The shell now exposes one search field on Mail/Files. Typing and clearing feed
their existing current-view filters; switching products restores the respective
query and folder navigation clears the Files query. Ctrl+K and Mail `/` reach the
visible control. Lock clears search and private suggestions. Standalone page
fixtures retain their local control. Appearance rebuild retains the shared query.

Files selection actions now support one/multiple items, list/grid and Trash. Star
toggles the cohort, ordinary views offer Move to Trash, and Trash offers Restore.
Single selection offers Details; Clear selection is always available. Mutation
controls follow active Identity/backend availability. Permanent deletion keeps
its existing explicit review. Shared icon styling follows a dedicated property,
so named controls retain their size, hover and keyboard focus presentation.

Isolated MinGW GUI/test builds passed. Complete Qt suite: 76 passed/0 failed after
the icon style fix; the subsequent background-strip appearance retention change
passed its focused regression (3/0 including setup/cleanup). Native light 1040×720 and dark 1280×860
fixtures are under `artifacts/ux-r4-20261007/`. These are presentation evidence;
no additional signer, deployment or network reset is involved. Broader quiet
success-state review, physical DPI/accessibility and complete FR/EN acceptance
remain. This package did not restart or replace the user's PoA desktop binary.
Final native captures of the two-stage dialog, background recovery strip, single
header Mail search and Files selection toolbar were visually inspected. An early
capture exposed named icon buttons losing the common padding style; the dedicated
icon property correction is included in the final captures and build.

## Desktop benchmark accounting package (2026-10-07)

Loadgen now reports attempted/submitted/finalized operations separately, tracks
unique measured OperationIDs, excludes historical jobs and observes wallet
finalization during drain. The battle report uses one controller measurement
window, validates per-client counters/rates and shows missing latency samples as
Unknown. Network Advanced consumes a bounded compiled reference only after PASS,
successful acceptance checks and matching NetworkBinding; it labels provenance,
workload, measurement window and Windows/WSL simulation scope. Opening the page
does not generate load. French translations accompany the reference display.

The first live run exposed a database missing CURRENT and a missing client vault;
its files and failed verdict are retained. Opening an existing LevelDB without
CURRENT now fails before create_if_missing can replace its metadata. A real-disk
regression verifies that rejection preserves the files, and a later single-client
run preserved vault, CURRENT and NetworkBinding across close/reopen. The cause of
the original file disappearance remains unresolved; automatic repair is absent.
Loadgen also stops its runtime before destroying referenced Identity services.

Core suite: 267 passed; final Qt suite: 75 passed, including the simulation scope
and accepted-reference parser regression; Python report suite: 6 passed.
Build/test logs are under `artifacts/ux-r3-20261007/`. Failed runs
`battle/20261007-195003.md` and `battle/20261007-203645.md` remain evidence: the
first produced no load metrics, and the second finalized six operations but
timed out at one of two required replicas. Their results are not accepted
benchmark references.

Accepted reference: `battle/20261007-205703.md`, current DEVNET, files profile,
one Windows client/Identity, seven 64 KiB files, two-replica target, ordinary WSL
Full Node plus the existing DEVNET peers. All seven operations finalized, load
drained with no failed operations and clean vault-only restore verified 7/7
files after the source client stopped. Controller throughput was 7/88.8 s,
approximately 0.08 finalized op/s; client load/drain throughput was approximately
0.22 op/s. These denominators differ by documented reconnect/controller overhead.
The desktop displays the unrounded reference rate as 4.7 finalized op/min;
the artifact and historical report retain the per-second measurement contract.
This short low-rate workload is not a capacity benchmark. Windows and WSL share
one physical host; two placement addresses do not prove independent remote
machines or failure domains. No proxy, Geo bypass, additional signer or network
reset was used. Its reviewed public `benchmark_reference.json` is compiled into
the isolated desktop build; the user's running PoA desktop is untouched.
Native Windows light 1040×720 and dark 1280×860 reference captures were visually
inspected: the accepted historical number, French workload/window text, binary
hash and same-host simulation caveat are readable. Peer/map snapshots in those
captures are synthetic fixtures, separate from the real reference evidence.

## Desktop Network presentation package (2026-10-07)

The schematic Network map now uses a compiled Natural Earth mainland/Corsica
outline with preserved aspect ratio (284/32 simplified vertices). The generated
header records the GeoJSON SHA-256 and `tools/ui/generate_france_outline.py`
reproduces it offline. No map SDK, runtime geography fetch, city assignment or
regional boundary is introduced. P markers are mesh observations; L markers are
in a bounded compact LAN inset, including a selected peer when the inset overflows.

The normal map shows its Public CYBOU/France scope, local connection/protection
summary and illustrative-position note. One selected-peer card moves between
the map and Advanced without losing selection. Normal rows show admission,
connection and unverified advertised height; endpoint, StorageId and tip delta
appear in Advanced. Floating row height is measured after row widgets become
visible. Closing the icon clears the selection. Disconnected observations retain
no current storage proof or advertised-height claim.

Ahead height announcements and matching heights are explicitly unverified;
neither renders as proof of synchronization. The provenance timestamp is labelled
Last sync, not diagnostics freshness. These changes concern local presentation,
not consensus, peer admission or provider placement.

Native Windows light/dark fixture captures are under `artifacts/ux-r2-20261007/`.
The final MinGW GUI/test build passed and the complete Qt suite passed 74/74
after the advertised-height correction, including selected-card height, drawer
selection retention, ahead-announcement and disconnected-evidence regressions.
Network, floating card and Advanced captures were visually inspected at 1040×720
(light) and 1280×860 (dark); fixture snapshots are not network/benchmark evidence.
Full DPI and FR/EN acceptance remain. Benchmark,
VPS deployment and signer/network changes remain outside this package.

## Desktop UX first implementation package (2026-10-07)

Implemented against `7f043f95`: Compact/Toolbar utility icon buttons with accessible
names and keyboard focus; Home activity refresh and Identity shortcut, Identity
copy/lock and Settings download-folder controls use them. Primary and sensitive
actions keep their text and existing reviews. Home, Wallet, Mail support fees,
activity and wallet errors use Network balance / Solde réseau for the canonical
System Balance. Identity no longer duplicates Wallet balances.

Header search is contextual on Home/Mail/Files; other pages show their title.
Ctrl+K temporarily opens search there without navigation; Escape or focus leaving
the field restores the title. Subsequent single-header Mail/Files integration is
recorded in the package above. Suggestions are scoped to the corresponding product, and Home/global
suggestions still span both local indexes.

Validation: MinGW GUI and Qt test targets built; the complete Qt suite passed
74/74 results. Native Windows fixture captures at 1040×720 (light) and 1280×860
(dark) are under `artifacts/ux-r1-20261007/`; Home, Identity and Wallet captures
were visually inspected. Offscreen captures had missing font glyphs and are not
visual acceptance evidence. These are synthetic UI fixtures, not network or
performance evidence; Windows DPI and the complete FR/EN acceptance pass remain.

Subsequent benchmark work is recorded in the dated package above. Remaining
packages from the accepted desktop proposal include recovery and Mail/Files
polish, Console and Authority professional workflows.
No fresh benchmark, VPS deployment or network reset was performed in this package.

## Build, peer admission and battle evidence (2026-10-06)

- The build uses CYBOU CMake modules only (`cmake/CybouBuildType`, `CybouFlags`,
  `CybouTargets`, `ThirdParty`). Every CYBOU library and executable links
  `core_interface`: stack protector, `_FORTIFY_SOURCE=3`, CET, ASLR/NX/high-entropy
  VA, relro and warnings. The tree builds without warnings with MinGW GCC 13.
- A node accepts 128 inbound sessions, at most 32 per public IP (was 8); local
  addresses are not limited per address. Deployed to the DEV VPS bootstrap and
  both storage peers.
- The desktop network page no longer leaks peer-detail labels (a stack overflow
  while painting) and refreshes only while shown. The status refresh no longer
  re-enables the PoA signer on the GUI thread.
- `tools/battle/battle_test.py` adds a clean-machine restore phase (each Identity
  restarts from its vault alone and reads every protected file and incoming mail
  back from the network) and a PASS/FAIL verdict with exit code. Provider loss,
  repair and settlement are not yet exercised live; a full run with the restore
  phase has not completed yet.

## Bounded console and redundant diagnostics removal (2026-10-06)

The duplicate diagnostics text window and its entry points are removed. Its six
values remain in Network Advanced. The menu now opens the read-only console.
Blank French multiline translations caused `help`, `status` and `storage` to
produce empty responses; the console translations are complete and help derives
from the actual command registry. Public local diagnostics, unlocked own-account
queries and genesis-key-proven Authority queries have separate dispatch/help
availability. Authority proof loss and locking clear private output and history.

Storage uses the runtime's measured physical counters rather than guessed default
capacity. Unknown replicas and finality remain unknown. The former `chunks`
implementation synthesized SHA-256 digests from root text/index and called them
verified BLAKE3 leaves; this false evidence is removed. Real chunk leaf inspection
is unavailable through the current semantic model and the command says so.
Lists, input, output and history are bounded. The UX contract records the next
API-backed inspector/explorer work without advertising unimplemented commands.

Validation: the native Windows Qt suite passed 73 results, including French
command responses, ordinary-versus-Authority dispatch, proof-loss/lock cleanup,
input/history/output bounds and truthful chunk evidence. The main desktop binary
was rebuilt and deployed locally. Native French console screenshots and the full
log are in `artifacts/console-20261006/`. This does not claim live-network audits
or delivery of the future chunk/history APIs.

## Network and unlock presentation (2026-10-06)

The local UI worktree combines Network and Diagnostics: the sidebar caption
shows the current network. The map fills the entire Network page below the
header; peer details and a scrollable Advanced drawer overlay it without resizing
it. Public markers are pulled inside the silhouette; LAN observations use a
separate bounded inset. Arrow keys select peers. Formerly connected endpoints
remain as bounded (100), volatile session observations, shown pale with explicit
disconnected text and unknown current height; switching network clears them.
These observations are not a discovery census or current reachability evidence.
Protection uses protected/total. Network refreshes coalesce; hidden diagnostics
pause presentation, and diagnostic rows update values without widget teardown.

After Identity unlock, a modal branded preparation view follows private DB
opening, history discovery and semantic content preparation. Known scan counts
provide progress; opening is indeterminate. The modal closes after applying a
usable Mail/Files projection, or confirming a genuinely empty complete scan.
Missing roots/failures remain explicit, with a local-view escape and a lock action.
Network connectivity is informational, never a prerequisite for local readiness
or Identity creation. Old session replies cannot revive the modal after locking;
failed catch-up scans retry at the normal worker interval instead of spinning.

Validation: the final native Windows Qt suite passed all 73 results, including
coalesced status bursts with the drawer hidden/visible, full-page geometry,
loading/failure/lock behavior and genuine empty projections through the core
adapter. Native screenshots cover 1040x720 and 1280x860 with synthetic fixtures.
Final logs and screenshots are under `artifacts/network-loading-20261006/`.
The main `build_cybou_qt_mingw/bin/cybou.exe` was rebuilt successfully. This does
not establish live-network performance acceptance or update the VPS deployment.

## Desktop UX source review (2026-10-05)

Source inspection at HEAD `cc6c18e` plus the current worktree confirms live
Mail/Files, encrypted local drafts, attachment references, application history
indexes, the extracted Identity worker/projections and existing Authority
finalizer/settlement controls. The Files product contract's former statement
that the encrypted catalog was not connected was obsolete.

The first implementation slice, based on HEAD `77a29d2`, is now in the working
tree: correlated Queued/Running/Committed/Failed Mail tasks, archive batch
acknowledgement and Undo after commit, debounced draft autosave with save-error
retention, and send acknowledgement after durable publication-job ownership.
An encrypted draft-to-message binding survives restart/stale compose replay;
successful handoff retains this local binding, explicit discard removes it.
One failed worker command no longer drops the remaining dequeued batch.
The binding retains a private recipient/text/attachment fingerprint: altered
draft content cannot silently resume an earlier saved publication. Preparation
can replace this fingerprint only before a durable publication job exists.

Files Advanced and detail scroll survive snapshot updates; active-job copy
counts are filled and unknown counts are explicit. Disabled Files-to-Mail and
download actions explain their protection gates. Folder destinations use IDs
and breadcrumbs. Downloads use atomic QSaveFile commit, preserving an existing
destination on retrieval/write/commit failure. Unchanged semantic Mail/Files
snapshots suppress signals and Home avoids unchanged activity reconstruction.
Mail row children pass mouse input to the existing drag/select viewport.

The next slice adds stable Identity-local activity IDs, cached Home rows with
in-place label updates, coalesced manual local-view refresh and a timestamp
scoped to that refresh. The worker reads semantic indexes without forcing
network sync, scanning history, auditing providers or creating operations.
Failed/interrupted refresh allows retry; old request/session replies are
ignored. Lock clears activity, row caches and refresh presentation. Home
scrolls vertically at small window sizes and wraps activity as plain text.

Remaining delivery work includes physical mouse drag acceptance, incremental
Mail rows and ID handoff, initial large-Files presentation cost, evidence
timestamps and measured
worker/shutdown latency. Folder-import discovery is now bounded and asynchronous
with cancellation; large-catalog responsiveness still needs measurement. Files-to-Mail remains
restricted to Protected references; pending-content compose needs the durable
reference design in the delivery plan. Map/explorer/console and the broader
assurance packages remain target work.

Current diagnostics do not supply a global node census, city locations, remote
uptime or canonical placement reliability. A schematic local-peer map and
scoped metrics are feasible targets; broader observability and ranking require
new evidence/privacy design. Existing distinct DEV storage peers share a host
and do not prove Beta failure-domain independence.

See [`DESKTOP_UX_DELIVERY_PLAN.md`](DESKTOP_UX_DELIVERY_PLAN.md) for delivery
dependencies and [`NETWORK_AND_ADVANCED_UX.md`](NETWORK_AND_ADVANCED_UX.md) for
new product boundaries. Validation for these slices on HEAD `da6d6b9` plus the
current UI worktree: all 55 Qt shell results passed (five new regression cases),
and all 12 ApplicationService cases passed. Focused native Windows checks cover
activity, refresh/session failures, Advanced, mailbox acknowledgements, drop
event routing and supported window-width constraints at the current display
scaling. Logs and the source manifest are under
`artifacts/uiux-implementation-20261005/`. A separately linked/deployed GUI
review build avoids replacing the executable/DLLs held by the running desktop.
These are local component/Qt checks, not distributed durability, native drag
or live-network acceptance. The implementation is not deployed to the running
desktop or VPS.
This page describes
implementation and deployment reality; `AGENTS.md` and the frozen architecture
define the target. The changes described here include committed local development; commit status is not release or deployment evidence.

## Target architecture vs. current code reality

| Component | Target constitution | Current code reality |
|---|---|---|
| Official networks | Compiled DEVNET public constants; MAINNET unprovisioned and GUI-disabled | Implemented (`d5d90cb`): one `OfficialNetwork` per kind via `RequireOfficialNetwork`. Current compiled DEVNET contains the immutable signed genesis under its new NetworkID. Retired DEVNET is not accepted. MAINNET throws as not provisioned; the GUI has no network selector and always runs DEVNET. |
| Network identity | `NetworkID = Network Public Key` | Implemented: verified genesis carries the exact key; every 32-byte field is named `network_binding` and equals `ComputeNetworkBinding(key)`; `GetNetworkId()` exists only for the exact key bytes of a verified genesis. The shared `MakeNodeRuntimeConfig` derives the verified genesis and rendezvous locators from `OfficialNetwork`; the genesis digest marker is removed from the StateStore. |
| Signed genesis | One immutable offline-signed `NetworkGenesis` object and initial state compiled per official network | Implemented: the compiled genesis must carry exactly the compiled Network Public Key and its initial state root. CYG1/CYN1, `CybouNetworkFile` and every file loader are removed. |
| Official startup | Select compiled public constants; verify signature and initial state root; no external official file or separate digest pin | Implemented: every `cybou` command, loadgen, storage smoke/soak and the desktop start only from a compiled network (`--network devnet`). No profile digest pin; the verified genesis digest anchors height zero. |
| Network Private Key | Strictly offline, signs genesis once; private material stays under gitignored `/private/` | `cybou-provision` is an explicitly built offline tool (`BUILD_PROVISION_TOOL=ON`), separate from production `cybou`; `verify-devnet` checks existing secrets without signing genesis. Existing constants and secrets cannot be overwritten. The retired DEVNET material is kept under `/private/devnet-retired-20261003`. |
| Peer discovery | Every node listens and shares dialed and verified inbound peers (DEC-287) | Implemented: HELLO is 82 bytes with `listen_port`; `InboundPeerServer` records listening inbound peers as candidates, `SyncFromConfiguredPeer` connects back to one per pass and adds it to the gossiped discovered set. Desktop, `node run` and loadgen listen by default (29461, else any free port). Before this, peers that only connected inbound were never shared and a desktop found no peer but the bootstrap. |
| Bootstrap | Ordinary CYBOU full peer | Bootstrap binding/protocol/store, `BOOTSTRAP_REQUEST/RESPONSE` and the `cybou-bootstrap` executable are removed. Nodes and the desktop dial the compiled locator first and check its SPKI pin; the locator node serves `--tls-certificate/--tls-key`. Every process uses the same Full Node network lifecycle; signer activation does not reconnect peers. Pinned rendezvous endpoints are protected from discovered-peer crowd-out. The DEV VPS runs the ordinary `cybou node run` command on current current DEVNET. |
| Geo updater | France-only admission with a valid local dataset; fail closed otherwise | HTTPS and published archive SHA-1 verification, bounded gzip decode, SHA-256 CSV cache and validated atomic installation. Each failed attempt re-fetches metadata and archive; five attempts use interruptible 0/2/5/15/60s delays. Exhaustion retains valid cache and retries in 1h, or retries in 5m without a valid dataset. Successful/current checks use 14 days. Test-only fetch/wait hooks cover publication mismatch, rejected candidates, cache retention, strict temporary cleanup and cancellation without network access. |
| Consensus bootstrap state | No grants, roster, or network-role announcements | Removed. |
| Consensus state | Unified current state format | State with Treasury monetary base, onboarding-origin System Balance, settlement cursor and storage leases (M5); no OnboardingPool; older decoders removed. No AUTH or usage counters (DEC-284). |
| Relay proof-of-work | Every user operation carries flat PoW checked by every Full Node and the PoA (DEC-273, DEC-284) | Implemented: `operation_work.h` (`CYBOU/OP-WORK`), `OPERATION_WORK_BITS` 22, names +4; `OperationPool::Admit` and revalidation check it; `OP_META` carries `size u32 + nonce u64`; `CybouNodeRuntime::PrepareOperationWork` solves outside the runtime lock and caches; CLI `operation submit` solves before sending. Never stored in blocks. Component tests set `NodeRuntimeConfig.operation_work_bits = 0`. |
| Candidate execution | Every full node executes candidates before relay; one pool per node, used by PoA for blocks | Implemented (`7f447d4`): `CybouNodeRuntime` owns the single `OperationPool`; local, peer and relayed operations pass `OperationPool::Admit` before staging or forwarding; every node revalidates after each finalized block. CybouNodeRuntime produces from that pool; PoaFinalizer only signs with its durable journal. |
| Anti-spam | Fees, storage rent and flat relay PoW; no AUTH, tiers or Validation (DEC-284) | Implemented: AUTH, `AccountUsage`, operation tiers, `PoaAuthAdjustment`, `ValidationAttestation`/`ValidationPool` and their P2P messages are removed. `CybouState.publications` remains the publication register. |
| Mutual Proof of Storage | Randomized challenge-response byte-offset and nonce auditing | Cryptographic primitive implemented (`b01f2dc`): `src/cybou/storage_audit.h` and `.cpp` provide `StorageAuditChallenge`, `CreateStorageAuditProof`, and `VerifyStorageAuditProof` using BLAKE3 chunk sampling (test coverage in `cybou_finalized_chunk_store_tests`). Network mutual-audit protocol, PoA notarization, and reliability coefficient in state: NOT IMPLEMENTED. |
| Object Pruning | Author `RevokePublication` retires the record and providers purge chunks (DEC-271) | Implemented: `RevokePublication` (ProtocolOperationKind 8, IdentityOperationKind 6) is owner-only, costs the payment fee and closes the lease; a revoked publication fails `FindFinalizedRootPublication`, so no new admission; `FinalizedChunkStore::PurgePublication` runs on each finalized revocation and deletes chunks no other publication authorizes. Desktop Mail/Files revoke automatically: once the application index is complete and every own job is finalized, `PublicationService::RevokeUnreferenced` revokes one own publication at a time (never recovery bridges, keeping 5 operations of the window for the user) when no catalog record came from it, no non-deleted message is it and no live file or attachment tree lives in its leaves; indexing skips revoked publications. "Delete forever" and "Empty Trash" can trigger revocation and managed purge once their prerequisites are satisfied; they do not erase historical capsules, retained keys, recipient copies or prove physical deletion. The fix committed in `a3f05aa` journals pending purge, retains quota on removal failure and retries on reopen and maintenance; startup reconciliation covers revocations whose local purge event was missed. |
| Executables | One production `cybou` | `cybou` is the desktop and the headless CLI (`src/cybou/cli/cybou_cli.cpp`, one parser, no internal positional commands); `BUILD_GUI=OFF` builds a headless-only `cybou`. `cybou-node` and the `network bootstrap` command are removed; `network info` lists the locators. `cybou-loadgen` builds only with `BUILD_TESTS`. |
| Operation routing | Uniform CYBOU P2P Full Node mesh | HELLO has no capability field. No PoA transport proof or special route. Sync completion is advisory. Storage is intrinsic; StorageId is challenged only for storage interaction. |
| PoA | Sole independent canonical finalizer | Single-operator PoA re-executes every candidate through the node pool and finalizes; a multi-node CYBOU P2P test covers ordinary nodes and PoA. |

## Finalized-state runtime snapshot (2026-10-05)

`CybouStateStore::GetStateSnapshot()` shares immutable finalized state through
an atomic `shared_ptr<const CybouState>`. First access validates the persisted
state, hash and network binding; genesis initialization and both ordinary and
min(BlockID) conflict commits publish a replacement only after the KV batch
succeeds. Allocation precedes the durable write. Empty no-op blocks retain the
same state object. Existing readers can retain the previous immutable state.
Runtime, operation admission, Identity/application services and desktop state
reads use this snapshot; `LoadState()` remains an explicit disk integrity read
and is retained at runtime startup. Direct mutation of the underlying database
while its StateStore is active is unsupported. Wire bytes, genesis, consensus
execution and durable PoA safety checks are unchanged.

Local validation: Windows MinGW Release headless and Qt GUI builds passed;
87 selected runtime/state/PoA/Identity/application/publication/storage regression
tests passed with 1,600 assertions. Snapshot-specific checks cover pointer reuse,
retained-reader immutability, commit/reopen equivalence, rejected/empty blocks,
corrupt persisted hash on reopen, and min(BlockID) conflict publication. This is
local component evidence, not deployment, battle, soak or throughput evidence.

This implements R1 of the performance refactoring proposal. Bounded storage
I/O, PoA event wakeups, application history indexes, Qt projection extraction
and battle/soak profiling remain separate work; no throughput improvement is
claimed without measurement.

## Runtime ownership domains (2026-10-05)

R2 retains `CybouNodeRuntime` as the desktop/headless facade, with three private
owners inside the same Full Node: `ChainCore` owns the database, state store,
candidate pool, PoA signing/retry state, Identity coordinators and operation
relay/status; `NetworkCore` owns sessions, peer retry/discovery, ingress limits
and routing hints; `ProviderCore` owns encrypted blobs, provider admission,
retention and the stable storage secret. Their implementations live in
`node_runtime_chain.cpp`, `node_runtime_network.cpp` and
`node_runtime_provider.cpp`. The facade keeps construction, aggregate diagnostics and cross-domain
admission/relay, finalized purge events and storage payout binding orchestration. These are internal ownership boundaries, not network roles or
independently exposed services.

Routing hints have a short dedicated NetworkCore mutex instead of the chain
mutex: peer callbacks can consult routes while session I/O owns its mutex,
and route access does not acquire chain/session locks. Existing session ->
chain callbacks and separate diagnostic lock scopes are retained. Provider
storage retains its own store locks. Destruction closes sessions before
provider and chain stores and cleanses the provider secret even if facade
construction fails after provider initialization. Storage endpoint binding
verification shares R1's immutable state rather than copying it.

Local R2 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 252 cases and 100,542 assertions, including
routing/discovery, synchronization, operation relay, provider admission,
revocation/purge, Identity recovery and PoA conflict/signing safety. The facade
implementation is 237 lines after extraction. No deployment or performance
benchmark is implied by this component evidence.


## StorageService responsibility extraction (2026-10-05)

R3 retains one Identity `StorageService` and the existing `StorageTransport`
interface. Private components have distinct ownership:

- `PlacementRepository` owns encrypted placement/rebuild records, exact-consumption
  decoding and atomic placement/index persistence;
- `EvidenceLedger` owns signed receipt persistence, bounded provider evidence,
  volatile replica verification times, shadow accrual and verified-slot counting;
- `ReplicaVerifier` owns random-offset challenges, full GET/ChunkID validation
  and evidence updates over the existing transport;
- `ProviderSelector` owns CSPRNG ordering by verified payout account (falling back
  to StorageId) and endpoint shuffling for recovery GET.

Their implementations are separate translation units; `storage_service_internal.h`
contains private declarations. Placement/audit/repair coordination lives in
`storage_service_repair.cpp`; recovery, public projections and settlement
preparation remain in the main service. No independently exposed storage service,
provider role, wire entity or canonical evidence is introduced.

Application DB keys and current binary layouts are unchanged. The ledger retains
its own evidence mutex; placement coordination retains the service mutex and
per-publication guard, releasing it during remote I/O. Existing rent caps, integer
rounding, payer exclusion, distinct economic identities, first-failure replica
downgrade and restart closure of credited intervals are preserved. Settlement
preparation obtains aggregate verified slots from the ledger without accessing
its maps or mutex directly. No evidence transport to PoA or parallel storage I/O
is implemented by this extraction.

Local R3 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 253 cases and 104,023 assertions. Added
regressions verify that persisted counters/placements do not authorize settlement
after reopen until fresh replica checks, and truncated evidence or an invalid
rent remainder are rejected without deleting valid placement metadata. Existing
coverage verifies signed receipts, restart accrual, repair without local cache,
partial rebuild persistence, payout identity deduplication, concurrent inspection
while placement I/O runs and P2P PUT/GET/audit. The main service implementation is
378 lines after extraction. This is component evidence, not deployment, soak or
measured concurrency/throughput evidence.


## Bounded concurrent storage I/O (2026-10-05)

R4 adds one node-local `StorageIoScheduler`: four persistent workers, at most
eight queued jobs, one active job per StorageId and two read/verification jobs.
The read budget conservatively includes random-offset checks that can fall back
to full GET. GET/proof recovery also passes through this scheduler. These are
local resource limits, not protocol roles or consensus parameters.

Placement plans up to four distinct economic identities for one chunk, sends
PUTs concurrently, collects every result, verifies each StorageId-bound receipt,
and checkpoints the placement once per completed batch. A batch shares one
owned ciphertext; it never queues an entire file. Audits check the current
chunk's replicas concurrently and retain exact-byte verification and evidence
rules. Per-publication guards serialize audit, rebuild and placement mutations,
while semantic inspection remains available during remote I/O. Exceptions are
returned through futures; queued jobs drain before runtime domains are destroyed.

Remote storage requests use a node-local pool of at most four cached ordinary
P2P sessions, one in flight per proven StorageId and two full GETs. Each session
is exclusively leased, separately proves the expected StorageId and uses the
same TCP deadline, Geo admission, compiled TLS pin, HELLO network and known-chain
checks as mesh sessions. Admission is rechecked on reuse. Storage I/O no longer
holds the mesh manager mutex; discovery/relay/sync remain in PeerManager. The
old PeerManager storage-transfer path is removed. No wire, genesis, canonical
state, payout formula or database format changes.

Concurrency is covered by gated tests for worker/read/provider budgets, exception
release and shutdown draining; simultaneous PUT/GET and receipt accounting;
invalid-receipt rejection and missing-replica resumption; and real TLS/HELLO/
StorageId session overlap and reuse. No performance multiplier is claimed;
battle/soak and measured throughput remain R8 work.

Local R4 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 257 cases and 97,660 assertions. This is
component and local integration evidence; no VPS deployment or soak was run.

## Event-driven PoA production (2026-10-05)

R5 replaces the 100 ms production polling tick with a runtime condition variable
and a local change revision protected by the chain mutex. New locally executed
candidates (local submission, mesh relay or signed settlement), signer changes
and finalized-head/pool revalidation wake the worker. Duplicate or rejected
admissions do not generate candidate wakeups. Reading the revision before
readiness and checking it under the same mutex in the wait predicate prevents
lost wakeups. Shutdown sets its stop flag and notifies this wait explicitly.

Idle or signer-disabled production waits without a timer. Pending work waits
until the successful-block interval or transient retry deadline, unless a state
change or stop occurs first. Existing exponential retry (100 ms to 5 seconds),
exact journaled candidate reuse and fail-closed safety halt remain intact.
DEC-286 now describes event-driven scheduling; non-empty automatic blocks,
manual finalization, height-counted windows, wire, state and durable signing
rules are unchanged. No network migration or genesis change is required.

Local R5 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 260 cases and 101,813 assertions. New tests
cover pre-wait events, idle/stop/deadline predicates, relay admission versus
duplicate suppression, block-interval enforcement, prompt shutdown and pending
work after worker restart. Existing service coverage verifies exact-candidate
retry after signer failure and resumed production after signer unlock.

## Local finalized-event coordinates (2026-10-05)

R6 adds `finalized_event_index.cpp` behind `CybouStateStore`. The current local
`cybou/events/` namespace contains operation coordinates (including publication,
rotation and revocation), publication-bearing heights, KEM coordinates by public
AccountID/epoch and a complete-head marker. Each coordinate is exactly 44 bytes
(height, operation index, BlockID); ordered height/epoch keys use fixed-width hex.
There is no recipient index or cached plaintext. New entries and removal of a
losing same-height canonical block are part of the canonical commit's atomic
batch; the index is not committed by state root and changes no wire/genesis.

Missing, stale or malformed complete-head metadata triggers a rebuild from
retained blocks. Rebuild invalidates the marker first, clears derived namespaces
in bounded batches and verifies parent continuity plus hybrid PoA certificates
before publishing a complete marker. Interruption never leaves a complete partial
index. Positive lookup coordinates are checked against the canonical source
block, operation identity/type and parent link; malformed/stale operation or KEM
rows cause one rebuild attempt. Missing referenced source history yields unavailable, and
no index is treated as independent proof of finality. Normal lookup validates
its source block, rather than re-verifying the entire preceding history each time.

Runtime operation/KEM lookup uses these coordinates. ApplicationService queries
bounded ordered publication-height ranges, skips unrelated heights and retains
atomic per-relevant-block private records/checkpoints and bridge rescanning.
Each range contains at most 256 relevant heights; semantic capsule filtering,
unavailable-content retries, own-publication recovery and monetary rules remain
in their existing services. `KVStore::ForEachStringRange` supplies an inclusive
fixed-length-key range with early termination. Rebuild requires retained source
blocks; deleting all canonical data still requires ordinary verified mesh sync.

Local R6 validation: Windows MinGW Release headless and Qt GUI builds passed;
the complete core suite passed all 263 cases and 98,100 assertions. New
regressions cover malformed/missing operation/KEM rows, marker deletion and
store reopen, rebuild failure with absent source blocks, unchanged canonical
state root, removal of losing-branch coordinates and sparse Mail recovery after
private Application DB deletion. Existing rotation, revocation, storage admission,
bridge recovery and receipt/settlement tests also pass. This is local component
and integration evidence; long-history recovery timing remains R8 profiling work.

## Qt Identity session responsibilities (2026-10-05)

R7 splits the Qt adapter's implementation into private Identity session,
scheduler, Mail, Files and storage translation units. `IdentitySession` owns the
encrypted Application DB and the existing three core services, plus rotation
preparation. `SessionScheduler` owns the sole Identity worker, command queue,
stop-aware interval wait and shutdown drain. The session stops and joins it
before destroying any service or projection.

`MailProjection` owns the unindexed outgoing Mail overlay and semantic Mail
snapshot construction. `FilesProjection` owns pending catalog mutations,
item-to-job associations and the local availability cache. `StorageProjection`
owns shared publication job results and the existing audit, GC, durability,
reference-revocation and settlement preparation work. Mail/Files command entry
points remain the same adapter API; their implementations live with the relevant
projection. GUI draft/delete/star reconciliation stays on the GUI thread, and
queued state delivery retains the session-generation check. No extra Identity
worker or core service is introduced.

Local R7 validation: Windows MinGW Release GUI and native Qt test targets built
successfully. The full offscreen desktop suite passed all 50 cases, including
live Mail/Files publication, draft/delete/star reconciliation, close/reopen,
private-content locking and recovery-phrase rotation with the live session.
This is local regression evidence; battle/soak and profiling remain R8 work.

## Storage economy status (2026-10-04)

DEC-274–DEC-283 are frozen as target architecture (M1). M2 is implemented:
`NodeRuntimeConfig::storage_capacity_bytes` is the explicit local capacity `V`
(default and minimum 15 GiB outside memory-only tests, `cybou node run
--capacity`), `ChunkBlobStore` rejects new blobs beyond `V` with
`CAPACITY_EXCEEDED`, and `FinalizedChunkStore` receives the provider budget
`ProviderBudgetBytes(V) = floor(2V/3)`; diagnostics report local and provider
usage separately. The desktop has no capacity picker yet and uses the
15 GiB default. M3 is implemented: successful admissions return a signed
`StorageReceipt` (P2P `CHUNK_ADMISSION_RESULT`), StorageService counts a replica
only with a receipt from that StorageId and keeps it in the encrypted
Application DB, `STORAGE_AUDIT_CHALLENGE`/`RESPONSE` (25/26) carry random-offset
audits, and `StorageService::ProviderEvidence()` exposes bounded in-memory
rolling evidence. Replicas still drop on the first failed check.
M4 shadow accounting is implemented: `storage_economy.h` holds the exact
integer rent arithmetic (512 KiB units, 5 CYBOU/GiB/day/replica, floor with
carried remainder). StorageService credits verified billing-unit-seconds only
between two successful checks of a replica (receipt opens the interval, gaps
capped at 24 h, failure or restart closes it), accrues a shadow reward per
provider, persists evidence in the encrypted Application DB and reports
`EstimatedDailyRent()`. Diagnostics show a provider-side estimate. No CYBOU
moves; measured DEVNET numbers will validate the rate before M5.
M5 consensus economics is implemented in `state.cpp`, `storage_lease.h/.cpp`
and `block_executor.cpp`: no MAX_SUPPLY or OnboardingPool, `TotalCybou`
conservation, Treasury-funded 20,000 onboarding, onboarding-origin tracking,
`RootPublication.lease_periods` (initial lease paid atomically), `StorageLease`
(operation kind 9) and PoA-signed `StorageSettlement` (kind 10, contiguous
86,400 s periods, per-lease period cap, no self-payout, origin-preserving
payouts and refunds). Storage quotas are removed; `MAX_PUBLICATION_CHUNKS`
remains a safety bound. Providers admit chunks only under an active lease;
PublicationService leases at publication and renews inactive leases. The
Central Authority desktop settles one complete UTC period per click
("Settle storage period"): `StorageService::SettlementEntries` splits each
active lease's period cap over its `units × replicas` slots and pays the live
payout account of every replica verified since the period start; the
settlement also advances the cursor and refunds ended leases. Limitation: the
PoA pays only leases whose placements its own Identity holds; evidence of
other payers is not yet transported to the PoA, so their escrow is refunded
at lease end. Not yet implemented: that evidence transport, lease renewal UX. Because the state and genesis formats changed, the
previous compiled DEVNET was retired. After M7, AUTH and Validation were removed
(DEC-284), changing the state format again: main compiles the DEVNET
`eee26eca…3665` (`verify-devnet` passes); the DEV VPS runs it with
`--capacity 40GiB`; desktops cut over on demand.

M6 adversarial evidence (`cybou_resource_limits_tests`,
`cybou_storage_placement_simulation_tests`):
- monetary conservation: a 400-block deterministic random walk over SystemLock,
  publish-with-lease, StorageLease, RevokePublication and StorageSettlement keeps
  `TotalCybou` exact and every state canonical;
- payout attacks refused: unknown lease, missing payout account, self-payout,
  over-cap or over-escrow payout, duplicate/unordered/zero entries, replayed or
  gapped period, overflowing period start, foreign signer, missing PoA key,
  oversized or overflowing lease extension;
- storage failures: existing suites cover lost, corrupt and offline providers,
  bit rot healing, repair without local cache and audit-detected wrong answers;
- concentration (1000 nodes: 700x15 GiB, 200x150 GiB, 80x1 TiB, 20x20 TiB) under
  the implemented uniform selection: at 5%/30%/70% demand the top 1% of nodes
  hold 2.8%/11.9%/33.9% of replicas, the 5 largest 1.4%/6.0%/17.0% (gate < 70%),
  effective provider count 678/161/43, and every home node receives work.
  Capacity-weighted selection would give the top 1% ~39% and leave 60% of home
  nodes idle at 5% demand;
- Sybil: with selection by payout account (implemented, DEC-280), 100 nodes of
  150 GiB under one account get 0.11% of replicas, the same as one 15 TiB node;
  per-StorageId selection would have given them 10.15%. Splitting into 100
  separate Identities still reaches 10.4% at 10% demand; only AccountCreate PoW
  prices that (open question in `25_OPEN_QUESTIONS.md`).

## Evidence limits reviewed on 2026-10-04

Committed code baseline: `a3f05aa`; local Windows headless verification passed
230 core cases / 9414 assertions. This is not GitHub CI, Linux, desktop UI or
deployment evidence. Subsequent governance commits changed documentation.

- Replica placement counts distinct proven StorageIds, not independently owned
  disks, hosts or operators. Physical independence remains the Beta target.
- StorageService checks replicas with random-offset audits over CYBOU P2P and
  full GET plus BLAKE3 one time in eight (always without a local copy). Per
  provider evidence (receipts, successes, failures, full verifications, times)
  is bounded; receipts and the evidence index persist in the encrypted Application DB.
  PoA settlement exists only for leases whose placements the Central Authority
  Identity itself holds (see the storage economy paragraph above).
- Repair is attempted from valid surviving bytes to available providers. Finality
  and admission ACKs alone do not prove current availability or recoverability.
- Local allocation and finalized quotas do not measure actual 1:3 reciprocal
  contribution. Signed storage receipts prove admission only.
- Finalized revocation stops new admission and initiates compliant-provider
  purge of unshared chunks. It does not remove historical capsules or establish
  per-object crypto-erasure. Recipient and adversarial copies are outside purge.

These are implementation limits, not changes to the frozen target decisions.

## DEVNET-only development (2026-10-03)

Runtime supports official DEVNET and the disabled, unprovisioned MAINNET only.
The alternate development profile, fixed test-network key derivation, runtime
genesis creation, seed-export command and dedicated build option are removed.
Development and integration use the existing compiled DEVNET with its saved
local keys. This removal changes no keys, NetworkID, genesis or canonical state.

France-only peer admission applies to desktop, headless nodes and development
clients. The private-route bypass and its GUI/environment/CLI paths are removed.
Geo diagnostics now have Waiting and Ready states. Event logs offer minimal or
detailed public fields; neither mode changes network policy.

The automatic multi-process/fault controller and its templates, namespace helper
and controller tests are removed. CLI acceptance connects an ordinary temporary
Full Node to existing DEVNET without a signer or operation submission. Storage
clients retain ordinary DEVNET admission and finality requirements. Component
tests use in-memory fixtures and synthetic Geo input through the actual parser;
these fixtures are not available as a runtime network profile.

Current NetworkBinding:
`eee26eca805d5a16b2f550d66b3355ecf50d90c43138bc1fefc34df92f7f3665`.
Signed genesis anchor:
`1abf2355b6597e53c6affb73d4c92bacc9b970f956a1a259bac8c3699f958cfc`.
Genesis allocations: `cybou` 100,000,000,000 CYBOU (Central Treasury; same
phrase and PoA key as the retired DEVNETs; AccountID is created on initial
onboarding into the current network); `bootstrap` 0 CYBOU. The private
material is under gitignored `private/devnet-no-auth/`. Production
Network Root derivation/signing are disabled. The existing official PoA is
locked; finality/content integration requires the operator to unlock it.

The VPS runs the ordinary Full Node at `51.255.46.58:29461`. Public peer admission
uses verified DB-IP Geo data and the compiled TLS pin. The earlier desktop/VPS
network domains remain retired; this pass does not reset or import them.

On 2026-10-04 the operator explicitly authorized a coordinated destructive
DEVNET reset with the exact existing keys and genesis. The preceding active
Windows state was archived under `CYBOU-retired-20261004-live-reset`, the VPS
state under `/var/lib/cybou/node-retired-20261004-live-reset`, and local WSL live
test state under `~/cybou-live-retired-20261004`. The height-2369 Windows chain
had a height-zero PoA journal halted with `HISTORY_MISMATCH`; that evidence was
archived rather than silently repaired. This is a development reset under the
explicit `AGENTS.md` exception, not a production recovery claim. The NetworkID,
genesis and TLS pin above remain unchanged. Historical blocks must not be imported
into the restarted exercise; they remain cryptographically valid.

See [DEVNET development](DEVNET_DEVELOPMENT.md) for commands and operator rules.

The 2026-10-04 live exercise resumed PoA on Windows and verified remote Mail and
Files publication and recovery through the VPS, including clean application
index and chunk loss. See [live content acceptance](DEVNET_LIVE_ACCEPTANCE.md)
for tested scope, repairs, the Windows transport workaround and economic limits.

Verification results must be attributed to the tested revision and build.
Committed baseline checks are recorded in [Data assurance and erasure](DATA_ASSURANCE_AND_ERASURE.md); they do not establish desktop CI or deployment status.
The DEVNET CLI acceptance reaches the existing pinned locator, restarts an
ordinary node and checks read-only doctor without activating a signer or
submitting operations. Production Windows GUI and Linux headless builds pass;
both linked Network Root guards reject private derivation/signing. The VPS
service is active and desktop/VPS doctors report READY on the unchanged genesis.
Its binary update preserved state. Finality/storage fault scenarios are not run
against the locked official signer during this removal.

Files private IDs are allocated once in the model and reused by the core adapter
for uploads, folders, copies and attachment saves. Pending-to-indexed Files
identity therefore requires no replacement mapping. See the live-core Qt
regression and delivery plan for current validation.
Validation for this slice: full Qt suite 55 passed/0 failed and native Windows
focused suite 5 passed/0 failed (including setup/cleanup); isolated review build
passed, based on `1ee870f` plus the UI worktree.

Folder-import slice (2026-10-05): background bounded discovery (10,000 entries,
64 levels) precedes publication staging. Nonmodal progress/Cancel, bounded GUI
batches, safe partial cancellation, hidden/empty folder preservation and lock/
page-lifetime handling are implemented. Isolated build passed; full Qt suite
56 passed/0 failed and focused native Windows suite 5 passed/0 failed (including
setup/cleanup), against `1ee870f` plus worktree. Blocking-volume cancellation and
large-catalog latency remain evidence gaps; cancellation does not undo accepted
publication jobs. Logs are in `artifacts/uiux-implementation-20261005/`.

Files row slice (2026-10-05): retained ID-keyed table/grid items, metadata/status
updates in place, selection/current-item continuity across refresh/sort/rename,
visible-anchor preservation on table insertion, view-mode selection transfer,
removal/filter cleanup and lock clearing are implemented. Child-count aggregation
is linear in the catalog and glyph icons are shared. Native synthetic 2,000-item
construction took 796 ms; ten one-file updates had median 11 ms/max 12 ms. This
is local fixture/model/page timing, not live storage or completed-frame evidence.
Per-item widgets still make initial presentation expensive; delegate/lazy-view
work and larger-catalog/frame benchmarks remain required.
Final isolated review build passed; full Qt suite 58 passed/0 failed and native
Windows focused suite 7 passed/0 failed including setup/cleanup, against
`1ee870f` plus worktree. Logs: `qt-file-rows-final-full.txt` and
`qt-file-rows-final-windows.txt` in the UI artifact directory.

Files delegate slice (2026-10-05): one viewport status delegate replaces per-row
QLabel widgets; plain display/accessibility text and full tooltips are retained.
The Qt table accessibility API returns the updated status. Synthetic native
construction/update times are 90 ms / median 8 ms, max 9 ms for 2,000 items;
527 ms / median 43 ms, max 49 ms for 10,000. These are fixture model/page timings
without completed-frame, live runtime or physical screen-reader evidence.
Full Qt suite 58 passed/0 failed; Windows focused 7 passed/0 failed and 10,000-item
run 3 passed/0 failed including setup/cleanup. Isolated review build passed and
list/upload screenshots were inspected. Sources: `1ee870f` plus worktree; logs
`qt-file-delegate-*.txt` in the UI artifact directory. Remaining performance
acceptance concerns large snapshot updates, scrolling and frame completion.


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
