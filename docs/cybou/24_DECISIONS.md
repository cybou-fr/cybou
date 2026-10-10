# Current product and protocol decisions

Status: CURRENT
Scope: Level 1 accepted decisions; implementation gaps do not change protocol bytes.

## Security, privacy and resilience precedence

Applicable law (RGPD and NIS2 where applicable), adopted ANSSI/ISO controls and
CIA objectives take precedence over internal decisions under `AGENTS.md` and
[`SECURITY_GOVERNANCE.md`](SECURITY_GOVERNANCE.md). Technical standards support
this broader baseline. [`SECURITY_STANDARDS.md`](SECURITY_STANDARDS.md) records primary
sources and implementation gaps. Resolve conflicts by revising decisions and
planning compatibility before release; drafts remain experimental. This rule
does not itself modify wire bytes, keys, cryptographic domains or genesis.


## Storage implementation evidence boundary

Operator-authorized destructive DEVNET resets may retain the exact compiled
genesis and keys under the bounded exception in `AGENTS.md`. Such an exercise
archives and clears active network data, including signing journals, across all
participants before starting one signer. Old signed histories remain valid and
can cause replay or safety halts; no MAINNET or production recovery exception exists.

The Beta target remains two independent remote full replicas; the current
placement algorithm deduplicates proven StorageIds, which does not establish
independent hosts, operators or failure domains. The 1:3 reciprocal baseline
is a capacity/service objective, not measured proof of contribution: explicit
local capacity is an operator choice (`V >= 15 GiB`), not proof of service.
Storage is paid by finalized leases; only PoA-signed settlements record service.

Canonical state currently records publications, roots and recipient capsules,
not provider placements or audit reliability. Off-chain audit transport is implemented (DEC-276). Autonomous mutual-audit
scheduling and PoA evidence aggregation require further design/integration;
per-audit canonical notarization and reliability coefficients are not the current
accepted storage model.
Off-chain storage evidence (DEC-276) consists of provider-signed receipts,
random-offset audits and periodic full GET plus ChunkID verification; none of
it is consensus state or a canonical proof.

Finalized revocation stops admission and closes the author's lease after the current period.
Compliant providers journal purge of unshared chunks, retaining physical byte
accounting until unlink succeeds or absence is confirmed; maintenance/restart
retry failures. This does not prove deletion of hidden copies or crypto-erasure.
See [data assurance evidence](DATA_ASSURANCE_AND_ERASURE.md) for scoped regression evidence.

## Uniform Full Node invariant

CYBOU defines exactly one network node type: Full Node. Every Full Node
implements the complete CYBOU P2P baseline: blocks, announcements,
discovery, operation relay and encrypted storage. There
is no capability bitmap and no network role announcement. Storage is intrinsic;
capacity is local policy. Bootstrap is only a known locator of an ordinary Full
Node. PoA is
possession of the private key matching the public key in genesis, with durable
signing safety. IP, endpoints, TLS sessions, StorageId and peer declarations
never confer consensus authority. StorageId is proven on demand only for a
storage relationship. Peer sync completion is a liveness/UX hint, never proof
of global freshness or a prerequisite for creating an Identity.

This register contains current accepted decisions only. Cancelled decisions and
prior wording live in [the historical register](history/decisions/2026-10-09-superseded.md).
Accepted targets with implementation gaps are explicitly marked; they are not
authority to silently change the current wire format. Lower documentation levels cannot introduce
architecture that contradicts these decisions.

## Active frozen decisions

### Product

| ID | Decision | Status |
|---|---|---|
| DEC-173 | Mail attachments use the shared encrypted content substrate; Mail is not a separate network transport. | Frozen |
| DEC-179 | Mail and Files use familiar Gmail/Google Drive interaction patterns without copying their branding or centralized trust assumptions. | Frozen |
| DEC-180 | Normal UI presents user actions/outcomes; protocol details use progressive disclosure. | Frozen |
| DEC-181 | Finality alone is not `Sent`/`Protected`; remote durability is required. | Frozen |
| DEC-185 | Files is the Beta file-management surface; Backup is post-Beta. | Frozen |
| DEC-194 | Mail and Files are product surfaces over one Identity, finalized state and one encrypted content substrate. | Frozen |

### Identity and onboarding

