# 24 — Protocol and product decisions

| ID | Decision | Status |
|---|---|---|
| DEC-001 | Product/network name is CYBOU | Frozen |
| DEC-002 | Native asset is `CYBOU` | Frozen for development |
| DEC-003 | Internal human identity namespace is `.cybou` | Frozen for development |
| DEC-004 | Example identity form is `stan.cybou` | Superseded by DEC-165; four-character label is invalid in V2 |
| DEC-005 | Windows executable is `cybou.exe` | Frozen |
| DEC-006 | Desktop is Qt 6 + modern C++ + CMake | Frozen |
| DEC-007 | Desktop application is itself a full node | Frozen |
| DEC-008 | No desktop REST/JSON-RPC/gRPC/local HTTP API | Frozen |
| DEC-009 | Bitcoin Core is starting C++ codebase, not network dependency | Frozen |
| DEC-010 | First manual desktop run only after Bitcoin network quarantine | Frozen |
| DEC-011 | Ordinary desktop nodes use bounded history/state sync | Frozen |
| DEC-012 | Ledger remains UTXO-derived; EVM not required | Superseded by DEC-160 |
| DEC-013 | Target consensus is BFT with explicit finality; validator admission is separate | Frozen |
| DEC-014 | Full node and validator roles are distinct | Frozen |
| DEC-015 | Storage is cooperative, not a host-price marketplace | Frozen |
| DEC-016 | Target storage ratio is 3:1 verified contribution : logical entitlement | Frozen target |
| DEC-017 | 3:1 is not triple replication | Frozen |
| DEC-018 | Storage accounting uses verified byte-time | Frozen direction |
| DEC-019 | Per-shard obligations stay off-chain; accounting is aggregated | Frozen direction |
| DEC-020 | Storage objects/manifests are encrypted/opaque | Frozen |
| DEC-021 | Erasure profile is parameterized | Frozen |
| DEC-022 | Backup as first user-facing application | Superseded |
| DEC-023 | Drive follows Backup | Superseded by DEC-185 |
| DEC-024 | Separate email-like product | Superseded |
| DEC-025 | Messaging Core is shared messaging substrate | Superseded by v0.0.1 |
| DEC-026 | Crypto is agile and PQ/hybrid capable | Frozen |
| DEC-027 | No custom cryptographic primitives | Frozen |
| DEC-028 | Normal chain transactions have non-zero anti-spam fees | Frozen direction |
| DEC-029 | User content does not go on-chain | Frozen |
| DEC-030 | All-keys-lost recovery is unresolved | Frozen truth |
| DEC-031 | CYBOU docs override conflicting inherited Bitcoin operational docs | Frozen |
| DEC-032 | Bitcoin docs are migrated incrementally with code | Frozen |
| DEC-033 | Required Bitcoin/third-party copyright notices are preserved | Frozen |
| DEC-034 | Active CYBOU docs do not advertise unsupported inherited features | Frozen |
| DEC-035 | Documentation migration is part of milestone Definition of Done | Frozen |
| DEC-036 | CYBOU Email is first user-facing product | Frozen roadmap |
| DEC-037 | Email v1 uses bounded Message Store, not general Object Storage | Superseded by v0.0.1 |
| DEC-038 | Message Store has TTL/quota/replication but no erasure coding or 3:1 | Superseded by v0.0.1 |
| DEC-039 | General Object Storage follows Email v1 | Superseded by DEC-173 |
| DEC-040 | Backup follows general Storage durability/accounting | Updated by DEC-177; Backup is post-Beta |
| DEC-041 | Large Messenger attachments are deferred until Object Storage exists | Superseded by DEC-173 |
| DEC-042 | Advanced Messenger/group features come after core Messenger and Backup | Superseded by v0.0.1 |
| DEC-043 | Project rename/copyright migration is an explicit audited pass | Planned |
| DEC-044 | Existing Bitcoin/third-party copyright notices are preserved | Frozen |
| DEC-045 | CYBOU notices are additive on new/substantially modified files | Frozen direction |
| DEC-046 | Company/R&D origin is France | Strategic baseline |
| DEC-047 | Network/product market strategy is Europe-first | Frozen strategy |
| DEC-048 | Protocol remains globally open where legally permitted | Frozen strategy |
| DEC-049 | Do not position CYBOU as France-only network | Frozen strategy |
| DEC-050 | Do not launch globally as first commercial market | Frozen strategy |
| DEC-051 | Public positioning: European sovereign P2P network designed in France | Frozen strategy |
| DEC-052 | First adoption unit is an organization/team, not isolated consumers | Frozen GTM |
| DEC-053 | No public token sale/ICO/listing objective before useful network exists | Frozen early policy |
| DEC-054 | No public/open channels in Email v1 | Frozen risk reduction |
| DEC-055 | CRA/security incident readiness begins before production | Frozen |
| DEC-056 | French cryptography/export classification is required before public commercial distribution | Frozen compliance gate |
| DEC-057 | Funding is acceleration, not a product dependency | Frozen |
| DEC-058 | Email Alpha precedes production-complete validator-admission economics | Frozen survival rule |
| DEC-059 | Consensus stages: Operator Validator -> static multi-validator BFT -> resilient independent-validator BFT | Frozen |
| DEC-060 | French organizational pilot requires static BFT minimum | Frozen |
| DEC-061 | Relay/rendezvous-capable routing is required; direct P2P is optional | Superseded by v0.13 |
| DEC-062 | No unreviewed custom Messenger E2EE session protocol | Superseded by v0.13 |
| DEC-063 | `.cybou` handle is optional alias over AccountID | Superseded by v0.13 |
| DEC-064 | Global people search is not required for first pilot | Superseded by v0.13 |
| DEC-065 | Public Bitcoin names/resources/config paths are removed early; internal cosmetic renames are deferred | Superseded by v0.13 |
| DEC-066 | Initial Windows consumer package ships `cybou.exe` only unless another binary is explicitly approved | Superseded by v0.13 |
| DEC-067 | First pilot uses controlled contact/invite onboarding to reduce spam surface | Superseded by v0.13 |
| DEC-068 | Desktop resource budgets are measured from early Messenger builds | Superseded by v0.0.1 |
| DEC-069 | First user-facing product is CYBOU Email, not Messenger | Superseded by v0.0.1 |
| DEC-070 | CYBOU Email v1 is CYBOU-native and does not require SMTP/IMAP/POP | Superseded by v0.13 |
| DEC-071 | All CYBOU-native email is E2E encrypted; no plaintext fallback | Frozen |
| DEC-072 | [SUPERSEDED BY DEC-193] Email content is encrypted once with a random CEK; CEK was HPKE-wrapped per recipient device | Historical direction |
| DEC-073 | Preferred PQ/T HPKE target is X25519 + ML-KEM-768, subject to standards/final implementation review | Frozen target |
| DEC-074 | No custom PQ KEM combiner | Frozen |
| DEC-075 | Subject/body/thread-sensitive metadata remain inside E2E protected content where possible | Frozen |
| DEC-076 | Sender authenticity is separate from HPKE key wrapping and remains crypto-agile/PQ-capable | Frozen |
| DEC-077 | A future SMTP gateway is optional and must be treated as a separate trust/security boundary | Frozen |
| DEC-078 | Email v1 uses safe content rendering; arbitrary active HTML is not required | Frozen |
| DEC-079 | Account exposes `Balance` and `System Balance`; both hold real CYBOU | Frozen |
| DEC-080 | Balance debits require user authorization; no arbitrary network/admin debit | Frozen |
| DEC-081 | System Balance is irreversible/non-transferable/non-withdrawable and pays protocol-defined fees | Frozen |
| DEC-082 | Onboarding Bonus is credited fully and immediately to System Balance; no vesting | Superseded / Updated to Onboarding Bonus |
| DEC-083 | Onboarding Bonus CYBOU directly contributes to Trust through System Balance; no separate invite-trust token/credit | Superseded / Updated to Onboarding Bonus |
| DEC-084 | New onboarded account Email limit target is approximately 20–30 recipient deliveries/day | Superseded by DEC-150 |
| DEC-085 | Recipient-delivery count, not UI-message count, is used for Email sending limits | Superseded by v0.0.1 |
| DEC-086 | Trust can raise limits but personal-account limits always retain a hard ceiling | Frozen |
| DEC-087 | System Balance never creates validator voting power | Frozen |
| DEC-088 | Monetary model is fixed maximum supply with bootstrap pools and fee recycling | Frozen |
| DEC-089 | Protocol fees are not burned by default | Frozen current policy |
| DEC-090 | Onboarding Bonus is funded from protocol/onboarding supply, not arbitrary mint-on-account-creation | Superseded by DEC-151 |
| DEC-091 | Pre-mainnet economic simulation is mandatory even though v0.10 numeric baseline is frozen for implementation | Frozen process rule |
| DEC-092 | `MAX_SUPPLY = 100,000,000,000 CYBOU` | Frozen v0.10 baseline |
| DEC-093 | CYBOU has zero decimal places; 1 CYBOU is the minimum unit | Frozen |
| DEC-094 | Genesis allocation: 25% Owner/Operator, 25% Onboarding, 20% Network Ops/Validators, 15% Product Reserve, 15% Strategic Reserve | Frozen v0.10 baseline |
| DEC-095 | Owner/Developer/Operator allocation is 25,000,000,000 CYBOU with no protocol founder vesting | Frozen v0.10 baseline |
| DEC-096 | CYBOU is a commercial owner/operator service, not a DAO/foundation/community-owned treasury | Frozen |
| DEC-097 | Decentralization target is operational resilience without mandatory CYBOU cloud, not decentralized business ownership | Frozen |
| DEC-098 | Current owner/operator is the development validator and remains an operator validator in production | Frozen direction |
| DEC-099 | Production operator validator may receive normal deterministic validator rewards for verified work | Frozen direction |
| DEC-100 | Dev onboarding bonus is 6,000 CYBOU; Beta and Mainnet amounts determined separately | Superseded by DEC-150 / DEC-151 |
| DEC-101 | Standard Email modeling unit targets approximately 1 CYBOU per recipient delivery | Superseded by v0.0.1 |
| DEC-102 | New onboarded account Email ceiling starts at 25 recipient deliveries/day | Superseded by DEC-137 |
| DEC-103 | Proof of Trust is global across Email, payments and all CYBOU services | Frozen |
| DEC-104 | Each service maps global PoT to its own bounded limits | Frozen |
| DEC-105 | PoT may limit outgoing payment velocity/volume but cannot confiscate or reassign Balance | Frozen |
| DEC-106 | PoT never creates BFT voting power | Frozen |
| DEC-107 | Stake-weighted PoS is optional validator admission, not mandatory CYBOU consensus identity | Frozen |
| DEC-108 | Recurrent protocol fee split is 35% Security / 40% Network Services / 25% Onboarding | Superseded by v0.13 |
| DEC-109 | Fee split is settled in exact 20-CYBOU integer batches: 7 / 8 / 5 | Superseded by v0.13 |
| DEC-110 | Unsettled fee remainder stays in consensus `PendingFeePool` and is never rounded away | Superseded by v0.13 |
| DEC-111 | There is no default fee burn | Superseded by v0.13 |
| DEC-112 | There is no permanent owner/operator protocol tax; recurring operator revenue is work-based | Superseded by v0.13 |
| DEC-113 | Operator may earn validator and service rewards when it performs verified work | Superseded by v0.13 |
| DEC-114 | Genesis Product/Strategic Reserves replace the need for a recurring reserve fee share | Superseded by v0.13 |
| DEC-115 | Service reward claims require verifiable accounting; self-reported bandwidth/capacity is insufficient | Superseded by v0.13 |
| DEC-116 | Validator rewards use equal split among eligible validators | Superseded by v0.13 |
| DEC-117 | Validator reward eligibility requires >= 90% consensus participation and no confirmed equivocation | Superseded by v0.13 |
| DEC-118 | v1 validator rewards are not stake-weighted | Superseded by v0.13 |
| DEC-119 | Relay Node weight = 1 service point | Superseded by v0.0.1 |
| DEC-120 | Mailbox Node weight = 2 service points | Superseded by v0.0.1 |
| DEC-121 | Service reward eligibility requires >= 95% uptime plus challenge pass and valid registration | Superseded by v0.0.1 |
| DEC-122 | Relay/Mailbox rewards are not based on raw message count, claimed bytes or self-reported traffic | Superseded by v0.0.1 |
| DEC-123 | Only registered service nodes participate in Network Service rewards | Superseded by v0.0.1 |
| DEC-124 | Service-node bond is not required in v1 | Superseded by v0.0.1 |
| DEC-125 | Reward payout uses epochs; exact epoch duration remains implementation-tunable | Superseded by v0.0.1 |
| DEC-126 | Integer payout remainder stays in the corresponding reward pool for the next epoch | Superseded by v0.0.1 |
| DEC-127 | CYBOU Email is a consensus-registered mail protocol, not a realtime messenger | Frozen v0.13 |
| DEC-128 | v1 MailTx stores E2E encrypted text in block data | Frozen v0.13 |
| DEC-129 | current state stores compact MailMarker data, not full email ciphertext | Superseded by v0.0.1 |
| DEC-130 | v1 has no Email Relay, Mailbox Store or ServiceNodeRegistry | Frozen v0.13 |
| DEC-131 | recipient availability is not required; receiving occurs through normal state/block synchronization | Frozen v0.13 |
| DEC-132 | Initial Mail profile is text-only; attachments are disabled until Store exists | Frozen initial profile; Beta extension required by DEC-173 |
| DEC-133 | after Object Storage, encrypted mail body/attachments move to Store and MailTx carries content commitments/references | Updated by DEC-175 |
| DEC-134 | v1 recurrent fee split is 75% Validators / 25% Onboarding | Frozen v0.13 |
| DEC-135 | v1 Fee Router uses exact 4-CYBOU batches: 3 Security / 1 Onboarding | Frozen v0.13 |
| DEC-136 | no rewarded generic service-node role exists before Object Storage | Frozen v0.13 |
| DEC-137 | new onboarded account baseline is 25 outgoing MailTx/day | Frozen v0.0.1 |
| DEC-138 | finalized MailTx provides cryptographic registration/origin/integrity evidence but is not automatically a legal notarial act | Frozen v0.13 |
| DEC-139 | Permanent per-MailTx consensus-state MailMarker is rejected | Frozen v0.0.1 |
| DEC-140 | MailTx existence/authenticity is proven by typed operation + block inclusion + BFT finality, not permanent bounded mail-validation state | Frozen v0.0.1 |
| DEC-141 | Before Store, active validators retain canonical historical MailTx bodies required for retrieval | Frozen v0.0.1 |
| DEC-142 | Broad mass-scale Email is gated on Object Storage or equivalent durable content layer | Updated by DEC-173; Store is already required for Beta |
| DEC-143 | MailTx is a first-class CYBOU protocol operation, not OP_RETURN/Bitcoin Script application data | Frozen v0.0.1 |
| DEC-144 | MailTx fees are deterministic and size-aware; strict max serialized MailTx size required | Frozen v0.0.1 |
| DEC-145 | Priority fee / fee bidding is disabled in v1 | Frozen v0.0.1 |
| DEC-146 | All active v1 validators have equal consensus weight = 1 | Frozen v0.0.1 |
| DEC-147 | Four active validators is the minimum deployment target when claiming f=1 BFT tolerance | Frozen v0.0.1 |
| DEC-148 | PoT time/rate logic uses deterministic block-height-derived protocol epochs, never local wall clock | Frozen v0.0.1 |
| DEC-149 | PoT consensus arithmetic is integer/deterministic | Frozen v0.0.1 |
| DEC-150 | Account creation is permissionless via protocol-native AccountCreateOp with AccountCreationWorkV1 anti-Sybil work | Frozen v0.0.1 |
| DEC-151 | Identity creation alone does not mint tokens; onboarding bonus is debited from OnboardingPool to SystemBalance | Frozen v0.0.1 |
| DEC-152 | Operator Authority, Validator, Release Signing and Treasury keys are separate domains | Frozen v0.0.1 |
| DEC-153 | Operator Authority target custody is 2-of-3 | Frozen operational direction |
| DEC-154 | MailEvidenceBundle must prove historical sender-key authorization at MailTx height | Frozen v0.0.1 |
| DEC-155 | Mail content commitment uses random salt/domain separation; no bare predictable plaintext hash | Frozen v0.0.1 |
| DEC-156 | AccountID V1 is an opaque stable 32-byte identifier; all-zero is invalid and key rotation does not change it | Frozen v0.0.1 |
| DEC-157 | AccountCreationWork network_id V1 is the 32-byte genesis block hash | Superseded by DEC-161 |
| DEC-158 | CybouStateStore solely owns canonical CYBOU state; BFT-finalized blocks use candidate-validate-atomic-commit and have no production rollback/reorg/undo path | Frozen v0.0.1 |
| DEC-159 | Canonical finalized head stores block ID and height; every committed child height equals previous finalized height + 1 | Frozen v0.0.1 |
| DEC-160 | CYBOU target ledger is account-based with AccountState as the sole balance/authorization/nonce source; inherited UTXO/Script remains transitional only until native Payment and BFT replacement land | Frozen direction |
| DEC-161 | NetworkID is the domain-separated hash of an immutable network definition binding genesis block, genesis state root, protocol parameters and initial validator-set commitment | Extended by DEC-163 |
| DEC-162 | Canonical state rejects unsupported or structurally invalid network definitions before initialization, loading or block transition | Frozen v0.0.1 |
| DEC-163 | The immutable network definition also binds the fixed Operator Authority keyset and every consensus MailTx quota parameter; Operator Authority rotation requires a versioned transition, and disposable DEV genesis must reset after this serialization change | Frozen Beta hardening |
| DEC-164 | Beta MailTx quota is one network-bound limit per account and epoch; SystemBalance pays service fees but does not increase quota | Frozen Beta hardening |
| DEC-165 | Identity V2 AccountID is random nonzero 256-bit, independent of mnemonic and keys; `stanislav.cybou` replaces the four-character example | Frozen V2 target |
| DEC-166 | A 24-word recovery phrase deterministically yields replaceable hybrid Ed25519 AND ML-DSA-65 Recovery Root; consensus binds RecoveryKeyID to AccountID | Frozen V2 target; encoding/vectors pending |
| DEC-167 | [SUPERSEDED BY DEC-193] Bounded device registry used hybrid Ed25519 AND ML-DSA-44 authorization, independent per-device nonces, and add/revoke operations | Historical direction; superseded by DEC-193 |
| DEC-168 | Portable CYBV2 vault uses random DEK with AES-256-GCM and password-derived Argon2id KEK; DPAPI is optional local unlock only | Frozen V2 target; parameters pending |
| DEC-169 | `.cybou` V1 labels are 5–32 lowercase ASCII bytes with explicit reservations; one permanent primary name per AccountID, no transfer/expiry/recycling | Frozen V2 target |
| DEC-170 | Name claims require finalized commit, network-bound work, and reveal; first valid finalized reveal wins | Frozen V2 target; bounds pending |
| DEC-171 | Save and verify encrypted recovery vault before AccountCreate broadcast; create and restore are equal desktop entry paths | Frozen V2 target |
| DEC-172 | Integrate versioned Identity V2 authorization and names, then perform one intentional disposable CYBOU-DEV reset | Frozen migration direction |
| DEC-173 | CYBOU Beta requires Object Storage-backed encrypted Mail attachments; text-only DEV/Alpha is transitional and does not satisfy Beta | Frozen product scope |
| DEC-174 | Initial Mail profile remains one-recipient, text-only, and attachment-free for DEV/Alpha integration | Frozen initial profile |
| DEC-175 | Beta attachment bytes stay off-chain/in consensus state; MailTx carries only required commitments/references and manifest/content remain E2E protected | Frozen product/security boundary; wire fields pending separate protocol review |
| DEC-176 | Storage providers receive ciphertext only; attachment filename, MIME, path, subject, and content keys remain private | Frozen product/security boundary |
| DEC-177 | Backup and Drive are post-Beta applications; Backup is included in integrated Email + Storage + Backup onboarding-budget economics | Product scope superseded by DEC-185; economics superseded by DEC-191 |
| DEC-178 | Beta Storage readiness includes chunking, distributed placement, leases, audits, repair, retrieval, and accounting; operational parameters are not set by DEC-173 | Frozen capability gate; parameters pending |
| DEC-179 | Gmail and Google Drive are interaction references for Mail and Files; CYBOU does not copy their branding or centralized-provider model | Frozen product UX direction |
| DEC-180 | Normal CYBOU UI presents human actions and outcomes; protocol, BFT, PQ, and Storage mechanics use progressive disclosure | Frozen product UX direction |
| DEC-181 | Mail containing attachments is not submitted through the normal Send path until each required attachment reaches the protocol-defined minimum Storage durability state | Frozen Beta UX/security boundary |
| DEC-182 | `delivery_uncertain` is distinct from rejection; retry or replacement requires reconciling the original operation and nonce reservation | Frozen cross-service correctness direction |
| DEC-183 | Mail and Files share the protected Object Storage layer; Save to Files should reuse an existing protected object/reference when ownership, privacy, and retention rules allow | Frozen Beta integration direction |
| DEC-184 | Network, cryptographic, and Storage work must not block the Qt event loop; pages issue asynchronous requests and render core/model state | Frozen desktop UX direction |
| DEC-185 | Files is the Beta file-management product surface; there is no separate Drive product milestone, while Backup remains a post-Beta application | Frozen product scope; Beta calibration superseded by DEC-191 |
| DEC-186 | [SUPERSEDED BY DEC-193] Identity is the security root for account-authorized Name, Wallet, Mail, and Files actions; every such submission uses the shared Identity operation coordinator | Superseded by DEC-193; Mail and Files integration remains pending |
| DEC-187 | [SUPERSEDED BY DEC-193] An account-authorized action must not allocate its own nonce, sign independently, or retry with replacement bytes; the coordinator journals exact bytes and operation identity until reconciliation | Superseded by DEC-193; one unresolved operation per Identity journal |
| DEC-188 | [SUPERSEDED BY DEC-193] Identity signing keys and recipient content-encryption/KEM keys are separate domains; recipient KEM capabilities are published through Identity, not a parallel Mail key registry | Superseded by DEC-193; DEV publishes one draft-05 X-Wing package per AccountID/key_epoch |
| DEC-189 | Bulk object content is encrypted with an approved AEAD and recipient access is conveyed by a standardized hybrid classical + PQ KEM profile | Target security architecture; suite, wire format, and key package require protocol review |
| DEC-190 | Saving a Mail attachment to Files creates an independent Files retention/ownership reference; Mail retention or deletion does not revoke that Files reference | Frozen product direction; implementation pending Object Storage and Mail integration |
| DEC-191 | Calibrate the Beta onboarding budget from measured Mail and Files/Storage usage; model Backup separately as a post-Beta capacity scenario | Frozen calibration scope; Mainnet bonus remains gated on aggregate Beta operating data |
| DEC-192 | Keep the local Storage Key Ring as an encrypted AccountID-bound CYBV2 sidecar until the account KEM package supports reviewed SMK wrapping and clean-machine restore | Provisional local key-custody implementation; account-level distribution and recovery remain open |

