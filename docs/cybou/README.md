# CYBOU documentation index

Status: CURRENT
Scope: Document authority/classification catalogue; classification checked 2026-10-09.

Start with [AGENTS](../../AGENTS.md), [architecture](02_ARCHITECTURE.md),
[current decisions](24_DECISIONS.md), [implementation status](26_IMPLEMENTATION_STATUS.md),
and [roadmap](22_ROADMAP.md). Current desktop acceptance ordering is
[DESKTOP_BETA_ACCEPTANCE_PLAN](DESKTOP_BETA_ACCEPTANCE_PLAN.md).
The old delivery plans and completed cutover are historical records only.

## Meaning and precedence

CURRENT is a current contract/specification/plan, not proof it is implemented.
PROPOSAL is an idea with no implementation authorization. EVIDENCE is a dated
revision/scenario result. HISTORICAL is cancelled/completed material and must
never create implementation tasks or operational commands.

Applicable law/adopted security requirements govern under
[Security governance](SECURITY_GOVERNANCE.md); internal precedence is:
0 AGENTS; 1 architecture/decisions; 2 normative domains; 3 implementation status;
4 roadmap/open work; 5 product/UX; 6 business/legal strategy; 7 machine mirrors;
8 public projection. Lower levels cannot introduce higher-level architecture.
An accepted target/code gap is recorded and resolved, not silently rewritten.

The [conflict register](DOCUMENTATION_CONFLICT_REGISTER.md) identifies verified
source discrepancies. The [refactor plan](DOCUMENTATION_REFACTOR_PLAN.md) records
content review still pending; classification alone does not revalidate all claims.

## Architecture and decisions

| Document | Status |
|---|---|
| [CYBOU architecture](02_ARCHITECTURE.md) | CURRENT |
| [Current product and protocol decisions](24_DECISIONS.md) | CURRENT |

## Protocol and domains

| Document | Status |
|---|---|
| [04 — Network lifecycle](04_NETWORK_LIFECYCLE.md) | CURRENT |
| [Canonical chain state](05_CHAIN_STATE.md) | CURRENT |
| [06 — Bootstrap and verified state sync](06_BOOTSTRAP_STATE_SYNC.md) | CURRENT |
| [Peer-to-peer transport target](08_P2P.md) | CURRENT |
| [Post-quantum cryptography profile](09_CRYPTO_PQ.md) | CURRENT |
| [Identity and `.cybou` names](10_IDENTITY_NAMES.md) | CURRENT |
| [CYBOU economics and fees](18_ECONOMICS_FEES.md) | CURRENT |
| [CYBOU security threat model](19_SECURITY_THREAT_MODEL.md) | CURRENT |
| [20 — Protocol serialization](20_PROTOCOL_SERIALIZATION.md) | CURRENT |
| [52 — Balance and System Balance](52_BALANCE_AND_SYSTEM_BALANCE.md) | CURRENT |
| [68 — Operator key separation](68_OPERATOR_KEY_SEPARATION.md) | CURRENT |
| [70 — Account creation and anti-Sybil work](70_ACCOUNT_CREATION_ANTI_SYBIL.md) | CURRENT |
| [76 — Identity vault and recovery](76_IDENTITY_VAULT_RECOVERY.md) | CURRENT |
| [77 — `.cybou` name registry](77_CYBOU_NAME_REGISTRY.md) | CURRENT |
| [86 — Identity security substrate](86_IDENTITY_SECURITY_SUBSTRATE.md) | CURRENT |
| [87 — Identity operation coordinator](87_IDENTITY_OPERATION_COORDINATOR.md) | CURRENT |
| [89 — Identity KEM capability publication](89_IDENTITY_KEM_PUBLICATION.md) | CURRENT |
| [Encrypted chunk tree](ENCRYPTED_CHUNK_TREE.md) | CURRENT |
| [Identity publication discovery and recovery](IDENTITY_DISCOVERY_AND_RECOVERY.md) | CURRENT |
| [CYBOU P2P transport](P2P_TRANSPORT.md) | CURRENT |
| [PoA finality target](POA_FINALITY.md) | CURRENT |
| [RootPublication](ROOT_PUBLICATION.md) | CURRENT |
| [Finalized chunk storage admission and durability](STORAGE_ADMISSION.md) | CURRENT |

