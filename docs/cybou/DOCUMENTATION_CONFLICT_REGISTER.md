# Documentation conflict register

Status: CURRENT
Scope: Foundation audit at 1f08f8c5; P2P/storage/economics audit at 787e18ca, 2026-10-09. Block/state/PoA audit at ee9d721a. Encrypted-content/KEM/recovery source audit at 531dc0da. Signing-role/vault source audit at 75ba7969. Identity operation source audit at 4b606ce1. Remaining domains are tracked below.

Tests named below are defining regression sources, not a claim that this
refactor ran protocol suites. No runtime behavior is authorized by this table.

| ID | Document / disputed statement | Actual source / observation | Class | Action / verification / state |
|---|---|---|---|---|
| DOC-001 | Active decisions contain AUTH/Validation/automatic allocation/old pools | DEC-274–DEC-284; state.h, protocol_params.h | Superseded documentation | Removed/partial wording archived; retained current fragments in DEC-246/249/265/272/273. Foundation complete |
| DOC-002 | Supplied plan requires P2P 1–26 | p2p/session.h: baseline now 1–28, usage 27/28 | Stale audit baseline | Preserve 1–28; peer manager unsupported_compact_wire_ids_are_rejected. Plan corrected |
| DOC-003 | STORAGE_ADMISSION: STORAGE_PROOF_REQUEST is 26 | session.h: request 24, proof 23; audit 25/26 | Documentation error | Corrected request 24/proof 23 and audit 25/26; linked defining source/tests. Complete |
| DOC-004 | Audit transport described as absent | session.cpp; storage_replica_verifier.cpp; cybou_storage_service_tests.cpp | Documentation error | Documented off-chain audit transport/short-tail digest, local settlement helper and absent production caller; autonomous network collection remains open. Documentation correction complete |
| DOC-005 | DEC-280 assignment finalized-seed/PoA attestation marked implemented | storage_provider_selector.cpp uses RAND_bytes; storage_service handles local placement | Accepted requirement / implementation gap | Retain accepted target, explicitly mark local CSPRNG vs missing attestation. Decide target/evidence design before code changes. Open |
| DOC-006 | DEC-282 full evidence-root/StorageId/end-time schema marked implemented | storage_lease.h: period, period_start_utc, entries(publication_id,payout_account,amount), poa_signature | Accepted target versus current wire | Mark as target, not current schema; document exact wire separately. Do not add/remove consensus fields in doc refactor. Open |
| DOC-007 | KEM publication/recovery described as future | publication_service, publication_history_index, identity_kem tests | Stale implementation description | Construction, current/historical lookup, scanning and bridge recovery documented with source/test references; DOC-016 construction review and clean GUI/live acceptance remain open. Documentation correction complete |
| DOC-008 | poa_chunk_tree.yaml has Validation_transport, four-field HELLO, obsolete quotas | session.h: listen_port fifth field; DEC-284 and storage economy | Stale machine mirror | Removed Validation transport/old quotas, added fifth HELLO field and IDs 27/28, lease_periods/current settlement layout. P2P/storage correction complete; chain/crypto/recovery review remains |
| DOC-009 | monetary_model.yaml: AUTH allocations/pre-M7 deployment | official_networks.cpp, state.h, protocol_params.h; deployed existing DEVNET | Stale machine mirror | Rewritten to current Treasury/System Balance/escrow flows; AUTH allocations and pre-M7 deployment removed. Documentation correction complete |
| DOC-010 | Mail To/Cc/Bcc appears mandatory Beta though current single-recipient client | 82_MAIL_UI_UX.md; MailService/private schema | Product target ambiguity | Separate implemented Beta flow from future expansion; do not automatically remove adopted requirements. Open |
| DOC-011 | Index lists old desktop delivery plan/current cutover runbook | DESKTOP_BETA_ACCEPTANCE_PLAN supersedes ordering; current binding eee26eca…3665 | Superseded planning/operations | Old plans/cutover archived with non-executable redirects; active product references changed. Complete |
| DOC-012 | Supplied Network contract substitutes maximum throughput for storage capacity | DEC-290, NetworkPage; operator correction: base 5 op/min, current/max separate, capacity/hosted | Stale product audit | Preserve latest explicit operator contract; no historical benchmark UI or inferred ceiling. Current UI package separately validated |
| DOC-013 | Accumulated status/roadmap mix old and current claims | 26_IMPLEMENTATION_STATUS, 22_ROADMAP, 25_OPEN_QUESTIONS | Superseded planning mixed with evidence | Archive dated records; short code/test/live/remaining matrix and remaining-only roadmap. Open |
| DOC-014 | Governance/legal/crypto claims require dated primary review | SECURITY_GOVERNANCE, SECURITY_STANDARDS, 37–43 | Unverified review scope | Verify primary sources in later package; metadata classification does not certify conformity or revalidate law. Open |
| DOC-015 | State docs retain usage/quota and lease status/remainder fields; finality wording implies arbitrary-depth replacement | state.h/state.cpp have no such fields; state_store.cpp replaces only current head competitors | Documentation error plus recovery scope gap | Snapshot, block commitments, certificate digest and journal namespace corrected. Historical conflict/descendant replay remains open; no runtime change authorized |
| DOC-016 | KEM profile/vector wording conflates adopted HPKE draft-05 target with current capsule wire and earlier draft vectors | identity_kem tests cite concrete-hybrid-KEM 03/04; root_recipient_capsule.cpp implements CYBOU HKDF/AEAD transcript | Crypto composition / acceptance gap | Exact wrapper, zero separator in package commitment, NetworkBinding and endian contexts documented. Independent profile equivalence/composition review remains open; no byte/key/domain change authorized |
| DOC-017 | Tree prose implies global visit/count enforcement and bounded RAM for every traversal | Fetch uses path plus caller visitor; Enumerate keeps O(N) seen set | Documentation overclaim / resource coverage | Builder count, reader visitor responsibility and enumeration memory distinguished. Malformed-tree/caller resource coverage remains a gate |
| DOC-018 | Vault helper comments imply every failure preserves the old file | identity_vault.cpp may fail sync/reopen after successful publish/rename; Promote supports exact-payload retry | Failure-outcome / evidence gap | Documented CYBV/CYID, authenticated reconciliation and limits. Post-publication fault injection, API outcome semantics and full GUI recovery remain open; no file/key/runtime mutation authorized |
| DOC-019 | IdentityRotate prose implies current and next epochs are both serialized; AccountCreate lag can be misread as a wait | protocol_operation.cpp encodes only next epoch; account_creation.cpp accepts current/preceding work epoch | Documentation error | Exact operation/registry bodies, tag versus kind, double-domain digests and allowed work-age window documented. Source comparison complete; independent vectors/product acceptance separate |