## Superseding Identity decision

| DEC-193 | Remove device as a protocol-level Identity entity. Derive Recovery (Ed25519 + ML-DSA-65), Authorization (Ed25519 + ML-DSA-44), and X-Wing KEM roles from mnemonic entropy under separate domains. Identity state contains one current key set, KEM commitment, shared nonce, and key_epoch. IdentityRotate atomically replaces every role; restore derives the current key set locally and does not submit DeviceAdd. Mail targets one AccountID/key_epoch capsule. | Current canonical direction; requires direct DEV cutover and discarding obsolete state/vaults |


## Product architecture decision

| DEC-194 | Mail, Files, and later Backup are product surfaces over one CYBOU Identity, finalized state, and shared encrypted immutable Object Layer. Mail UX targets Gmail's core workflows; Files targets Google Drive's core workflows without copying brand assets or centralized-provider assumptions. Beta attachments are Storage objects referenced from E2E-protected Mail content; bytes and Storage topology stay out of MailTx. Files uses a private encrypted catalog and account-root commitment, not per-file consensus state. Object keys are independent from message CEKs. Save to Files and Files-to-Mail reuse authorized object bytes with independent ownership/retention references. Sharing is AccountID/Identity-KEM based; anonymous links are not the default. Initial DEV/Alpha Mail remains one-recipient text-only with no attachments; Beta targets one or more recipients with one KEM capsule per AccountID/key_epoch so familiar reply-all flows are possible. | Frozen product architecture; recipient limits, sharing wire/state, durability, and Storage economics remain separate protocol gates |