## Implementation and work

| Document | Status |
|---|---|
| [CYBOU protocol and product roadmap](22_ROADMAP.md) | CURRENT |
| [Open engineering and product gates](25_OPEN_QUESTIONS.md) | CURRENT |
| [Implementation status](26_IMPLEMENTATION_STATUS.md) | CURRENT |
| [Beta clean-client Files recovery — FILES-RESTORE-01](BETA_FILES_CLEAN_RESTORE_ACCEPTANCE.md) | CURRENT |
| [Beta Identity session acceptance — ID-SESSION-01](BETA_IDENTITY_SESSION_ACCEPTANCE.md) | CURRENT |
| [Beta offline Mail attachment acceptance — MAIL-OFFLINE-01](BETA_MAIL_OFFLINE_ACCEPTANCE.md) | CURRENT |
| [Beta Wallet uncertain-delivery acceptance — WALLET-RESTART-01](BETA_WALLET_RESTART_ACCEPTANCE.md) | CURRENT |
| [Desktop Beta Acceptance & Performance Evidence](DESKTOP_BETA_ACCEPTANCE_PLAN.md) | CURRENT |
| [Documentation conflict register](DOCUMENTATION_CONFLICT_REGISTER.md) | CURRENT |
| [Documentation refactor delivery plan](DOCUMENTATION_REFACTOR_PLAN.md) | CURRENT |

## Product and UX

| Document | Status |
|---|---|
| [CYBOU vision](00_VISION.md) | CURRENT |
| [50 — Mail and Files content security](50_EMAIL_SECURITY_MODEL.md) | CURRENT |
| [72 — Desktop Wallet UI](72_DESKTOP_WALLET_UI.md) | CURRENT |
| [73 — Core → Desktop contract](73_CORE_DESKTOP_CONTRACT.md) | CURRENT |
| [78 — Identity desktop UX contract](78_IDENTITY_DESKTOP_UX.md) | CURRENT |
| [81 — Beta product scope](81_BETA_PRODUCT_SCOPE.md) | CURRENT |
| [82 — CYBOU Mail UI/UX](82_MAIL_UI_UX.md) | CURRENT |
| [83 — CYBOU Files UI/UX](83_STORAGE_UI_UX.md) | CURRENT |
| [84 — CYBOU product design system](84_PRODUCT_DESIGN_SYSTEM.md) | CURRENT |
| [85 — CYBOU Beta UI/UX acceptance](85_BETA_UI_ACCEPTANCE.md) | CURRENT |
| [CYBOU application data plane](APPLICATION_DATA_PLANE.md) | CURRENT |
| [Network and Advanced product contract](NETWORK_AND_ADVANCED_UX.md) | CURRENT |
| [Built-in Network overview](NETWORK_OBSERVABILITY_PLAN.md) | CURRENT |

## Operations and governance

