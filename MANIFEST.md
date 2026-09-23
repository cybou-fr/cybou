# CYBOU Documentation v0.14 — Manifest

v0.14 is the architecture-hardening release and consistency-cleaned baseline.

## Frozen hardening decisions

- MailTx is a first-class CYBOU operation;
- no permanent per-MailTx consensus-state object;
- validators retain required pre-Store MailTx history;
- mass-scale Email is gated on Object Storage;
- deterministic size-aware MailTx fees; priority fees disabled;
- equal validator weight; 4 validators minimum for f=1 target;
- PoT uses deterministic finalized-height epochs;
- Welcome Grant requires a one-time Operator-signed Invite Voucher;
- Operator Authority / Validator / Release / Treasury keys are separate;
- salted content commitment + historical sender-key evidence;
- old Message Store/Relay/Messenger active design has been superseded.

## Inventory

| File | Lines | SHA-256 prefix |
|---|---:|---|
| `AGENTS.md` | 61 | `9dccd126d108ec14` |
| `README.md` | 101 | `186b9e8ffa1d7c31` |
| `docs/cybou/00_VISION.md` | 81 | `c4925e5bda478519` |
| `docs/cybou/01_BASELINE_AND_SCOPE.md` | 59 | `22edcc694bf13c53` |
| `docs/cybou/02_ARCHITECTURE.md` | 76 | `d27de93b60cf46b4` |
| `docs/cybou/03_BITCOIN_DERIVATION.md` | 59 | `c09f5e23e4f58952` |
| `docs/cybou/04_NETWORK_QUARANTINE.md` | 53 | `f13c2052a4373185` |
| `docs/cybou/05_CHAIN_STATE.md` | 78 | `8a51ed16e1d75ced` |
| `docs/cybou/06_BOOTSTRAP_STATE_SYNC.md` | 53 | `ca8b6af15c309c01` |
| `docs/cybou/07_BFT_CONSENSUS.md` | 85 | `b0e020d8d8ea1124` |
| `docs/cybou/08_P2P.md` | 34 | `3f31ca57881d201d` |
| `docs/cybou/09_CRYPTO_PQ.md` | 36 | `417f778d16a8a492` |
| `docs/cybou/10_IDENTITY_NAMES.md` | 65 | `cbbb32a1339f5327` |
| `docs/cybou/11_STORAGE_OBJECTS.md` | 91 | `74483180e2ce078f` |
| `docs/cybou/12_STORAGE_ACCOUNTING_3_TO_1.md` | 100 | `b20c60860eba40f3` |
| `docs/cybou/13_STORAGE_PROOFS_REPAIR.md` | 78 | `169d9dd8ad90ed86` |
| `docs/cybou/14_BACKUP.md` | 57 | `9cadec18446f35c0` |
| `docs/cybou/15_DRIVE.md` | 33 | `aaf0676328b1ddca` |
| `docs/cybou/16_MAIL_PROTOCOL.md` | 64 | `aa4ac347cd8ae4b2` |
| `docs/cybou/17_EMAIL.md` | 86 | `91031b5eeea48fed` |
| `docs/cybou/18_ECONOMICS_FEES.md` | 73 | `28713398e770e247` |
| `docs/cybou/19_SECURITY_THREAT_MODEL.md` | 53 | `25daa9a974d1294a` |
| `docs/cybou/20_PROTOCOL_SERIALIZATION.md` | 68 | `9e58f0076fb66b90` |
| `docs/cybou/21_RELEASE_SECURITY.md` | 40 | `57772fe861d4f333` |
| `docs/cybou/22_ROADMAP.md` | 123 | `ec724d89acf6a558` |
| `docs/cybou/23_MANUAL_ACCEPTANCE.md` | 82 | `8475cee7d5c4023b` |
| `docs/cybou/24_DECISIONS.md` | 159 | `8aa1c38b8b5a7630` |
| `docs/cybou/25_OPEN_QUESTIONS.md` | 73 | `b801fe7c1d09b815` |
| `docs/cybou/26_IMPLEMENTATION_STATUS.md` | 80 | `616c60e6a10b2f08` |
| `docs/cybou/27_ARCHITECTURE_AUDIT_V0_3.md` | 175 | `1dfbeabc5a0f6588` |
| `docs/cybou/28_BITCOIN_DOCUMENTATION_MIGRATION.md` | 221 | `63ba6ee5d01c00dc` |
| `docs/cybou/29_DOCUMENTATION_AUTHORITY.md` | 79 | `fad3b5767d5ffa7d` |
| `docs/cybou/30_REPOSITORY_DOCUMENTATION_CLEANUP.md` | 215 | `05597f40cb045934` |
| `docs/cybou/31_IMPLEMENTATION_DETAIL_GATES.md` | 101 | `020f7185a750cfd8` |
| `docs/cybou/32_UPSTREAM_REFERENCE_SOURCES.md` | 20 | `de2fc2cb65c2bda2` |
| `docs/cybou/33_EMAIL_FIRST_PRODUCT_STRATEGY.md` | 65 | `ee3430d19de85aeb` |
| `docs/cybou/34_MAIL_STATE_AND_RETENTION.md` | 111 | `5a637a214249c9d2` |
| `docs/cybou/35_ROADMAP_CHANGE_EMAIL_FIRST.md` | 44 | `f7ddf5096a47e798` |
| `docs/cybou/36_PROJECT_RENAME_AND_COPYRIGHT_PLAN.md` | 15 | `981e9aee26fbdb69` |
| `docs/cybou/37_FRANCE_EU_GLOBAL_STRATEGY.md` | 96 | `99058ca64593188d` |
| `docs/cybou/38_SOVEREIGNTY_DEFINITION.md` | 32 | `ff24b825809b656a` |
| `docs/cybou/39_GO_TO_MARKET_AND_PILOTS.md` | 66 | `b04c28592379c1db` |
| `docs/cybou/40_REGULATORY_READINESS_FR_EU.md` | 149 | `0a209c5995079a5d` |
| `docs/cybou/41_PUBLIC_SUPPORT_AND_FUNDING.md` | 107 | `08e4ac9e49c8c9dd` |
| `docs/cybou/42_COMPETITIVE_POSITIONING.md` | 57 | `44a9177464721ddc` |
| `docs/cybou/43_STRATEGIC_SOURCE_REGISTER.md` | 116 | `b2c9dc42f9ddbae2` |
| `docs/cybou/44_CRITICAL_SURVIVAL_AUDIT_V0_7.md` | 101 | `4633302324e7aad8` |
| `docs/cybou/45_PROJECT_RENAME_MAP.md` | 290 | `d29235a0809f0af7` |
| `docs/cybou/46_COPYRIGHT_ATTRIBUTION_POLICY.md` | 109 | `dcfbbc4dff8e93e3` |
| `docs/cybou/47_EMAIL_CHAIN_DELIVERY_MODEL.md` | 49 | `f2c58498aaa22d88` |
| `docs/cybou/48_CONSENSUS_STAGING_STRATEGY.md` | 72 | `6af551c899198bfe` |
| `docs/cybou/49_EMAIL_E2EE_HPKE_PQ.md` | 182 | `42d40f5894da8144` |
| `docs/cybou/50_EMAIL_SECURITY_MODEL.md` | 69 | `d78cfa191419f1d6` |
| `docs/cybou/51_EMAIL_PRODUCT_POSITIONING.md` | 75 | `711b20494aa1a78d` |
| `docs/cybou/52_BALANCE_AND_SYSTEM_BALANCE.md` | 142 | `bf1eb1a09d81ab67` |
| `docs/cybou/53_TRUST_AND_NETWORK_LIMITS.md` | 65 | `559405aaab35c821` |
| `docs/cybou/54_EMISSION_AND_MONETARY_POLICY.md` | 141 | `533258bbb0e2215e` |
| `docs/cybou/55_FEE_FLOW_AND_RECYCLING.md` | 89 | `db86ae32f94eab29` |
| `docs/cybou/56_OWNER_OPERATOR_AND_RESILIENCE.md` | 70 | `fe62720eba15712d` |
| `docs/cybou/57_GLOBAL_PROOF_OF_TRUST_POLICY.md` | 60 | `d42d5677d5929377` |
| `docs/cybou/58_CIRCULATION_SIMULATION.md` | 161 | `517c017b1a884783` |
| `docs/cybou/59_DETERMINISTIC_FEE_ROUTER.md` | 95 | `38c144970b0d7e6e` |
| `docs/cybou/60_VALIDATOR_REWARDS_AND_OPERATOR_REVENUE.md` | 74 | `f9568c67a10a8494` |
| `docs/cybou/62_MAIL_REGISTRATION_AND_PROOF_MODEL.md` | 70 | `e23b80c01787fb98` |
| `docs/cybou/63_MAIL_STATE_GROWTH_AND_DISCOVERY.md` | 77 | `8e4a2cc39d14118a` |
| `docs/cybou/64_PRESTORE_MAIL_RETENTION.md` | 78 | `65dc8c973f4f2470` |
| `docs/cybou/65_MAILTX_PROTOCOL_OPERATION.md` | 106 | `78402ab70ee3864a` |
| `docs/cybou/66_BFT_VALIDATOR_SET_HARDENING.md` | 87 | `7317b76767292bc0` |
| `docs/cybou/67_POT_EPOCHS_AND_INVITE_VOUCHERS.md` | 121 | `555fed2a9f766cd3` |
| `docs/cybou/68_OPERATOR_KEY_SEPARATION.md` | 57 | `e299a7003950ab51` |
| `docs/cybou/69_MAIL_EVIDENCE_BUNDLE.md` | 85 | `05f2f7b893c215f5` |
| `docs/cybou/70_INVITE_VOUCHER_SIGNING_GATE.md` | 115 | `fe2bd08574896190` |
| `docs/cybou/UPSTREAM_BASELINE.md` | 35 | `55877f4ce32d689b` |
| `spec/bft_validator_policy.yaml` | 11 | `2b0e131a71004387` |
| `spec/bitcoin_doc_migration.yaml` | 209 | `3d8535234f55471b` |
| `spec/circulation_scenarios.csv` | 74 | `db240cf0d3280644` |
| `spec/cybou_baseline.yaml` | 253 | `88773a18c92a83bb` |
| `spec/email_crypto_profile.yaml` | 40 | `3c15f3e6d4ca02b2` |
| `spec/fee_router.yaml` | 18 | `4c5b247f40c03b49` |
| `spec/fee_router_test_vectors.csv` | 9 | `db9274d10b0a3dfa` |
| `spec/invite_voucher.yaml` | 17 | `1633b6d0d021313c` |
| `spec/mail_protocol.yaml` | 27 | `c03a36b9ef96da99` |
| `spec/market_strategy.yaml` | 42 | `cfa81471775352eb` |
| `spec/monetary_model.yaml` | 51 | `45aa73ad5689b3e1` |
| `spec/operator_keys.yaml` | 22 | `8fc6da6f562b011e` |
| `spec/project_rename_map.yaml` | 128 | `37d46672c907e3fb` |
| `spec/proof_of_trust.yaml` | 28 | `1fc1bf278725885e` |
| `spec/reward_policy.yaml` | 13 | `676e276df9a12277` |
| `spec/survival_gates.yaml` | 51 | `c0ddefcb3ea4f58a` |
| `spec/validator_reward_test_vectors.csv` | 5 | `2f439357448d8aa1` |

Files excluding manifest: 90