## Protocol reset decision

| DEC-195 | At the next coordinated DEV protocol cutover, replace BFT/ValidatorSet and application-specific MailTx/Files-root publication with a genesis-bound single-operator hybrid-PQ PoA finalizer, generic RootPublication, and one randomized encrypted content-addressed Chunk DAG. Full nodes independently validate every block and state transition. ChunkID is full BLAKE3(stored ciphertext); bounded canonical CBOR keeps application schemas and graph links encrypted. Providers admit chunks only with finalized-publication Merkle proofs. No runtime compatibility, dual decoder, or partial cutover. | Frozen target architecture; PoA fork/equivocation recovery, wire profiles, generic limits/fees, storage durability, and clean-machine recovery are mandatory pre-reset gates |
| DEC-196 | Replace whole-file encrypted DAG vectors with a streaming immutable ordered ROOT/INDEX/DATA tree. ROOT and INDEX use private canonical CBOR; DATA is raw application bytes. Crypto APIs consume/return bytes. Remove the 256 MiB per-file ceiling; bound per-chunk size, tree depth/fan-out/count and uint64 counters. RootPublication wire profile 2 publishes only root_chunk_id, chunk_authorization_root, chunk_count, a provider-enforced authorized_stored_bytes ceiling, and recipient capsules; it never includes the complete chunk-ID set. Keep per-chunk Merkle inclusion proofs. | Superseded by DEC-197 for tree metadata and publication/Merkle fields. |

