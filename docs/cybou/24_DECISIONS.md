# 24 — Protocol and product decisions

| ID | Decision | Status |
|---|---|---|
| DEC-001 | Product/network name is CYBOU | Frozen |
| DEC-002 | Native asset is `CYBOU` | Frozen for development |
| DEC-003 | Internal human identity namespace is `.cybou` | Frozen for development |
| DEC-004 | Example identity form is `stan.cybou` | Frozen |
| DEC-005 | Windows executable is `cybou.exe` | Frozen |
| DEC-006 | Desktop is Qt 6 + modern C++ + CMake | Frozen |
| DEC-007 | Desktop application is itself a full node | Frozen |
| DEC-008 | No desktop REST/JSON-RPC/gRPC/local HTTP API | Frozen |
| DEC-009 | Bitcoin Core is starting C++ codebase, not network dependency | Frozen |
| DEC-010 | First manual desktop run only after Bitcoin network quarantine | Frozen |
| DEC-011 | Ordinary desktop nodes use bounded history/state sync | Frozen |
| DEC-012 | Ledger remains UTXO-derived; EVM not required | Frozen |
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
| DEC-023 | Drive follows Backup | Frozen |
| DEC-024 | Separate email-like product | Superseded |
| DEC-025 | Messaging Core is shared messaging substrate | Superseded by v0.14 |
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
| DEC-037 | Email v1 uses bounded Message Store, not general Object Storage | Superseded by v0.14 |
| DEC-038 | Message Store has TTL/quota/replication but no erasure coding or 3:1 | Superseded by v0.14 |
| DEC-039 | General Object Storage follows Email v1 | Frozen roadmap |
| DEC-040 | Backup follows general Storage durability/accounting | Frozen roadmap |
| DEC-041 | Large Messenger attachments are deferred until Object Storage exists | Superseded by v0.14 |
| DEC-042 | Advanced Messenger/group features come after core Messenger and Backup | Superseded by v0.14 |
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
| DEC-068 | Desktop resource budgets are measured from early Messenger builds | Superseded by v0.14 |
| DEC-069 | First user-facing product is CYBOU Email, not Messenger | Superseded by v0.14 |
| DEC-070 | CYBOU Email v1 is CYBOU-native and does not require SMTP/IMAP/POP | Superseded by v0.13 |
| DEC-071 | All CYBOU-native email is E2E encrypted; no plaintext fallback | Frozen |
| DEC-072 | Email content is encrypted once with a random CEK; CEK is HPKE-wrapped per recipient device | Frozen direction |
| DEC-073 | Preferred PQ/T HPKE target is X25519 + ML-KEM-768, subject to standards/final implementation review | Frozen target |
| DEC-074 | No custom PQ KEM combiner | Frozen |
| DEC-075 | Subject/body/thread-sensitive metadata remain inside E2E protected content where possible | Frozen |
| DEC-076 | Sender authenticity is separate from HPKE key wrapping and remains crypto-agile/PQ-capable | Frozen |
| DEC-077 | A future SMTP gateway is optional and must be treated as a separate trust/security boundary | Frozen |
| DEC-078 | Email v1 uses safe content rendering; arbitrary active HTML is not required | Frozen |
| DEC-079 | Account exposes `Balance` and `System Balance`; both hold real CYBOU | Frozen |
| DEC-080 | Balance debits require user authorization; no arbitrary network/admin debit | Frozen |
| DEC-081 | System Balance is irreversible/non-transferable/non-withdrawable and pays protocol-defined fees | Frozen |
| DEC-082 | Invite Bonus is credited fully and immediately to System Balance; no vesting | Frozen |
| DEC-083 | Invite Bonus CYBOU directly contributes to Trust through System Balance; no separate invite-trust token/credit | Frozen |
| DEC-084 | New invited account Email limit target is approximately 20–30 recipient deliveries/day | Frozen direction |
| DEC-085 | Recipient-delivery count, not UI-message count, is used for Email sending limits | Superseded by v0.14 |
| DEC-086 | Trust can raise limits but personal-account limits always retain a hard ceiling | Frozen |
| DEC-087 | System Balance never creates validator voting power | Frozen |
| DEC-088 | Monetary model is fixed maximum supply with bootstrap pools and fee recycling | Frozen |
| DEC-089 | Protocol fees are not burned by default | Frozen current policy |
| DEC-090 | Invite Bonus is funded from protocol/onboarding supply, not arbitrary mint-on-account-creation | Frozen |
| DEC-091 | Pre-mainnet economic simulation is mandatory even though v0.10 numeric baseline is frozen for implementation | Frozen process rule |
| DEC-092 | `MAX_SUPPLY = 100,000,000,000 CYBOU` | Frozen v0.10 baseline |
| DEC-093 | CYBOU has zero decimal places; 1 CYBOU is the minimum unit | Frozen |
| DEC-094 | Genesis allocation: 25% Owner/Operator, 25% Onboarding, 20% Network Ops/Validators, 15% Product Reserve, 15% Strategic Reserve | Frozen v0.10 baseline |
| DEC-095 | Owner/Developer/Operator allocation is 25,000,000,000 CYBOU with no protocol founder vesting | Frozen v0.10 baseline |
| DEC-096 | CYBOU is a commercial owner/operator service, not a DAO/foundation/community-owned treasury | Frozen |
| DEC-097 | Decentralization target is operational resilience without mandatory CYBOU cloud, not decentralized business ownership | Frozen |
| DEC-098 | Current owner/operator is the development validator and remains an operator validator in production | Frozen direction |
| DEC-099 | Production operator validator may receive normal deterministic validator rewards for verified work | Frozen direction |
| DEC-100 | `WELCOME_GRANT = 6,000 CYBOU` paid fully to System Balance on valid invite | Frozen v0.10 baseline |
| DEC-101 | Standard Email modeling unit targets approximately 1 CYBOU per recipient delivery | Superseded by v0.14 |
| DEC-102 | New invited account Email ceiling starts at 25 recipient deliveries/day | Frozen v0.10 baseline |
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
| DEC-119 | Relay Node weight = 1 service point | Superseded by v0.14 |
| DEC-120 | Mailbox Node weight = 2 service points | Superseded by v0.14 |
| DEC-121 | Service reward eligibility requires >= 95% uptime plus challenge pass and valid registration | Superseded by v0.14 |
| DEC-122 | Relay/Mailbox rewards are not based on raw message count, claimed bytes or self-reported traffic | Superseded by v0.14 |
| DEC-123 | Only registered service nodes participate in Network Service rewards | Superseded by v0.14 |
| DEC-124 | Service-node bond is not required in v1 | Superseded by v0.14 |
| DEC-125 | Reward payout uses epochs; exact epoch duration remains implementation-tunable | Superseded by v0.14 |
| DEC-126 | Integer payout remainder stays in the corresponding reward pool for the next epoch | Superseded by v0.14 |
| DEC-127 | CYBOU Email is a consensus-registered mail protocol, not a realtime messenger | Frozen v0.13 |
| DEC-128 | v1 MailTx stores E2E encrypted text in block data | Frozen v0.13 |
| DEC-129 | current state stores compact MailMarker data, not full email ciphertext | Superseded by v0.14 |
| DEC-130 | v1 has no Email Relay, Mailbox Store or ServiceNodeRegistry | Frozen v0.13 |
| DEC-131 | recipient availability is not required; receiving occurs through normal state/block synchronization | Frozen v0.13 |
| DEC-132 | v1 Email is text-only; attachments are forbidden until Object Storage exists | Frozen v0.13 |
| DEC-133 | after Object Storage, encrypted mail body/attachments move to Store and MailTx carries content commitments/references | Frozen direction |
| DEC-134 | v1 recurrent fee split is 75% Validators / 25% Onboarding | Frozen v0.13 |
| DEC-135 | v1 Fee Router uses exact 4-CYBOU batches: 3 Security / 1 Onboarding | Frozen v0.13 |
| DEC-136 | no rewarded generic service-node role exists before Object Storage | Frozen v0.13 |
| DEC-137 | new invited account baseline is 25 outgoing MailTx/day | Frozen v0.13 |
| DEC-138 | finalized MailTx provides cryptographic registration/origin/integrity evidence but is not automatically a legal notarial act | Frozen v0.13 |
| DEC-139 | Permanent per-MailTx consensus-state MailMarker is rejected | Frozen v0.14 |
| DEC-140 | MailTx existence/authenticity is proven by typed operation + block inclusion + BFT finality, not permanent bounded mail-validation state | Frozen v0.14 |
| DEC-141 | Before Store, active validators retain canonical historical MailTx bodies required for retrieval | Frozen v0.14 |
| DEC-142 | Broad mass-scale Email is gated on Object Storage or equivalent durable content layer | Frozen v0.14 |
| DEC-143 | MailTx is a first-class CYBOU protocol operation, not OP_RETURN/Bitcoin Script application data | Frozen v0.14 |
| DEC-144 | MailTx fees are deterministic and size-aware; strict max serialized MailTx size required | Frozen v0.14 |
| DEC-145 | Priority fee / fee bidding is disabled in v1 | Frozen v0.14 |
| DEC-146 | All active v1 validators have equal consensus weight = 1 | Frozen v0.14 |
| DEC-147 | Four active validators is the minimum deployment target when claiming f=1 BFT tolerance | Frozen v0.14 |
| DEC-148 | PoT time/rate logic uses deterministic block-height-derived protocol epochs, never local wall clock | Frozen v0.14 |
| DEC-149 | PoT consensus arithmetic is integer/deterministic | Frozen v0.14 |
| DEC-150 | Welcome Grant requires one-time Operator-signed Invite Voucher | Frozen v0.14 |
| DEC-151 | Identity creation alone does not receive Welcome Grant | Frozen v0.14 |
| DEC-152 | Operator Authority, Validator, Release Signing and Treasury keys are separate domains | Frozen v0.14 |
| DEC-153 | Operator Authority target custody is 2-of-3 | Frozen operational direction |
| DEC-154 | MailEvidenceBundle must prove historical sender-key authorization at MailTx height | Frozen v0.14 |
| DEC-155 | Mail content commitment uses random salt/domain separation; no bare predictable plaintext hash | Frozen v0.14 |
