# Remaining desktop UI/UX work — 2026-10-06

Status: Level 4 delivery backlog, reviewed against source HEAD
`9b04d768d4f594485f0fe3157366199c2b5f9ea2`. This is a remaining-work plan,
not a release acceptance statement. It complements DESKTOP_UX_DELIVERY_PLAN.md;
AGENTS, architecture and product contracts retain their authority.

## Assessment and evidence limits

The Identity-centered shell and daily Mail/Files paths are in place. Retained
Mail/Files rows, acknowledged Mail moves, draft recovery, stable semantic IDs,
Advanced expansion, cancellable folder enumeration and plain-text reader cleanup
are implemented. These are foundations to keep, not features to rebuild.

Fresh Windows fixture captures cover Home, onboarding/restore, Mail/compose/offline,
Files, Wallet, Identity, Diagnostics and Authority. Capture settings: dark
1440×960 and light 1040×720; empty/offline at 1040×720. These are fixture layout
observations, not distributed operation or physical mouse acceptance. Current
app and test binaries do not establish one reproducible current-HEAD build:
the normal test executable lacks the newer reader regression slot present in
source; the isolated review executable contains it. Record binary hashes and
source separately. Artifact report, screenshots and logs are under
`artifacts/uiux-audit-20261006/` (local ignored evidence).

Six available scenarios passed in the normal Windows test binary, 8 results
including setup/cleanup. Two reader/list regressions passed in the review binary,
4 results including setup/cleanup. An initial invocation of the normal binary
reported a missing test function; that is a build/provenance gap, not evidence
of a reader behavior failure. No full current-HEAD suite or live Beta acceptance
was performed in this audit. The earlier rotation deadline failure remains open
as a stability investigation despite its passing isolated rerun.

## Observed gaps

| Finding | Evidence | Consequence |
|---|---|---|
| F1 Search work and scope | MainWindow::rebuildSearchIndex clears/recreates all suggestions on every Mail/Files change; submitSearch searches the current Files view or otherwise switches to Mail | Large collections incur hidden work; the global placeholder does not explain Enter's narrower scope |
| F2 Context after shell rebuild | reloadAppearance snapshots compose but rebuilds all pages and returns only to the selected product | Folder/view/search/selection/scroll/Advanced/reader context can be lost on theme/language change |
| F3 Remaining refresh churn | Wallet rebuildActivity destroys rows on Mail/Files changes; Authority refresh clears sections on a 2-second timer | The stability work applied to Home/Mail/Files is incomplete across the product |
| F4 Protection explanations | Files shows 1/2 or unknown counts, but CybouFileItem has no observation time or typed blocker; Download remains Protected-gated despite independent available_offline | Users cannot reliably distinguish waiting, stale evidence and actionable failure; local usable content can appear inaccessible |
| F5 Responsive information density | At 1040×720 Files keeps a separate 300px details pane and loses the size column; restore needs vertical scrolling | Tasks fit, but context and action discoverability need deliberate responsive design and keyboard verification |
| F6 Wallet forecast and localization | System Balance/fee is labelled remaining operations; rent is absent from this estimate. relTime uses untranslated English literals visible in French Wallet/Diagnostics | Budget advice can mislead; language consistency is incomplete. English fixture message content is not itself a translation defect |
| F7 Assurance wording | Home/Identity/onboarding display broad Protected/Secured/post-quantum labels; Files uses technical recovery-capsule text | Labels need explicit property and scope; they cannot imply tested recovery, whole-product certification or complete erasure |
| F8 Advanced product depth | Diagnostics has a local network monitor; Authority has controls/summaries. No Network page/map, indexed explorer or own-content console exists in the shell | Requested advanced features remain delivery work, with telemetry/evidence dependencies |

## Delivery order and acceptance

### R0 — Reproducible acceptance baseline (P0; W0/W8)

Create one isolated app/test build from the same source revision and configuration;
record hashes, Qt/DPI/window sizes, fixture/live scope and test logs. Keep the
working desktop and nodes running. Investigate the rotation deadline with bounded
phase timing before altering timeouts. Track required UI scenarios as implemented,
automated, native/manual and live-accepted separately.

Exit: app/test source provenance is known; required new test slots exist;
failures have recorded causes or remain explicit release blockers. A passing
rerun never silently replaces a failing run.

