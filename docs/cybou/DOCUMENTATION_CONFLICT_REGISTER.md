# Documentation conflict register

Status: CURRENT
Scope: Source audit at 1f08f8c5, 2026-10-09; documentation refactor tracking.

Tests named below are defining regression sources, not a claim that this
refactor ran protocol suites. No runtime behavior is authorized by this table.

| ID | Document / disputed statement | Actual source / observation | Class | Action / verification / state |
|---|---|---|---|---|
| DOC-001 | Active decisions contain AUTH/Validation/automatic allocation/old pools | DEC-274–DEC-284; state.h, protocol_params.h | Superseded documentation | Removed/partial wording archived; retained current fragments in DEC-246/249/265/272/273. Foundation complete |
| DOC-002 | Supplied plan requires P2P 1–26 | p2p/session.h: baseline now 1–28, usage 27/28 | Stale audit baseline | Preserve 1–28; peer manager unsupported_compact_wire_ids_are_rejected. Plan corrected |
| DOC-003 | STORAGE_ADMISSION: STORAGE_PROOF_REQUEST is 26 | session.h: request 24, proof 23; audit 25/26 | Documentation error | Correct exact IDs in protocol package; peer-manager TLS storage proof tests. Open |
| DOC-004 | Audit transport described as absent | session.cpp; storage_replica_verifier.cpp; cybou_storage_service_tests.cpp | Documentation error | Separate implemented off-chain transport from unimplemented autonomous mutual scheduling/notarization. Architecture boundary clarified; domain audit open |
| DOC-005 | DEC-280 assignment finalized-seed/PoA attestation marked implemented | storage_provider_selector.cpp uses RAND_bytes; storage_service handles local placement | Accepted requirement / implementation gap | Retain accepted target, explicitly mark local CSPRNG vs missing attestation. Decide target/evidence design before code changes. Open |
| DOC-006 | DEC-282 full evidence-root/StorageId/end-time schema marked implemented | storage_lease.h: period, period_start_utc, entries(publication_id,payout_account,amount), poa_signature | Accepted target versus current wire | Mark as target, not current schema; document exact wire separately. Do not add/remove consensus fields in doc refactor. Open |
| DOC-007 | KEM publication/recovery described as future | publication_service, publication_history_index, identity_kem tests | Stale implementation description | Verify each claimed path and update domain/spec mirrors, retaining clean GUI acceptance gaps. Open |
| DOC-008 | poa_chunk_tree.yaml has Validation_transport, four-field HELLO, obsolete quotas | session.h: listen_port fifth field; DEC-284 and storage economy | Stale machine mirror | Repair alongside protocol/economics; no invented version or role. Open |
| DOC-009 | monetary_model.yaml: AUTH allocations/pre-M7 deployment | official_networks.cpp, state.h, protocol_params.h; deployed existing DEVNET | Stale machine mirror | Rewrite treasury/current parameters; preserve immutable material. Open |
| DOC-010 | Mail To/Cc/Bcc appears mandatory Beta though current single-recipient client | 82_MAIL_UI_UX.md; MailService/private schema | Product target ambiguity | Separate implemented Beta flow from future expansion; do not automatically remove adopted requirements. Open |
| DOC-011 | Index lists old desktop delivery plan/current cutover runbook | DESKTOP_BETA_ACCEPTANCE_PLAN supersedes ordering; current binding eee26eca…3665 | Superseded planning/operations | Old plans/cutover archived with non-executable redirects; active product references changed. Complete |
| DOC-012 | Supplied Network contract substitutes maximum throughput for storage capacity | DEC-290, NetworkPage; operator correction: base 5 op/min, current/max separate, capacity/hosted | Stale product audit | Preserve latest explicit operator contract; no historical benchmark UI or inferred ceiling. Current UI package separately validated |
| DOC-013 | Accumulated status/roadmap mix old and current claims | 26_IMPLEMENTATION_STATUS, 22_ROADMAP, 25_OPEN_QUESTIONS | Superseded planning mixed with evidence | Archive dated records; short code/test/live/remaining matrix and remaining-only roadmap. Open |
| DOC-014 | Governance/legal/crypto claims require dated primary review | SECURITY_GOVERNANCE, SECURITY_STANDARDS, 37–43 | Unverified review scope | Verify primary sources in later package; metadata classification does not certify conformity or revalidate law. Open |

An accepted requirement with a code gap stays accepted unless an explicit decision
changes it. Current bytes remain governed by their defining source/normative
layout; this register never silently introduces a migration.