| ID | Decision | Status |
|---|---|---|
| DEC-150 | Account creation is permissionless and uses protocol-native anti-Sybil work. | Frozen |
| DEC-152 | Identity Recovery, Authorization, KEM, Network Root, PoA, Release Signing and Treasury use separate key roles. | Frozen |
| DEC-165 | AccountID is a random stable nonzero 256-bit identifier independent of mnemonic and keys. | Frozen |
| DEC-169 | `.cybou` names are protocol names finalized through commit/work/reveal. | Frozen |
| DEC-193 | Device is not a protocol Identity entity; one account has one current authorization/KEM key set. | Frozen |

### Core protocol

| ID | Decision | Status |
|---|---|---|
| DEC-197 | ROOT/INDEX metadata contains private ordered ChunkIDs; DATA contains application bytes; ChunkID is full BLAKE3 of stored ciphertext. | Frozen |
| DEC-198 | Mail has no consensus operation or per-message canonical state; private Mail is discovered through generic RootPublication. | Frozen |
| DEC-199 | The physical ChunkStore is one encrypted content-addressed network store and has no user-facing own/foreign semantic classification. | Frozen |
| DEC-200 | Each unlocked Identity uses a separate encrypted rebuildable Application DB; GUI never browses provider ChunkStore contents. | Frozen |
| DEC-201 | One RootPublication may authorize chunks from multiple private encrypted trees; only the main root is capsule-addressed. This does not create a new wire entity. | Frozen |
| DEC-202 | Recoverable publisher content uses an application-layer self capsule. | Frozen |
| DEC-203 | Files persistent private history uses a minimal ordered mutation model (`UPSERT_ITEM`, `DELETE_ITEM`) over canonical PoA order. | Frozen |
| DEC-204 | Identity rotation must protect required historical KEM recovery material before rotation when clean recovery needs old epochs. | Frozen |
| DEC-205 | Development targets 1 remote full replica; Beta targets 2 independent remote full replicas plus local cache. Placement currently counts distinct StorageIds; physical/operator independence requires separate evidence. Cache does not count toward remote durability; erasure coding is disabled. | Frozen; implementation boundary explicit |
| DEC-206 | Placement, provider health, audit and repair are StorageService policy, not consensus state. | Frozen |
| DEC-212 | StorageId is BLAKE3 of the existing domain-separated STORAGE public key encoding, proven on demand with a fresh challenge bound to TLS exporter and both HELLOs. It proves possession of a cryptographic storage key only; all Full Nodes implement storage. | Frozen |
| DEC-213 | Local blob retention is a generic node-local pin/cache registry keyed by opaque (holder, reference) tags; GC evicts only unpinned, non-admitted cache entries past a grace period. ChunkStore stays free of application semantics. | Frozen |
| DEC-217 | Authority creates no canonical reservations, tickets, resource budgets, or per-I/O accounting. Provider limits are local policy. | Frozen |
| DEC-244 | Official networks are DEVNET and MAINNET. Network identity is immutable `NetworkID = Network Public Key`. For each NetworkID, exactly one signed genesis is valid and immutable for the lifetime of that network. The Network Private Key is strictly offline, never online, and used solely by the network owner to sign the immutable genesis at network creation. | Frozen |
| DEC-245 | Bootstrap is an ordinary CYBOU full peer running the same executable and CYBOU P2P protocol; it has no network-role announcement, no consensus role, and no special protocol capability. Known IP:port provides transport discovery only. | Frozen |
| DEC-246 | GenesisAllocation assigns initial CYBOU to ordinary Identities and is claimed once. Allocations grant no bootstrap role or consensus authority (DEC-277, DEC-284). | Frozen current rule; prior wording archived |
| DEC-247 | Canonical finality is single-operator hybrid-PQ PoA under the genesis-authorized PoA key. The PoA finalizer MUST execute every candidate independently, trusts no peer state, and remains the sole canonical finalizer without BFT or validator quorums. | Frozen |
| DEC-249 | Every Full Node independently executes each candidate against its latest finalized state and relays only locally valid candidates. No additional Validation signature or provisional state exists (DEC-284). | Frozen current rule; prior wording archived |
| DEC-252 | For each NetworkID exactly one signed genesis is valid. Genesis is immutable for the lifetime of that network. There is no `genesis_generation`, re-genesis, in-place genesis replacement, or `NetworkTrustStore`. Any different genesis requires a new Network Key and therefore a new NetworkID. | Frozen |
| DEC-253 | Each official network is compiled into the client as public constants: exact Network Public Key (`NetworkID`), immutable signed `NetworkGenesis` object, initial genesis state, and bootstrap locators (IP:port + TLS SPKI). Runtime uses no external official network/genesis file or separate genesis digest profile pin. Bootstrap never supplies genesis. | Frozen |
| DEC-254 | Provisioning generates the Network and ordinary `cybou.cybou` Identity private material once. Secret material lives only under gitignored `/private/`; Git contains public keys, public Identity data, and signed genesis constants. The Network Private Key remains strictly offline. | Frozen |
| DEC-255 | DEVNET is the enabled official profile with bootstrap locator `51.255.46.58:29461`. MAINNET is unprovisioned, has no bootstrap locator, and is disabled in the GUI until its keys, genesis, and bootstrap are created. | Frozen |
| DEC-256 | `cybou.cybou` is an ordinary account-level Identity with AccountID, Recovery, Authorization, KEM, Mail/support, and a distinct PoA signing key role from its mnemonic. It is not a separate PoA Identity entity. Consensus finalization right is determined only by the PoA public key authorized in genesis, never by name or AUTH value. | Frozen |
| DEC-258 | Every network participant is a Full Node implementing the uniform CYBOU P2P baseline without a capability bitmap or network role. Storage is intrinsic and quota-controlled. StorageId is proven on demand only for storage. PoA is private-key possession with durable signing safety; signer toggles never reconnect peers. Peer sync completion is advisory and does not gate Identity creation. | Frozen |