### R1 — Stable and responsive collections/search (P0; W2/W4)

Profile Mail at 2,000 and 10,000 items; replace row widgets with a delegate/model
if profiling warrants it. Measure first completed frame, scroll, snapshot/filter
cost and update latency, not construction alone. Reconcile search suggestions
incrementally, coalesce changes and bound query results. Preserve an open search
popup, current suggestion, text and focus during unrelated updates. Avoid costly
hidden-page presentation. Retain Wallet/Authority rows and update only changed
sections; separate age-label ticks from data refresh. Keep explicit manual Refresh
and observation time; it must not trigger sync, audit or history rebuild.

Exit: measured reference datasets and hardware establish budgets; one-item changes
do not rebuild unrelated rows or interrupt typing/selection. No private text or
paths enter performance logs. A delegate change retains accessible semantics,
selection, context-menu and drag behavior.

### R2 — Complete and verify daily workflows (P0; W1/W3/W8)

Exercise real mouse drag from row children and multiple selected messages to
Archive/Trash, folder drops and Compose attachment drops at supported DPI settings.
Use the existing correlated commands, with target feedback, per-item partial
results and ordered Undo. Verify Protected Files → Compose → acknowledged draft
→ durable send → receive → Save to Files → original-message deletion and restart.
Include recipient failure, offline/reconnect, lock and source removal. Do not
report synthetic MIME events as physical mouse acceptance.

Complete a shared non-blocking task view for queue/wait/current phase, safe retry
and redacted details. Prefer inline/toast progress for routine moves; avoid an
archive modal that prevents other work. Only show determinate percentage when
work is measurable; cancellation is exposed only at phases the backend can stop.

Exit: each entry point has the same result/gates, failures retain recoverable data,
acknowledged drafts survive restart and saved attachments retain authorized
references. Existing durable-send binding and exact-byte retry remain intact.

### R3 — Understandable file protection and safe local actions (P0; W2/W3/W8)

Extend the owning service projection with bounded observation time, source/scope
and safe typed blockers where actual evidence exists. Present confirmation,
remote protection, local availability and retrieval separately. For 1/2 show the
known count, when measured and what the user can safely do; do not guess why a
replica is missing. Distinguish unknown, stale, repair and failed observations.
Put disabled-action explanations inline as well as in tooltips.

Evaluate verified local Open/Save while remote protection is incomplete. It needs
core-reported authorization, local bytes and integrity checks, not a GUI override.
Keep pending-file reference attachment gated until durable ownership/retention,
restart and duplicate-staging semantics are designed. Existing Protected sending
must remain usable. Do not change randomized paid placement or infer failure-domain
independence from replica count.

Exit: fixtures and service tests cover 0/2, 1/2, 2/2, unknown/stale, offline cached,
corrupted, repair and revoked states; each displayed reason is source-backed.

### R4 — Preserve context and finish shared interaction (P1; W4)

Snapshot nonsecret presentation state per unlocked session: Mail view/reader,
Files folder/mode/selection, filters, scroll anchors and Advanced expansion.
Restore after theme/language rebuild without retaining private state across lock
or account change. Define global versus product-local search scope explicitly;
provide bounded result groups and useful empty/no-result/error states.

Give Files a responsive details drawer or full-width detail mode when columns
become unreadable. Keep focus return and Escape/back behavior consistent. Add
comfortable/compact density only after layout and measured performance are sound.
Polish long names, keyboard navigation, selection actions and drag feedback using
the existing token system rather than rebuilding the visual identity.

Exit: context survives refresh/rebuild and clears on lock; keyboard tasks work
without pointer-only tooltips. Supported 100/125/150/175/200% scales and window
sizes are tested on appropriate screens; small-screen scrolling keeps actions
reachable. Measure contrast/focus and verify assistive technology separately.

### R5 — Wallet, onboarding and trust copy (P1; W4/W8)

Replace remaining-operation advice with a clearly scoped fee-only estimate or a
source-backed budget view including active lease/rent assumptions; hide unsupported
forecasts. Keep transfer review explicit about resolved recipient, amount, fee,
Balance/System Balance source, unresolved operation and irreversible conversion.
Retain contact suggestions and history context during updates.

