# CYBOU Documentation Index

This directory contains the normative specifications, product contracts, and
architecture registers for the CYBOU project.

## Authority and precedence hierarchy

Architecture and protocol implementations must strictly adhere to the following
order of precedence. A lower level cannot introduce protocol behavior absent
from or conflicting with higher levels:

- **LEVEL 0 — Implementation authority**: [`AGENTS.md`](../../AGENTS.md)
- **LEVEL 1 — Frozen architecture and decisions**: [`24_DECISIONS.md`](24_DECISIONS.md), [`02_ARCHITECTURE.md`](02_ARCHITECTURE.md)
- **LEVEL 2 — Normative domain specifications**: Core protocol, consensus, state, storage, and identity models
- **LEVEL 3 — Mutable implementation truth**: [`26_IMPLEMENTATION_STATUS.md`](26_IMPLEMENTATION_STATUS.md)
- **LEVEL 4 — Roadmap and unresolved work**: [`22_ROADMAP.md`](22_ROADMAP.md), [`25_OPEN_QUESTIONS.md`](25_OPEN_QUESTIONS.md)
- **LEVEL 5 — Product and UX contracts**: Mail, Files, Design System, Application Data Plane
- **LEVEL 6 — Business, legal, and sovereign strategy**: Positioning, France/EU policy, compliance
- **LEVEL 7 — Machine-readable mirrors**: `spec/*`
- **LEVEL 8 — Public projection**: `README.md`, `www/*`, `www/llms.txt`

---

## Document index and classification