| DEC-259 | Runtime and StateStore take verified signed genesis as their sole network definition; its signed specification digest is the chain height-zero tip and durable signing-journal anchor. No synthetic genesis block identifier exists. | Frozen |
| DEC-260 | CYBOU P2P has compact IDs 1–28, batched consecutive GET_BLOCKS with strict BLOCKS_END count, and one shared OP_META/OP_DATA/OP_RESULT path for submission and polling. Configured peers are one ordered endpoint/optional-pin vector; discovered peers remain bounded and separate. | Frozen |
| DEC-261 | PublicationService stages directly into pinned local encrypted blobs and saves the ordered authorization leaf list once in encrypted Application DB. A transient O(N) Merkle tree generates individual O(log N) proofs; no persisted proof levels or separate staging service exists. | Frozen |
| DEC-262 | RootPublication wire layout, encrypted ROOT/INDEX schema and private Mail/Files/RecoveryBridge schema are fixed-order bounded binary layouts. Reject unknown types and trailing bytes; no generic CBOR parser or legacy decoder remains. BLAKE3 is unchanged. | Frozen |
| DEC-263 | Native Hash256 is an opaque canonical 32-byte value; raw-byte order, comparison and forward hex agree. No numeric padding or reverse-byte display. Hash256, hex and binary codecs are CYBOU-native. | Frozen |
| DEC-264 | Single-current-baseline architecture: if only one supported form of a structure, wire message, state, or schema exists, it has no version identifier. Version fields appear only when multiple formats actually coexist or temporary migration is required, and are removed once the migration window closes. Code does not record development history in type names or wire bytes. Cryptographic domain separation strings transition to eternal unversioned names exclusively during coordinated network genesis resets before MAINNET. | Frozen |
| DEC-265 | Production capacity is explicitly chosen, V >= 15 GiB (DEC-275). Zero/smaller capacities are confined to memory-only component fixtures. Low disk rejects new storage admissions without changing node type, finality authority or mesh participation. | Frozen current rule; prior wording archived |
| DEC-266 | Official network provisioning (Network Root key generation, NetworkID derivation, and genesis signing) is strictly offline tooling (`cybou-provision`), not a production `cybou` command or runtime capability. Production binaries contain only compiled public official network constants and cannot generate or re-provision official networks. | Frozen |
| DEC-267 | Canonical state records RootPublication metadata, roots and recipient capsules; quotas and admission rights derive from finalized state. Provider placements are encrypted local metadata. Canonical mutual-proof records remain an unimplemented target requiring an evidence/privacy/accounting design; no local capacity declaration grants authority. | Frozen; implementation boundary explicit |
| DEC-271 | Finalized author RevokePublication removes the active register entry, frees canonical quota and stops admission. Compliant providers durably schedule unshared chunks for managed purge and retry failures. Historical blocks remain; finality and local purge do not prove hidden-copy deletion or crypto-erasure. | Frozen; implementation boundary explicit |
| DEC-272 | Author-only finalized RevokePublication stops admission and schedules managed purge of unshared chunks (DEC-271, DEC-279). One .cybou name per Identity. No AUTH tier limits or canonical publication quota remain (DEC-274, DEC-284). | Frozen current rule; prior wording archived |
| DEC-274 | Remote storage is paid CYBOU service, not an AUTH entitlement. At the storage-economy cutover (DEC-283) AUTH storage quota and AUTH largest-publication limits, `AccountUsage::stored_chunks`, `STORAGE_QUOTA_EXCEEDED` and AUTH-based `PUBLICATION_TOO_LARGE` are removed. `MAX_PUBLICATION_CHUNKS` remains a parser/memory/CPU safety bound only. Local capacity declarations confer no monetary or storage right. Relay PoW, AccountCreate PoW and name PoW are outside this decision; AUTH and Validation were removed later by DEC-284. | Frozen; implemented (M5), active from the M7 genesis |
| DEC-275 | Every production Full Node has an explicit local CYBOU capacity `V >= 15 GiB` chosen by its operator (GUI default 15 GiB; headless `--capacity`). The single content-addressed `ChunkBlobStore` is bounded by `V`; finalized provider obligations are bounded by `floor(2V/3)`; the remainder is a local reserve, not a network quota. No volume container, custom filesystem, volume allocator, volume journal or volume Merkle tree exists. `V` is never consensus state. Automatic free-space allocation is removed. | Frozen; implemented (M2) |
| DEC-276 | Storage evidence is off-chain StorageService data: durable provider obligations (PublicationID, ChunkID, stored size, StorageId, state), signed `StorageReceipt` after PUT, periodic `AUDIT_CHALLENGE`/`AUDIT_RESPONSE` over exact ciphertext using `StorageAuditChallenge`, and rarer full GET with ChunkID recomputation. Evidence is bounded with retention; one response never proves service over time. No per-audit, per-GET or ping record enters a block. A PoA node may audit as an ordinary peer. | Frozen; implemented (M3): receipts, audit transport, full-GET spot checks, in-memory evidence |
| DEC-277 | CYBOU is created only by genesis. The whole genesis monetary base belongs to the `cybou.cybou` Central Treasury allocation; there is no `MAX_SUPPLY`, OnboardingPool, unissued supply, mint or burn. `TotalCybou` = unclaimed genesis Balances + Balances + System Balances + StorageEscrow; block execution requires `TotalCybou(parent) == TotalCybou(candidate)` and rejects overflow as invalid state. AccountCreate (PoW retained) transfers the genesis `onboarding_bonus` from Treasury Balance to the new Identity System Balance; claiming the `cybou` allocation receives no bonus. | Frozen; implemented (M5), active from the M7 genesis |
| DEC-278 | Protocol operation fees: payer System Balance -> Central Treasury Balance (100%). Storage rent: payer System Balance -> StorageEscrow -> providers with verified foreign storage service (100%; no Central Authority commission). No stake, slashing, auction or variable provider price: a failed audit means no payment, degraded replica and repair. | Frozen; implemented (M5), active from the M7 genesis |
| DEC-279 | `StorageLease` is the canonical rent operation per PublicationID: billing units of 512 KiB per authorized chunk, `replica_target = 2`, period count, escrow amount, Authorization signature. Cost uses integer arithmetic over `units x seconds x replicas x rate / (2048 x 86400)` with floor rounding and a carried integer remainder (`AccrueStorageRent`); no floating point. A RootPublication carries `lease_periods` and pays its initial lease atomically, so finalized content is never unpaid; StorageLease extends it. Escrow is charged rounded up; each settlement pays at most one period's rent per lease. The settlement of a lease's last period refunds the remaining escrow to payer System Balance with its origin. RevokePublication shortens the lease to the current period: final settlement, refund, managed purge. | Frozen; implemented (M5), active from the M7 genesis; rate provisional until measured |
| DEC-280 | Paid providers are never chosen by the payer. Assignment is randomized over eligible Full Nodes (valid recently proven StorageId, reachable, provider budget available, acceptable recent behaviour), seeded by finalized randomness, and attested by PoA in settlement; payments to payer-selected or unassigned providers are invalid. No scoring with top-k, capacity weighting, stake, AUTH, certification or storage-node role. The two replicas of a chunk use distinct StorageIds and distinct payout AccountIDs ("distinct storage/economic identities", not proven independent failure domains). | Accepted assignment target; local CSPRNG placement is implemented, finalized seed/assignment attestation are not evidenced. See DOC-005 in the conflict register. |
| DEC-281 | Onboarding-origin CYBOU never becomes transferable through storage. System Balance tracks its onboarding-origin portion, consumed first by debits; escrow records the funded origin split; the onboarding-origin share of a payout credits provider System Balance and only the SystemLock-origin share credits provider Balance. This closes self-dealing conversion of onboarding credit into transferable CYBOU. | Frozen; implemented (M5): `AccountState.onboarding_system_balance`, lease `escrow_onboarding`/`escrow_locked` |
| DEC-282 | `StorageSettlement` is a PoA-signed operation per contiguous 86,400 s period (`period_start_utc`, `period_end_utc`, evidence root, entries of lease, StorageId, payout AccountID, verified service, payout). Full Nodes verify PoA signature, contiguity and monotonicity, lease and escrow existence, payout <= remaining escrow, valid `StoragePayoutBinding`, no duplicate payout, exact arithmetic and conservation; they do not claim to have observed audits. Beta trust model: PoA is the canonical aggregator of off-chain storage evidence and of settlement time. `StoragePayoutBinding` is signed by both the Storage key and the Account Authorization key, served with the StorageId proof and verified by clients against the finalized Authorization key; no provider register or role exists in state. | Accepted settlement target, not a current wire schema. Current entries are publication_id/payout_account/amount; evidence_root, StorageId and explicit end time are absent from wire. See DOC-006 in the conflict register. |
| DEC-283 | The storage economy is active on existing DEVNET (DEC-274–DEC-284); its current immutable genesis is retained. Protected requires active publication/lease and confirmed remote durability under the service policy. Further provisioning, cutover or reset is a separate explicitly authorized operation, never an unfinished documentation step. | Frozen current rule; prior wording archived |
| DEC-273 | Relay work is SHA-256(CYBOU/OP-WORK || NetworkBinding || OperationID || nonce u64 LE), flat 22 leading zero bits, names +4 (DEC-284). Every Full Node checks it before candidate execution/relay. OP_META carries the nonce; it never enters finalized blocks/state. AccountCreate retains separate consensus PoW; StorageSettlement uses the genesis PoA signature. | Frozen current rule; prior wording archived |
| DEC-284 | AUTH, Validation and AUTH operation tiers are removed. AccountState holds only Balance, System Balance (with its onboarding-origin part) and creation height/epoch; GenesisAllocation carries no AUTH; there is no `PoaAuthAdjustment`, no per-account usage counters, no per-block or per-epoch Identity operation limit and no Validation signature, attestation message or `Validated` state. Spam is priced by protocol fees and storage rent in CYBOU and by relay proof-of-work at one flat difficulty: 22 leading zero bits, NameCommit/NameReveal +4; AccountCreate keeps its consensus PoW and StorageSettlement its PoA signature. Operation kinds are 1–10 (StorageLease 9, StorageSettlement 10); P2P storage messages are 23–26. The storage payout binding keeps the ordinary Identity Authorization signer. Requires a new DEVNET genesis (state format changed). | Frozen; implemented; DEVNET `eee26eca…3665` |
| DEC-285 | France-only peer admission applies to public addresses. Local-network addresses that are never publicly routed (loopback, RFC 1918, link-local, IPv6 ULA, including v4-mapped forms) are admitted without Geo data, inbound and outbound. Public addresses still require valid local DB-IP data and a French range and fail closed otherwise. This lets an operator's machines, VMs and LAN form a mesh (battle tests, home networks); it grants no consensus authority and changes no wire format. | Frozen; implemented |
| DEC-286 | The PoA block production loop signs a block only when its candidate pool is non-empty; it wakes on candidate admission, signer changes and finalized-head changes, with timed waits only for the local block interval or transient retry deadline (R5, 2026-10-05). Idle production does not poll. Empty blocks every second made the chain grow by 86,400 blocks per day and every new node had to download and verify them. Block height no longer tracks time: block-counted windows (AccountCreate work epoch of 1,024 blocks, name reveal depth) last longer on a quiet network. A manual single-block finalization may still be empty. | Frozen; implemented |
| DEC-287 | Every Full Node, desktop included, accepts inbound peers: it listens on 29461 by default, or on any free port when that one is taken, and announces the port in HELLO (82 bytes, `listen_port` u16 LE, 0 = not listening). A receiver records the connection IP with that port only as a candidate, connects back, and shares the address with others only after a successful same-network handshake, so unreachable or forged endpoints are never advertised. The bootstrap is an ordinary peer everyone knows in advance and shares inbound peers like any node. No node announces the PoA key. Wire change: all DEVNET nodes update together. | Frozen; implemented |
| DEC-288 | Eligible providers come from known Full Nodes, beyond outbound mesh sessions. Existing independent probes prove storage identity; DEC-290 uses a 30-second per-endpoint counter sampling interval and 90-second counter freshness. Placement proof retention is 30 minutes. Distinct StorageIds/addresses do not prove independent physical hosts. Production placement excludes this machine and deduplicates addresses; in-memory loopback fixtures are exempt. | Frozen current rule; prior wording archived |