DOC-005 follow-up (2026-10-09): deterministic off-chain assignment preparation
and immutable encrypted plan retention now exist in storage_assignment. They
retain a caller-supplied eligible snapshot and finalized seed, with separate
payout-identity grouping and replayable commitment. Production placement remains
CSPRNG; proof collection, PoA attestation and evidence/settlement integration are
not implemented by this helper. DOC-005 and DOC-006 remain open.

Further follow-up: off-chain PoA attestation, both payout-binding signature checks,
raw binding retention and assignment-scoped receipt verification now have tested
primitives. Production eligibility/provenance validation, dispatch and assignment
attestation in canonical settlement remain absent; DOC-005/006 are still open.

Assignment-scoped observation follow-up: a transport boundary now verifies the
attested slot/receipt before random-offset audit or exact full GET. It returns
instantaneous local observations, without interval credit, persistent raw evidence
or production aggregation. DOC-005/006 remain open.

Durable collector follow-up: assigned observations can now be retained/replayed
in app.db with immutable records and an atomic bounded index. Receipt and raw
audit response are rechecked against local ciphertext; GET success/time is a
trusted local assertion. Production collection/aggregation, interval derivation
and canonical settlement integration remain absent; DOC-005/006 remain open.

Cross-epoch accounting follow-up: shared funded-slot claims now reject overlapping
intervals/proof reuse across replacement assignments and commit atomically with
local interval records. This does not establish audit interval policy, active
epoch authority, canonical lease provenance or settlement activation. DOC-005/006
and the rounding/state transition gap DOC-020 remain open.

Payout preparation follow-up: stored interval journals now feed a read-only
single-slot cumulative target quote with complete chunk/epoch resolution,
provider-specific floors and canonical-paid inputs. No current wire/state rules
or live payout path were changed; provenance, policy and activation gates keep
DOC-005/006/020 open.

Historical-binding follow-up: payout preparation now rechecks retained STORAGE
and Authorization signatures using the registry snapshot for each assignment seed.
Snapshot finalized provenance is still caller-verified, and production/state
activation is absent; DOC-005/006/020 remain open.

Full-term quote follow-up: all funded slots now share one read-only preparation,
global entry limit and escrow check; overlapping claims across slots cannot use
the same storage/economic identity. Physical independence, production provenance,
audit policy and canonical activation remain unproven; DOC-005/006/020 stay open.

### DOC-020 — rent accrual versus period payout ceiling (2026-10-09)

Classification: accepted economic target / implementation gap. Whole-interval
floor accrual with carried remainder is not enforced by the current canonical
period-cap check in storage_lease.cpp. For one billing unit and two replicas,
30-period escrow and one-period cap both equal one CYBOU; all escrow can therefore
be paid in the first eligible period, despite floor accrual over 30 periods being
zero. The economics quote regression reproduces the arithmetic discrepancy.
The dated simulation describes model accrual/refunds separately from actual
settlements. A cumulative cap/remainder and deterministic provider rounding
require an explicit consensus specification/cutover decision. Open; no deployed
rent, settlement wire or state arithmetic changed in this package.

Follow-up ECONOMICS-P0-01 selects separate funded replica shares and cumulative
verified-service entitlement (DEC-292). Pure target arithmetic and explicit
preparation overflow rejection are implemented. Canonical term/assignment/evidence
inputs and cumulative paid enforcement remain open; DOC-020 is not marked closed.

Approved isolated-development follow-up: canonical initial/renewal funding in
the source tree now reserves N separately rounded replica shares through the
existing ComputeStorageLeaseEscrow call sites. One unit/two replicas/30 periods
reserves two CYBOU. The regression still demonstrates premature day-one payment
under the unchanged daily cap (one paid, one remains), so this partial transition
must not be deployed and does not close DOC-020. The earlier one-CYBOU escrow
description above records the audited deployed baseline, not the new development
funding formula. Separate canonical terms, paid/evidence and settlement remain open.

An accepted requirement with a code gap stays accepted unless an explicit decision
changes it. Current bytes remain governed by their defining source/normative
layout; this register never silently introduces a migration.