| DEC-197 | Encrypted ROOT/INDEX metadata stores one child_kind and an ordered ChunkID array; no per-child byte lengths. Empty and metadata-only content uses a ROOT with zero children. Merkle leaves commit only to ChunkID because ChunkID is the full BLAKE3 of stored ciphertext. Admission proofs contain only leaf_index and sibling hashes; PUT_CHUNK and finalized RootPublication provide the ChunkID and leaf count. Remove the redundant authorized_stored_bytes publication ceiling; each provider enforces its own capacity. Fee is `root_publication_fee_per_started_kib * started_kib + root_publication_fee_per_chunk * chunk_count`, committed by immutable network parameters; DEV defaults are four and four. Each four-unit fee splits 3:1. RootPublication wire profile is 3, with no compatibility decoder. | Frozen simplification for the coordinated protocol cutover; cross-implementation vectors and integrated storage remain gates. |
| DEC-198 | Remove MailTx and all mail-specific consensus machinery: per-account quota counters, MailEvidenceBundle, and block discovery filters. Mail uses encrypted Object Storage objects and client-owned Inbox/Sent/read indexes; no per-message consensus object, MailTx fee, or consensus quota. Treat this as a breaking protocol reset: bump wire/network domains, discard old DEV state at the coordinated reset, and ship no legacy decoder. | Frozen reset direction; encrypted mailbox indexing, delivery receipts, abuse controls, and integrated Email/Storage/Backup economics remain implementation gates. |