### Network operations and topology

| ID | Decision | Status |
|---|---|---|
| DEC-289 | Beta monitoring is passive local metrics plus independently verified chain state, ordinary P2P liveness and service-owned storage evidence. No broad remote resource telemetry, address-group/cohort aggregation or separate polling scheduler. CPU/RAM/frame traffic stay local Technical/Console diagnostics. Network charts are limited to finalized operations and completed PUT/GET payload. Additions require a concrete question, trustworthy source and explicit scope decision. No global census or throughput ceiling is inferred. | Frozen local boundary; narrow storage extension under DEC-290 |
| DEC-290 | Network shows three figures: one operations/min value (initially 5, replaced and saved locally per exact NetworkID by each new positive completed 60-second observation of actually finalized operations; zero, absent/incomplete measurements and synchronization retain the saved value across launches; no separate base metric; history imports, Battle Test references and artificial/theoretical rates excluded; observed maxima derive only from real observations, never initial 5), approximate summed provider capacity and admitted encrypted bytes across responding Full Nodes. Query existing FinalizedChunkStore counters over ordinary TLS, reuse existing storage identity for deduplication; no new usage signature, audit, accounting platform, registry or worker. Reuse independent storage probe (30 s per endpoint, 90 s sample freshness), show ≈/coverage/time; do not sum peer aggregates or claim a census, free disk, unique-file bytes or audited service. Centered full map with a top horizontal translucent overlay; benchmark UI removed. | Explicit operator scope correction, 2026-10-09; peer deployment/counter responses evidenced; full GUI aggregate acceptance remains scoped |
| DEC-219 | The Central Authority desktop operates the genesis-authorized PoA key after unlock and local chain verification. The Network Private Key is separate and strictly offline. | Frozen target |
| DEC-226 | Authority mobility does not permit concurrent independent signers sharing one PoA key; one active signer and durable anti-equivocation safety remain required. | Frozen target |
| DEC-232 | All public inbound and outbound P2P admission is France-only for every Full Node; the rule is local networking policy, not consensus state. | Frozen target |
| DEC-233 | Known VPN/proxy/Tor filtering is optional local policy using local data. It never affects consensus, Identity, Authority, or PoA. | Frozen target |
| DEC-235 | Central Authority is identified only by the PoA key. It has no authenticated transport route; no persistent Authority endpoint or NodeID is stored. | Frozen target |
| DEC-238 | Cross-network migration does not exist. A network cutover to a new official network replaces all local network-bound state, wiping everything (chain, genesis, Identity, vault, AccountID, balances, names, Mail, Files, application DB, peer DB, storage metadata). | Frozen target |
| DEC-240 | Ordinary peers form a direct P2P mesh after initial discovery. Bootstrap is an initial rendezvous peer, not a mandatory traffic intermediary. | Frozen target |
| DEC-251 | PoA owns no special canonical pending state. Every full node holds a bounded volatile pool of locally executed candidate operations; a PoA node produces blocks from that same pool. Only finalized state is canonical. | Frozen target |

