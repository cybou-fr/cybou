# CYBOU Documentation v0.0.1 — Manifest

v0.0.1 is the architecture-hardening release and consistency-cleaned baseline.

## Frozen hardening decisions

- MailTx is a first-class CYBOU operation;
- no permanent per-MailTx consensus-state object;
- validators retain required pre-Store MailTx history;
- mass-scale Email is gated on Object Storage;
- deterministic size-aware MailTx fees; priority fees disabled;
- equal validator weight; 4 validators minimum for f=1 target;
- PoT uses deterministic finalized-height epochs;
- permissionless account creation with anti-Sybil proof-of-work;
- Operator Authority / Validator / Release / Treasury keys are separate;
- salted content commitment + historical sender-key evidence;
- old Message Store/Relay/Messenger active design has been superseded.

## Inventory

| File | Lines | SHA-256 prefix |
|---|---:|---|
| AGENTS.md | 67 | 723af94fbd372300 |
| README.md | 101 | 40f012dfbdb87e98 |
| docs\cybou\00_VISION.md | 81 | c4925e5bda478519 |
| docs\cybou\01_BASELINE_AND_SCOPE.md | 59 | 22edcc694bf13c53 |
| docs\cybou\02_ARCHITECTURE.md | 76 | d27de93b60cf46b4 |
| docs\cybou\03_BITCOIN_DERIVATION.md | 59 | c09f5e23e4f58952 |
| docs\cybou\04_NETWORK_QUARANTINE.md | 53 | f13c2052a4373185 |
| docs\cybou\05_CHAIN_STATE.md | 78 | 8a51ed16e1d75ced |
| docs\cybou\06_BOOTSTRAP_STATE_SYNC.md | 53 | ca8b6af15c309c01 |
| docs\cybou\07_BFT_CONSENSUS.md | 85 | b0e020d8d8ea1124 |
| docs\cybou\08_P2P.md | 34 | 3f31ca57881d201d |
| docs\cybou\09_CRYPTO_PQ.md | 36 | 417f778d16a8a492 |
| docs\cybou\10_IDENTITY_NAMES.md | 70 | b767813861575917 |
| docs\cybou\11_STORAGE_OBJECTS.md | 91 | 74483180e2ce078f |
| docs\cybou\12_STORAGE_ACCOUNTING_3_TO_1.md | 100 | b20c60860eba40f3 |
| docs\cybou\13_STORAGE_PROOFS_REPAIR.md | 78 | 169d9dd8ad90ed86 |
| docs\cybou\14_BACKUP.md | 57 | 9cadec18446f35c0 |
| docs\cybou\15_DRIVE.md | 33 | aaf0676328b1ddca |
| docs\cybou\16_MAIL_PROTOCOL.md | 64 | aa4ac347cd8ae4b2 |
| docs\cybou\17_EMAIL.md | 86 | 91031b5eeea48fed |
| docs\cybou\18_ECONOMICS_FEES.md | 75 | 95d533abd9db183b |
| docs\cybou\19_SECURITY_THREAT_MODEL.md | 53 | 25daa9a974d1294a |
| docs\cybou\20_PROTOCOL_SERIALIZATION.md | 90 | ed6d627b2f03ec16 |
| docs\cybou\21_RELEASE_SECURITY.md | 40 | 57772fe861d4f333 |
| docs\cybou\22_ROADMAP.md | 128 | 83d208696397e0a7 |
| docs\cybou\23_MANUAL_ACCEPTANCE.md | 82 | f2aa303209b24341 |
| docs\cybou\24_DECISIONS.md | 161 | a04682b6342ffb98 |
| docs\cybou\25_OPEN_QUESTIONS.md | 91 | 2139bbd9b09809d4 |
| docs\cybou\26_IMPLEMENTATION_STATUS.md | 121 | caf11f99453d6170 |
| docs\cybou\27_ARCHITECTURE_AUDIT_V0_3.md | 175 | 7b3cdc6050c7cf85 |
| docs\cybou\28_BITCOIN_DOCUMENTATION_MIGRATION.md | 221 | 63ba6ee5d01c00dc |
| docs\cybou\29_DOCUMENTATION_AUTHORITY.md | 79 | fad3b5767d5ffa7d |
| docs\cybou\30_REPOSITORY_DOCUMENTATION_CLEANUP.md | 215 | 05597f40cb045934 |
| docs\cybou\31_IMPLEMENTATION_DETAIL_GATES.md | 101 | 020f7185a750cfd8 |
| docs\cybou\32_UPSTREAM_REFERENCE_SOURCES.md | 20 | de2fc2cb65c2bda2 |
| docs\cybou\33_EMAIL_FIRST_PRODUCT_STRATEGY.md | 65 | ee3430d19de85aeb |
| docs\cybou\34_MAIL_STATE_AND_RETENTION.md | 111 | 072aa5ca428f57f6 |
| docs\cybou\35_ROADMAP_CHANGE_EMAIL_FIRST.md | 44 | f7ddf5096a47e798 |
| docs\cybou\36_PROJECT_RENAME_AND_COPYRIGHT_PLAN.md | 15 | 981e9aee26fbdb69 |
| docs\cybou\37_FRANCE_EU_GLOBAL_STRATEGY.md | 96 | 99058ca64593188d |
| docs\cybou\38_SOVEREIGNTY_DEFINITION.md | 32 | ff24b825809b656a |
| docs\cybou\39_GO_TO_MARKET_AND_PILOTS.md | 66 | b04c28592379c1db |
| docs\cybou\40_REGULATORY_READINESS_FR_EU.md | 149 | 0a209c5995079a5d |
| docs\cybou\41_PUBLIC_SUPPORT_AND_FUNDING.md | 107 | 08e4ac9e49c8c9dd |
| docs\cybou\42_COMPETITIVE_POSITIONING.md | 57 | 44a9177464721ddc |
| docs\cybou\43_STRATEGIC_SOURCE_REGISTER.md | 116 | b2c9dc42f9ddbae2 |
| docs\cybou\44_CRITICAL_SURVIVAL_AUDIT_V0_7.md | 101 | e6c2c86615489c2a |
| docs\cybou\45_PROJECT_RENAME_MAP.md | 290 | d29235a0809f0af7 |
| docs\cybou\46_COPYRIGHT_ATTRIBUTION_POLICY.md | 109 | dcfbbc4dff8e93e3 |
| docs\cybou\47_EMAIL_CHAIN_DELIVERY_MODEL.md | 49 | f2c58498aaa22d88 |
| docs\cybou\48_CONSENSUS_STAGING_STRATEGY.md | 72 | 6af551c899198bfe |
| docs\cybou\49_EMAIL_E2EE_HPKE_PQ.md | 182 | 42d40f5894da8144 |
| docs\cybou\50_EMAIL_SECURITY_MODEL.md | 69 | d78cfa191419f1d6 |
| docs\cybou\51_EMAIL_PRODUCT_POSITIONING.md | 75 | 711b20494aa1a78d |
| docs\cybou\52_BALANCE_AND_SYSTEM_BALANCE.md | 142 | f611db804aa22ed2 |
| docs\cybou\53_TRUST_AND_NETWORK_LIMITS.md | 65 | 65feb3d66936d7c4 |
| docs\cybou\54_EMISSION_AND_MONETARY_POLICY.md | 132 | a5d66001fe327a98 |
| docs\cybou\55_FEE_FLOW_AND_RECYCLING.md | 89 | ec724df5498eea68 |
| docs\cybou\56_OWNER_OPERATOR_AND_RESILIENCE.md | 70 | fe62720eba15712d |
| docs\cybou\57_GLOBAL_PROOF_OF_TRUST_POLICY.md | 60 | d42d5677d5929377 |
| docs\cybou\58_CIRCULATION_SIMULATION.md | 161 | 748386018bbb7875 |
| docs\cybou\59_DETERMINISTIC_FEE_ROUTER.md | 95 | 38c144970b0d7e6e |
| docs\cybou\60_VALIDATOR_REWARDS_AND_OPERATOR_REVENUE.md | 74 | f9568c67a10a8494 |
| docs\cybou\62_MAIL_REGISTRATION_AND_PROOF_MODEL.md | 70 | e23b80c01787fb98 |
| docs\cybou\63_MAIL_STATE_GROWTH_AND_DISCOVERY.md | 77 | 7865ff3352cbf80a |
| docs\cybou\64_PRESTORE_MAIL_RETENTION.md | 78 | 65dc8c973f4f2470 |
| docs\cybou\65_MAILTX_PROTOCOL_OPERATION.md | 106 | 78402ab70ee3864a |
| docs\cybou\66_BFT_VALIDATOR_SET_HARDENING.md | 87 | f7575b006c5cf2dd |
| docs\cybou\67_POT_EPOCHS_AND_ONBOARDING.md | 111 | 32105739bf8b16ff |
| docs\cybou\68_OPERATOR_KEY_SEPARATION.md | 81 | 8b9bb7e6d34e12bb |
| docs\cybou\69_MAIL_EVIDENCE_BUNDLE.md | 85 | 05f2f7b893c215f5 |
| docs\cybou\70_ACCOUNT_CREATION_ANTI_SYBIL.md | 76 | 7edcc23067843fbe |
| docs\cybou\71_WINDOWS_MINGW_BUILD.md | 119 | 9fae942d8ee560b8 |
| docs\cybou\UPSTREAM_BASELINE.md | 34 | 55877f4ce32d689b |
| spec\bft_validator_policy.yaml | 11 | 1f1934f0f3ea0776 |
| spec\bitcoin_code_removal.yaml | 153 | 5807adaa909df625 |
| spec\bitcoin_doc_migration.yaml | 209 | 3d8535234f55471b |
| spec\circulation_scenarios.csv | 74 | db240cf0d3280644 |
| spec\cybou_baseline.yaml | 253 | 3072207f130205a3 |
| spec\email_crypto_profile.yaml | 40 | 75512b33cf03cfe0 |
| spec\fee_router.yaml | 18 | ccebf298d9eb9ec0 |
| spec\fee_router_test_vectors.csv | 9 | db9274d10b0a3dfa |
| spec\mail_protocol.yaml | 27 | 104e9cb288c55e92 |
| spec\market_strategy.yaml | 42 | 9421f65346373f9e |
| spec\monetary_model.yaml | 53 | b3f4881bb25221dd |
| spec\onboarding.yaml | 30 | 4a12ead846d1a692 |
| spec\operator_keys.yaml | 44 | 5b3e36d8930ba646 |
| spec\project_rename_map.yaml | 128 | 37d46672c907e3fb |
| spec\proof_of_trust.yaml | 28 | 6f2e15b465174431 |
| spec\reward_policy.yaml | 13 | 676e276df9a12277 |
| spec\survival_gates.yaml | 51 | c0ddefcb3ea4f58a |
| spec\validator_reward_test_vectors.csv | 5 | 2f439357448d8aa1 |

Files excluding manifest: 92