| Document | Description | Hierarchy level | Status |
|---|---|---|---|
| [`00_VISION.md`](00_VISION.md) | High-level vision and product principles | Level 5 | Product target |
| [`02_ARCHITECTURE.md`](02_ARCHITECTURE.md) | High-level system architecture and data plane | Level 1 | Frozen |
| [`04_NETWORK_LIFECYCLE.md`](04_NETWORK_LIFECYCLE.md) | Official network profiles, bootstrap lifecycle, wipe semantics, and key rotation | Level 2 | Active |
| [`05_CHAIN_STATE.md`](05_CHAIN_STATE.md) | Deterministic state execution, accounts, and names | Level 2 | Active |
| [`06_BOOTSTRAP_STATE_SYNC.md`](06_BOOTSTRAP_STATE_SYNC.md) | Verified state synchronization over CYBOU P2P | Level 2 | Active |
| [`08_P2P.md`](08_P2P.md) | CYBOU P2P transport and peer management | Level 2 | Active |
| [`09_CRYPTO_PQ.md`](09_CRYPTO_PQ.md) | Post-quantum cryptography profile and key roles | Level 2 | Active |
| [`10_IDENTITY_NAMES.md`](10_IDENTITY_NAMES.md) | Protocol Identity, key derivation, and `.cybou` names | Level 2 | Active |
| [`18_ECONOMICS_FEES.md`](18_ECONOMICS_FEES.md) | Native asset, Balance, System Balance, and direct Central Authority fees | Level 2 | Active |
| [`19_SECURITY_THREAT_MODEL.md`](19_SECURITY_THREAT_MODEL.md) | System threat model, attacker assumptions, and boundaries | Level 2 | Active |
| [`20_PROTOCOL_SERIALIZATION.md`](20_PROTOCOL_SERIALIZATION.md) | Deterministic wire formats and bounded binary schemas | Level 2 | Active |
| [`21_RELEASE_SECURITY.md`](21_RELEASE_SECURITY.md) | Release signing and distribution integrity | Level 2 | Active |
| [`22_ROADMAP.md`](22_ROADMAP.md) | Protocol and product roadmap | Level 4 | Active |
| [`24_DECISIONS.md`](24_DECISIONS.md) | Frozen architecture decisions and superseded history | Level 1 | Frozen |
| [`25_OPEN_QUESTIONS.md`](25_OPEN_QUESTIONS.md) | Open architectural trade-offs and research questions | Level 4 | Active |
| [`26_IMPLEMENTATION_STATUS.md`](26_IMPLEMENTATION_STATUS.md) | Implemented code truth, prototype gap, and test evidence | Level 3 | Implementation status |
| [`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`](37_FRANCE_SOVEREIGN_NETWORK_POLICY.md) | France-first sovereign P2P policy and local Geo boundaries | Level 6 | Active |
| [`38_SOVEREIGNTY_DEFINITION.md`](38_SOVEREIGNTY_DEFINITION.md) | Technological sovereignty criteria | Level 6 | Active |
| [`39_GO_TO_MARKET_AND_PILOTS.md`](39_GO_TO_MARKET_AND_PILOTS.md) | Go-to-market strategy, organizations, and pilot rollout | Level 6 | Active |
| [`40_REGULATORY_READINESS_FR_EU.md`](40_REGULATORY_READINESS_FR_EU.md) | Regulatory readiness: CRA, GDPR, NIS 2, MiCA | Level 6 | Active |
| [`41_PUBLIC_SUPPORT_AND_FUNDING.md`](41_PUBLIC_SUPPORT_AND_FUNDING.md) | Public research and funding alignment | Level 6 | Active |
| [`42_COMPETITIVE_POSITIONING.md`](42_COMPETITIVE_POSITIONING.md) | Strategic positioning vs incumbents and alternatives | Level 6 | Active |
| [`43_STRATEGIC_SOURCE_REGISTER.md`](43_STRATEGIC_SOURCE_REGISTER.md) | Register of authoritative sources and references | Level 6 | Active |
| [`46_COPYRIGHT_ATTRIBUTION_POLICY.md`](46_COPYRIGHT_ATTRIBUTION_POLICY.md) | IP, attribution, and license compliance | Level 6 | Active |
| [`50_EMAIL_SECURITY_MODEL.md`](50_EMAIL_SECURITY_MODEL.md) | End-to-end encrypted email privacy model | Level 5 | Product target |
| [`52_BALANCE_AND_SYSTEM_BALANCE.md`](52_BALANCE_AND_SYSTEM_BALANCE.md) | Balance and System Balance semantics | Level 2 | Active |
| [`56_OWNER_OPERATOR_AND_RESILIENCE.md`](56_OWNER_OPERATOR_AND_RESILIENCE.md) | Operational resilience and operator key handling | Level 6 | Active |
| [`57_IDENTITY_AUTHORITY.md`](57_IDENTITY_AUTHORITY.md) | Canonical AUTH account value, issuance/burn and Validation eligibility | Level 2 | Active |
| [`68_OPERATOR_KEY_SEPARATION.md`](68_OPERATOR_KEY_SEPARATION.md) | PoA and operator key role custody | Level 2 | Active |
| [`70_ACCOUNT_CREATION_ANTI_SYBIL.md`](70_ACCOUNT_CREATION_ANTI_SYBIL.md) | Permissionless AccountCreate, anti-Sybil work, and OnboardingPool | Level 2 | Active |
| [`71_WINDOWS_MINGW_BUILD.md`](71_WINDOWS_MINGW_BUILD.md) | Local Windows MinGW + vcpkg build procedure | — | Active |
| [`72_DESKTOP_WALLET_UI.md`](72_DESKTOP_WALLET_UI.md) | Desktop wallet presentation contract | Level 5 | Product target |
| [`73_CORE_DESKTOP_CONTRACT.md`](73_CORE_DESKTOP_CONTRACT.md) | Contract between native NodeRuntime and desktop client | Level 5 | Active |
| [`76_IDENTITY_VAULT_RECOVERY.md`](76_IDENTITY_VAULT_RECOVERY.md) | Encrypted CYID/CYBV vault structure and phrase recovery | Level 2 | Active |
| [`77_CYBOU_NAME_REGISTRY.md`](77_CYBOU_NAME_REGISTRY.md) | Commit/work/reveal `.cybou` name registry rules | Level 2 | Active |
| [`78_IDENTITY_DESKTOP_UX.md`](78_IDENTITY_DESKTOP_UX.md) | Identity creation and restore desktop UI/UX | Level 5 | Product target |
| [`80_CRYPTO_SOVEREIGNTY_AUDIT.md`](80_CRYPTO_SOVEREIGNTY_AUDIT.md) | Cryptography and sovereignty compliance audit | Level 6 | Active |
| [`81_BETA_PRODUCT_SCOPE.md`](81_BETA_PRODUCT_SCOPE.md) | Scope and acceptance boundaries for Beta release | Level 5 | Product target |
| [`82_MAIL_UI_UX.md`](82_MAIL_UI_UX.md) | Mail client UX contract and interaction patterns | Level 5 | Product target |
| [`83_STORAGE_UI_UX.md`](83_STORAGE_UI_UX.md) | Files client UX contract and interaction patterns | Level 5 | Product target |
| [`84_PRODUCT_DESIGN_SYSTEM.md`](84_PRODUCT_DESIGN_SYSTEM.md) | Desktop design tokens, components, and patterns | Level 5 | Product target |
| [`85_BETA_UI_ACCEPTANCE.md`](85_BETA_UI_ACCEPTANCE.md) | Acceptance test scenarios for desktop Beta | Level 5 | Product target |
| [`86_IDENTITY_SECURITY_SUBSTRATE.md`](86_IDENTITY_SECURITY_SUBSTRATE.md) | Identity security substrate and key store boundaries | Level 2 | Active |
| [`87_IDENTITY_OPERATION_COORDINATOR.md`](87_IDENTITY_OPERATION_COORDINATOR.md) | In-flight identity operation serialization and retry | Level 2 | Active |
| [`89_IDENTITY_KEM_PUBLICATION.md`](89_IDENTITY_KEM_PUBLICATION.md) | Public KEM package commitment and epoch publication | Level 2 | Active |
| [`APPLICATION_DATA_PLANE.md`](APPLICATION_DATA_PLANE.md) | Application services, chunk retention, and local views | Level 5 | Active |
| [`P2P_TRANSPORT.md`](P2P_TRANSPORT.md) | CYBOU P2P transport protocol specification | Level 2 | Active |
| [`ENCRYPTED_CHUNK_TREE.md`](ENCRYPTED_CHUNK_TREE.md) | Encrypted ROOT/INDEX/DATA chunk tree structure | Level 2 | Active |
| [`IDENTITY_DISCOVERY_AND_RECOVERY.md`](IDENTITY_DISCOVERY_AND_RECOVERY.md) | Clean-machine identity restore and publication scanning | Level 2 | Active |
| [`DEVNET_DEVELOPMENT.md`](DEVNET_DEVELOPMENT.md) | Existing DEVNET, operator commands and development checks | — | Active |
| [`POA_FINALITY.md`](POA_FINALITY.md) | Genesis-authorized single-operator PoA, anti-equivocation journal | Level 2 | Active |
| [`POA_FINALITY_VECTORS.md`](POA_FINALITY_VECTORS.md) | PoA block and certificate test vectors | Level 2 | Active |
| [`ROOT_PUBLICATION.md`](ROOT_PUBLICATION.md) | Canonical RootPublication wire format and validation | Level 2 | Active |
| [`STORAGE_ADMISSION.md`](STORAGE_ADMISSION.md) | Merkle inclusion proofs, chunk admission, provider policy | Level 2 | Active |
| [`UPSTREAM_BASELINE.md`](UPSTREAM_BASELINE.md) | Upstream Bitcoin Core baseline delta and removal plan | — | Historical |
| [`VALIDATION.md`](VALIDATION.md) | Validation signatures after independent execution, eligibility (> 1M AUTH) | Level 2 | Active |
| [`future/VALIDATION.md`](future/VALIDATION.md) | Archived historical note on early advisory validation design | Future | Historical |