### Fixed economics

| ID | Decision | Status |
|---|---|---|

Current model in code (DEC-277–DEC-282):

```text
decimals = 0
genesis monetary base: 100,000,000,000 CYBOU, all in the cybou.cybou Treasury allocation
no MAX_SUPPLY, no OnboardingPool, no mint, no burn
TotalCybou(parent) == TotalCybou(candidate)
AccountCreate:      Treasury Balance -20,000 -> new System Balance +20,000
protocol fees:      payer System Balance -> Treasury Balance (100%)
storage rent:       payer System Balance -> StorageEscrow -> verified providers (100%)
lease refund:       StorageEscrow -> payer System Balance
onboarding origin:  never becomes transferable Balance through storage payouts
```

Runtime policy governance is not part of the current target. Network parameters
remain immutable.

---


## Local and Network application execution

| ID | Decision | Status |
|---|---|---|
| DEC-291 | LocalApplicationService owns indispensable encrypted local.db and commits Mail, drafts, Files desired state and immutable Outbox independently of network I/O. NetworkSyncService owns independent background execution through existing ApplicationService, PublicationService and StorageService and encrypted app.db. Preserve exact JobID/OperationID, canonical ordering, newer desired state and existing journals; no automatic DB deletion. Prepare wrapped access to the same data key before IdentityRotate. One Full Node, one process, unchanged protocol/consensus/cryptography. | Accepted operator plan, 2026-10-09; implementation acceptance remains scoped |

