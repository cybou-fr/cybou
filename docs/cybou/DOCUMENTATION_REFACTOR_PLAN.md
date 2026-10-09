# Documentation refactor delivery plan

Status: CURRENT
Scope: Documentation-only work; source baseline 1f08f8c5, 2026-10-09.

The supplied audit was based on 7d847647. Current P2P ends at 28, not 26.
Network retains provider capacity and hosted bytes; the operator-confirmed base
rate is 5 op/min, with measured current rate/achieved maximum separately labelled.
GUI fixes are a separate code package, never an implicit documentation change.

| Package | Result | State |
|---|---|---|
| 01 authority | Mandatory AGENTS, one classified index, cancelled decisions archived, conflict register, historical plan/cutover redirects | Implemented; structural checks pass (86 classified/indexed documents), manifest/diff checks pass |
| 02 protocol | Exact P2P/HELLO, chain/PoA and chunk layouts, off-chain audit, actual settlement wire | P2P/HELLO, audit, settlement, block/state/PoA and encrypted-content layouts reviewed; DOC-005/006/015/017 gaps and independent interoperability acceptance stay open |
| 03 economics | Current Treasury/System Balance/lease/settlement rules and YAML mirrors | Current flows, lease/settlement wire and YAML mirrors reviewed; DOC-009 corrected. DOC-005/006 accepted design gaps remain open |
| 04 identity/crypto | Actual KEM publication/recovery integration, source/vector limits, privacy | KEM package/capsule construction, discovery and bridge recovery source review done; DOC-007 corrected. Signing-role/vault review, DOC-016 composition/vector review and clean GUI acceptance remain |
| 05 product | One latest Network contract; current Mail/Files versus future multi-recipient/Backup | Pending; DOC-010/012 |
| 06 status/work | Current source/test/live/remaining matrix; archive dated status and old roadmap; only open questions | Pending; DOC-013 |
| 07 operations/governance | Current DEVNET runbook, dated legal/security primary-source review; preserved FAIL reports | Pending review; old cutover already quarantined; DOC-014 |
| 08 validation | Every document classified/indexed, local links, exact protocol mirror checks, CI, manifest | Foundation classification done; full content/protocol checks pending |

Each package ends in its own reviewable commit. Preserve stable normative paths,
all test failures and dated evidence. No consensus/wire/domain/genesis/key/live
state changes are authorized here. Retired migration instructions must never be
replayed. A CURRENT label identifies a document's purpose, not completed review,
implementation, Beta readiness or certification.

Acceptance: no cancelled requirements in active decisions; no active YAML requires
AUTH/Validation; IDs/layouts agree with source; no benchmark supplies Network rate;
current runbooks match existing genesis; one roadmap/delivery ordering; code tests
and live acceptance stay separate; local links resolve; archived content cannot
create tasks; FAIL evidence remains; runtime and live state unchanged by doc work.