| Document | Status |
|---|---|
| [21 — Release and supply-chain security](21_RELEASE_SECURITY.md) | CURRENT |
| [37 — France-first sovereign P2P policy](37_FRANCE_SOVEREIGN_NETWORK_POLICY.md) | CURRENT |
| [38 — CYBOU sovereignty definition](38_SOVEREIGNTY_DEFINITION.md) | CURRENT |
| [39 — Go-to-market and pilot strategy](39_GO_TO_MARKET_AND_PILOTS.md) | CURRENT |
| [40 — France/EU regulatory readiness](40_REGULATORY_READINESS_FR_EU.md) | CURRENT |
| [41 — French and European support / funding map](41_PUBLIC_SUPPORT_AND_FUNDING.md) | CURRENT |
| [42 — Competitive positioning](42_COMPETITIVE_POSITIONING.md) | CURRENT |
| [43 — Strategic source register](43_STRATEGIC_SOURCE_REGISTER.md) | CURRENT |
| [46 — Copyright, license and attribution policy](46_COPYRIGHT_ATTRIBUTION_POLICY.md) | CURRENT |
| [56 — Owner, operator, and network trust](56_OWNER_OPERATOR_AND_RESILIENCE.md) | CURRENT |
| [71 — Сборка CYBOU core (Windows, Qt MinGW + vcpkg)](71_WINDOWS_MINGW_BUILD.md) | CURRENT |
| [CYBOU dependency boundary](80_CRYPTO_SOVEREIGNTY_AUDIT.md) | CURRENT |
| [Data, key and retention inventory](DATA_PROCESSING_INVENTORY.md) | CURRENT |
| [DEVNET development](DEVNET_DEVELOPMENT.md) | CURRENT |
| [Diagnostic logging, retention and support export](LOGGING_RETENTION_AND_EXPORT.md) | CURRENT |
| [Security, privacy and resilience governance](SECURITY_GOVERNANCE.md) | CURRENT |
| [Security standards and evidence](SECURITY_STANDARDS.md) | CURRENT |

## History and evidence

| Document | Status |
|---|---|
| [CYBOU battle test 20261005-180752](battle/20261005-180752.md) | EVIDENCE |
| [CYBOU battle test 20261005-223338](battle/20261005-223338.md) | EVIDENCE |
| [CYBOU battle test 20261006-134809](battle/20261006-134809.md) | EVIDENCE |
| [CYBOU battle test 20261006-212338](battle/20261006-212338.md) | EVIDENCE |
| [CYBOU battle test 20261006-225825](battle/20261006-225825.md) | EVIDENCE |
| [CYBOU battle test 20261007-141346](battle/20261007-141346.md) | EVIDENCE |
| [CYBOU battle test 20261007-195003](battle/20261007-195003.md) | EVIDENCE |
| [CYBOU battle test 20261007-203645](battle/20261007-203645.md) | EVIDENCE |
| [CYBOU battle test 20261007-205703](battle/20261007-205703.md) | EVIDENCE |
| [Data assurance and erasure review](DATA_ASSURANCE_AND_ERASURE.md) | EVIDENCE |
| [Desktop UI/UX delivery plan](DESKTOP_UX_DELIVERY_PLAN.md) | HISTORICAL |
| [Remaining desktop UI/UX work — 2026-10-06](DESKTOP_UX_REMAINING_WORK_2026-10-06.md) | HISTORICAL |
| [DEVNET live content acceptance — 2026-10-04](DEVNET_LIVE_ACCEPTANCE.md) | EVIDENCE |
| [DEVNET storage-economy cutover (M7) — operator runbook](DEVNET_STORAGE_ECONOMY_CUTOVER.md) | HISTORICAL |
| [Prior implementation authority](history/AGENTS-2026-10-09.md) | HISTORICAL |
| [Superseded CYBOU decisions](history/decisions/2026-10-09-superseded.md) | HISTORICAL |
| [Desktop UI/UX delivery plan](history/DESKTOP_UX_DELIVERY_PLAN.md) | HISTORICAL |
| [Remaining desktop UI/UX work — 2026-10-06](history/DESKTOP_UX_REMAINING_WORK_2026-10-06.md) | HISTORICAL |
| [DEVNET storage-economy cutover (M7) — operator runbook](history/DEVNET_STORAGE_ECONOMY_CUTOVER.md) | HISTORICAL |
| [PoA finality interoperability vectors](POA_FINALITY_VECTORS.md) | EVIDENCE |

## Machine-readable mirrors and other entry points

[spec](../../spec/) mirrors current normative sources; it creates no independent
architecture. Content corrections remain pending in protocol/economics packages.
[Core desktop integration](../../src/qt/CORE_INTEGRATION.md) is a current code boundary.
[Battle reference JSON](battle/benchmark_reference.json) is historical test evidence,
not a GUI resource, live rate or throughput maximum. Dated FAIL reports are retained.