## Conflict handling

### DEC-292 — Economics-first cumulative settlement target (2026-10-09)

Accepted operator implementation plan: finish accounting, assignment/evidence
and recoverable payouts before financial UX/Beta. For each separately funded
term, reserve a whole-CYBOU ceil share per replica; payout is cumulative floor
entitlement from verified unit-seconds minus finalized paid. Do not round daily
payouts up, truncate obligations, use elapsed time as evidence or convert
onboarding-origin rent to transferable Balance. See
[the exact target and transition gates](ECONOMICS_SETTLEMENT_COMPLETION.md).
Pure target arithmetic and fail-closed preparation are implementation steps;
canonical state/wire enforcement, evidence integration and live acceptance remain
open. DEC-279's current deployed funding/cap rules stay explicitly documented
until a separately reviewed protocol transition. This decision does not authorize
cutover, genesis replacement, signer duplication or historical-state deletion.

Operator approval on 2026-10-09 permits developing canonical StorageLease,
StorageSettlement and CybouState changes and checking them in isolated fixtures.
It does not authorize deployment or modifying the running DEVNET. Partial
implementation commits are development checkpoints, not an activated supported
network format or completed economics acceptance.

Operator clarification on 2026-10-10 adopts the isolated Beta service policy:
planned checks every 12 hours per paid chunk/slot/provider; maximum credited
two-success gap 24 hours; exact GET at initial/replacement assignment and at
least every eighth successful check. Any failure breaks the credited interval
without resetting the GET success counter. Gaps beyond 24 hours receive no
automatic catch-up credit. This is isolated-test authorization only; activated
payment-policy changes require an explicit protocol decision. It does not close
assignment provenance, canonical service/paid, settlement or deployment gates.

The 2026-10-10 [concrete atomic contract for review](ECONOMICS_SETTLEMENT_COMPLETION.md#concrete-atomic-assignmentsettlement-contract-for-review-2026-10-10)
proposes finalized PREPARE/ACTIVATE actions inside StorageSettlement before PAY,
with a strictly later seed and retained canonical binding/allocation summaries.
The operator explicitly approved these actions and their effective-epoch rule
on 2026-10-10 for implementation and verification in isolated tests. The placement
gate is closed for this atomic slice only. The reviewed atomic proof layout fits
only 20 eligible bindings and 3988 manifest chunks; approval does not imply
completed implementation, scalable Beta acceptance or deployment authorization.

See [documentation conflict register](DOCUMENTATION_CONFLICT_REGISTER.md).
Resolve architecture/code gaps explicitly before release; do not rewrite accepted
requirements merely because code differs. Historical records never create tasks.