Localize relative dates, plural forms, status/tooltips and errors. Verify French
and English across both themes; do not translate user-authored message content.
Refine create/restore/name flows and explicit local capacity explanation (minimum
15 GiB). Explain vault password versus recovery phrase, recovery scope and uncertain
operations. Do not require peer sync completion before creating an Identity.

Replace broad Protected/Secured claims with scoped statements and a clear action
to inspect evidence. A recovery phrase being present is not a successful restore
test. Defer expansive security/certification claims to actual evidence/governance.

Exit: clean-machine create/restore and recovery scenarios pass; normal tasks need
no protocol vocabulary. Budget text cannot be mistaken for guaranteed storage
lifetime or a count of arbitrary future operations.

### R6 — User Network page and France map (P1; W5)

Reuse bounded local diagnostics: own connectivity, locally verified height,
observed peers, capacity aggregates and own content-protection summary. Provide a
list first, then stable schematic France placement with an explicit illustrative
location label. Show LAN/unknown separately. Add source, collection window and
update time. Map selection and list selection share accessible details.

Local observed reachability may be added with sample counts/gaps and bounded
retention. Top100/200/300 requires an actual wider observation source and privacy
design; never synthesize nodes, global uptime or city coordinates. Ranking affects
display only, never randomized storage assignment. No provider-selection feature
is introduced by this plan.

Exit: small/no-data/offline samples are honest; map has a keyboard/list alternative;
no endpoint/Identity linkage or extra storage proof is created just for display.

### R7 — Authority explorer and operational workspace (P1/P2; W6)

Extend the existing page with paginated/indexed locally verified blocks,
operations, account values, roots, leases/escrow and settlements where APIs exist.
Separate canonical facts, local candidate pool and off-chain evidence. Show queue
age, phase timing, signer safety status and settlement evidence readiness.
Read-only public exploration must not acquire signing powers.

Exit: no GUI-thread full-history scan; no plaintext content or ownership guesses;
signing/settlement authorization remains the actual local genesis-authorized key.
Missing completeness evidence stays visible, and idle chains are not labelled
outages merely because a block is old.

### R8 — Own-content inspector and bounded console (P2; W7)

Add read-only own-file publication/chunk-tree inspection through the unlocked
Application DB. Explain authorized references and scoped integrity/retrieval
evidence. Then expose only implemented bounded commands (status, own storage
summary, own files/info/chunks, observed peers, jobs), with help, output limits,
safe cancellation and private-history cleanup on lock.

Exit: no shell, arbitrary SQL/script, mutation/signing commands, secrets or foreign
ChunkStore/provider objects; large graphs and lock races remain bounded and safe.

### R9 — Assurance/deletion, recovery and Beta gates (P0 gates/P2 depth; W8)

Design per-object evidence for confidentiality, integrity, availability and
recovery with scope and age. Explain Trash, finalized revocation, lease closure,
provider purge and retained shared/recipient/backup copies as distinct phases.
Where remote purge acknowledgements are absent, say so. Do not imply universal
RGPD conformity, residency, crypto-erasure or certification from UI labels.

Run clean-machine restore, rotation/historical-key recovery, provider outage,
shared-reference retention and purge/unlink-failure restart tests. Beta durability
requires measured independent failure domains; the colocated DEV peers do not
prove them. Passive diagnostics precede optional reviewed DEVNET test-build
benchmarking. No automatic benchmark on page entry, production load generator,
network reset or extra signer.

Exit: every assurance claim has recorded source/environment/evidence. Governance
and backend evidence gaps block the respective claim, not a cosmetic redesign.

## Recommended implementation batches

1. R0 + R1: trustworthy build baseline, collection/search responsiveness and stable
   Wallet/Authority updates.
2. R2 + R3: complete daily workflows, physical interactions and useful protection
   explanations/local actions.
3. R4 + R5: context, responsive presentation, accessibility/localization and
   wallet/onboarding clarity.
4. R6: first honest user Network overview and schematic France map.
5. R7: indexed explorer and operator evidence workspace.
6. R8: own-content inspection followed by the restricted console.
7. R9: evidence work runs alongside relevant batches; live Beta acceptance closes
   the release only after the applicable safety/recovery/durability gates pass.

The baseline targets Gmail/Drive-like predictability and keyboard efficiency,
not a pixel copy or feature parity claim. Fixed calendar estimates should follow
R0 profiling and R3/R7/R9 API discovery, because these contain backend dependencies.
