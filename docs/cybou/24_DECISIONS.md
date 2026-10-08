# 24 â€” Current product and protocol decisions

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
is a capacity/service objective, not measured proof of contribution: automatic
local capacity is an explicit operator choice (`V >= 15 GiB`), not proof of service.
Storage is paid by finalized leases; only PoA-signed settlements record service.

Canonical state currently records publications, roots and recipient capsules,
not provider placements or audit reliability. Mutual-audit transport, PoA
notarization and canonical reliability coefficients are unimplemented target
work requiring an evidence/privacy/accounting design before implementation.
Off-chain storage evidence (DEC-276) consists of provider-signed receipts,
random-offset audits and periodic full GET plus ChunkID verification; none of
it is consensus state or a canonical proof.

Finalized revocation stops admission and closes the author's lease after the current period.
Compliant providers journal purge of unshared chunks, retaining physical byte
accounting until unlink succeeds or absence is confirmed; maintenance/restart
retry failures. This does not prove deletion of hidden copies or crypto-erasure.
See `docs/cybou/DATA_ASSURANCE_AND_ERASURE.md` for scoped regression evidence.

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

This register records active frozen architecture decisions, followed by the
superseded decision history. Lower documentation levels cannot introduce
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
| DEC-151 | Account creation funds System Balance from OnboardingPool and does not mint supply. | Superseded by DEC-277 (Treasury-funded onboarding); removed from code in M5 |
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
| DEC-246 | GenesisAllocation may assign initial AUTH to designated ordinary Identities. AccountCreate claims it exactly once. Such an allocation is a genesis decision, not a property of the bootstrap role; there are no consensus bootstrap grants or bootstrap Identity roles. | AUTH part superseded by DEC-284 (allocations carry CYBOU only); otherwise frozen |
| DEC-247 | Canonical finality is single-operator hybrid-PQ PoA under the genesis-authorized PoA key. The PoA finalizer MUST execute every candidate independently, trusts no peer state, and remains the sole canonical finalizer without BFT or validator quorums. | Frozen |
| DEC-248 | AUTH is a canonical non-transferable account value stored in AccountState and committed by the state root, independent of the 100B CYBOU supply. It changes only through deterministic finalized transitions: GenesisAllocation, +1 AUTH to the authorizing account of finalized utility operations (RootPublication, SystemLock), capped at +1 per account per block, and one PoA-signed `PoaAuthAdjustment` operation with GRANT (+N) or BURN (-N, floor 0) that itself earns no AUTH. There is no AUTH transfer. Finalized AUTH > 10,000,000 makes an Identity eligible to sign Validation; AUTH never grants PoA finalization power. Automatic penalties are not frozen. | Superseded by DEC-284 (no AUTH) |
| DEC-249 | Every full node independently validates and executes every candidate operation against its latest finalized state and relays only locally valid candidates. Validation is an additional signature by an eligible Identity over NetworkBinding, OperationID, finalized base BlockID and its AccountID, made only after its own node validated the operation. It never substitutes local or PoA execution, never changes state, and creates no provisional state. There is no `validation.enabled`, `min_signatures` policy or block-candidate Validation. | Local execution and relay rule frozen; Validation superseded by DEC-284 (removed) |
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
| DEC-265 | Production Full Node always has a positive storage allocation. Configured capacity of zero is forbidden in production and restricted to memory-only unit tests. Default allocation is automatic based on available storage. Nodes with depleted local disk space reject new inbound admissions without changing node type, consensus authority, or mesh relay participation. | Automatic allocation superseded by DEC-275 (explicit `V >= 15 GiB`); positive-capacity and low-disk rules remain |
| DEC-266 | Official network provisioning (Network Root key generation, NetworkID derivation, and genesis signing) is strictly offline tooling (`cybou-provision`), not a production `cybou` command or runtime capability. Production binaries contain only compiled public official network constants and cannot generate or re-provision official networks. | Frozen |
| DEC-267 | Canonical state records RootPublication metadata, roots and recipient capsules; quotas and admission rights derive from finalized state. Provider placements are encrypted local metadata. Canonical mutual-proof records remain an unimplemented target requiring an evidence/privacy/accounting design; no local capacity declaration grants authority. | Frozen; implementation boundary explicit |
| DEC-268 | Resource rate limits and storage allowances are governed by the canonical AUTH ladder. Low-AUTH identities have bounded per-epoch operation rates and storage allowances to mitigate spam and Sybil attacks. Allowances scale progressively with finalized AUTH up to the Validator tier (AUTH > 10,000,000). The concrete, finite ladder is DEC-272. | Superseded: storage by DEC-274, operation rates by DEC-284 |
| DEC-269 | Each newly registered Identity has a finalized 5 GiB remote publication quota at the onboarding tier. The 1:3 reciprocal baseline is a capacity/service objective, not evidence of actual contribution. Local automatic allocation depends on free disk space; it does not guarantee 10–15 GB or enforce measured service reciprocity. | Superseded by DEC-274/DEC-277 (paid storage, Treasury onboarding); removed from code in M5 |
| DEC-270 | Target: randomized mutual storage audits and possible PoA-notarized reliability evidence. Current runtime uses operational GET/hash checks; mutual-audit transport, notarization and canonical reliability coefficients are unimplemented. Their evidence, privacy and accounting design remains open before implementation. | Superseded by DEC-276/DEC-282 (off-chain evidence, PoA settlement; no canonical reliability coefficients) |
| DEC-271 | Finalized author RevokePublication removes the active register entry, frees canonical quota and stops admission. Compliant providers durably schedule unshared chunks for managed purge and retry failures. Historical blocks remain; finality and local purge do not prove hidden-copy deletion or crypto-erasure. | Frozen; implementation boundary explicit |
| DEC-272 | Consensus-enforced AUTH tier limits. Block execution meters every Identity-authorized operation (Payment, SystemLock, IdentityRotate, NameCommit, NameReveal, RootPublication, RevokePublication) against the parent finalized AUTH: operations per block and per epoch, remote storage quota and largest publication. Counters (`usage`) and the publication register (`publications`, keyed by RootPublication OperationID) are committed by the state root; the section is omitted while empty, so the genesis state root is unchanged. Tiers: T0 <10k: 1/block, 30/epoch, 5 GiB, file 1 GiB; T1 >=10k: 5, 150, 25 GiB, 4 GiB; T2 >=100k: 25, 750, 100 GiB, 16 GiB; T3 >=1M: 100, 3,000, 500 GiB, 64 GiB; Validator >10M: 1,000, 30,000, 2 TiB, 256 GiB. Quota unit is 512 KiB per authorized chunk. `RevokePublication` (author-only, payment fee, no AUTH) frees quota and stops chunk admission; providers purge chunks no other publication authorizes. One `.cybou` name per Identity. | Storage quota and file-size parts superseded by DEC-274, AUTH operation limits by DEC-284; revocation and one-name rule remain frozen |
| DEC-274 | Remote storage is paid CYBOU service, not an AUTH entitlement. At the storage-economy cutover (DEC-283) AUTH storage quota and AUTH largest-publication limits, `AccountUsage::stored_chunks`, `STORAGE_QUOTA_EXCEEDED` and AUTH-based `PUBLICATION_TOO_LARGE` are removed. `MAX_PUBLICATION_CHUNKS` remains a parser/memory/CPU safety bound only. Local capacity declarations confer no monetary or storage right. Relay PoW, AccountCreate PoW and name PoW are outside this decision; AUTH and Validation were removed later by DEC-284. | Frozen; implemented (M5), active from the M7 genesis |
| DEC-275 | Every production Full Node has an explicit local CYBOU capacity `V >= 15 GiB` chosen by its operator (GUI default 15 GiB; headless `--capacity`). The single content-addressed `ChunkBlobStore` is bounded by `V`; finalized provider obligations are bounded by `floor(2V/3)`; the remainder is a local reserve, not a network quota. No volume container, custom filesystem, volume allocator, volume journal or volume Merkle tree exists. `V` is never consensus state. Automatic free-space allocation is removed. | Frozen; implemented (M2) |
| DEC-276 | Storage evidence is off-chain StorageService data: durable provider obligations (PublicationID, ChunkID, stored size, StorageId, state), signed `StorageReceipt` after PUT, periodic `AUDIT_CHALLENGE`/`AUDIT_RESPONSE` over exact ciphertext using `StorageAuditChallenge`, and rarer full GET with ChunkID recomputation. Evidence is bounded with retention; one response never proves service over time. No per-audit, per-GET or ping record enters a block. A PoA node may audit as an ordinary peer. | Frozen; implemented (M3): receipts, audit transport, full-GET spot checks, in-memory evidence |
| DEC-277 | CYBOU is created only by genesis. The whole genesis monetary base belongs to the `cybou.cybou` Central Treasury allocation; there is no `MAX_SUPPLY`, OnboardingPool, unissued supply, mint or burn. `TotalCybou` = unclaimed genesis Balances + Balances + System Balances + StorageEscrow; block execution requires `TotalCybou(parent) == TotalCybou(candidate)` and rejects overflow as invalid state. AccountCreate (PoW retained) transfers the genesis `onboarding_bonus` from Treasury Balance to the new Identity System Balance; claiming the `cybou` allocation receives no bonus. | Frozen; implemented (M5), active from the M7 genesis |
| DEC-278 | Protocol operation fees: payer System Balance -> Central Treasury Balance (100%). Storage rent: payer System Balance -> StorageEscrow -> providers with verified foreign storage service (100%; no Central Authority commission). No stake, slashing, auction or variable provider price: a failed audit means no payment, degraded replica and repair. | Frozen; implemented (M5), active from the M7 genesis |
| DEC-279 | `StorageLease` is the canonical rent operation per PublicationID: billing units of 512 KiB per authorized chunk, `replica_target = 2`, period count, escrow amount, Authorization signature. Cost uses integer arithmetic over `units x seconds x replicas x rate / (2048 x 86400)` with floor rounding and a carried integer remainder (`AccrueStorageRent`); no floating point. A RootPublication carries `lease_periods` and pays its initial lease atomically, so finalized content is never unpaid; StorageLease extends it. Escrow is charged rounded up; each settlement pays at most one period's rent per lease. The settlement of a lease's last period refunds the remaining escrow to payer System Balance with its origin. RevokePublication shortens the lease to the current period: final settlement, refund, managed purge. | Frozen; implemented (M5), active from the M7 genesis; rate provisional until measured |
| DEC-280 | Paid providers are never chosen by the payer. Assignment is randomized over eligible Full Nodes (valid recently proven StorageId, reachable, provider budget available, acceptable recent behaviour), seeded by finalized randomness, and attested by PoA in settlement; payments to payer-selected or unassigned providers are invalid. No scoring with top-k, capacity weighting, stake, AUTH, certification or storage-node role. The two replicas of a chunk use distinct StorageIds and distinct payout AccountIDs ("distinct storage/economic identities", not proven independent failure domains). | Frozen; implemented: placement picks a uniformly random payout account (from a verified StoragePayoutBinding; a StorageId without binding is its own identity), then one of its nodes, and keeps a chunk's replicas on distinct accounts, so many StorageIds of one account count once. On-chain: payout never to the payer, at most `replicas` payouts per lease and period. Separate Identities remain priced only by AccountCreate PoW; assignment attestation is PoA off-chain duty |
| DEC-281 | Onboarding-origin CYBOU never becomes transferable through storage. System Balance tracks its onboarding-origin portion, consumed first by debits; escrow records the funded origin split; the onboarding-origin share of a payout credits provider System Balance and only the SystemLock-origin share credits provider Balance. This closes self-dealing conversion of onboarding credit into transferable CYBOU. | Frozen; implemented (M5): `AccountState.onboarding_system_balance`, lease `escrow_onboarding`/`escrow_locked` |
| DEC-282 | `StorageSettlement` is a PoA-signed operation per contiguous 86,400 s period (`period_start_utc`, `period_end_utc`, evidence root, entries of lease, StorageId, payout AccountID, verified service, payout). Full Nodes verify PoA signature, contiguity and monotonicity, lease and escrow existence, payout <= remaining escrow, valid `StoragePayoutBinding`, no duplicate payout, exact arithmetic and conservation; they do not claim to have observed audits. Beta trust model: PoA is the canonical aggregator of off-chain storage evidence and of settlement time. `StoragePayoutBinding` is signed by both the Storage key and the Account Authorization key, served with the StorageId proof and verified by clients against the finalized Authorization key; no provider register or role exists in state. | Frozen; implemented (M5) except off-chain evidence aggregation; active from the M7 genesis |
| DEC-283 | Storage economy delivery: M1 architecture freeze; M2 explicit capacity; M3 evidence; M4 shadow accounting on current DEVNET with no CYBOU moved; M5 consensus economics; M6 adversarial, storage-failure, monetary-conservation, provider-concentration and Sybil simulations as release gates; M7 new DEVNET (new Network Root, NetworkID and signed genesis with `onboarding_bonus = 20,000`, rate 5 CYBOU/GiB/day/replica, unit 512 KiB, replicas 2, period 86,400 s) only under explicit operator authorization; M8 product Beta. Consensus changes wait for M7; the current DEVNET genesis is never modified. `Protected` then requires an active publication, active funded lease, two remote obligations at distinct storage/economic identities and fresh evidence. | Frozen target |
| DEC-273 | Relay proof-of-work for every user operation. Work = SHA-256(`CYBOU/OP-WORK` || NetworkBinding || OperationID || nonce u64 LE) with leading zero bits >= the author tier difficulty (22/21/20/19/18 bits for T0..Validator; NameCommit/NameReveal +4). Every Full Node, PoA included, checks it before candidate execution and relay; `OP_META` carries the nonce beside the exact bytes. The work is pre-finalization evidence only: it is not part of the block, the block hash or the state, and finalized history is never re-checked for it. AccountCreate (consensus PoW) and PoaAuthAdjustment (genesis PoA signature) are exempt. | Tiered difficulty superseded by DEC-284 (flat 22 bits, names +4); work rule frozen |
| DEC-284 | AUTH, Validation and AUTH operation tiers are removed. AccountState holds only Balance, System Balance (with its onboarding-origin part) and creation height/epoch; GenesisAllocation carries no AUTH; there is no `PoaAuthAdjustment`, no per-account usage counters, no per-block or per-epoch Identity operation limit and no Validation signature, attestation message or `Validated` state. Spam is priced by protocol fees and storage rent in CYBOU and by relay proof-of-work at one flat difficulty: 22 leading zero bits, NameCommit/NameReveal +4; AccountCreate keeps its consensus PoW and StorageSettlement its PoA signature. Operation kinds are 1–10 (StorageLease 9, StorageSettlement 10); P2P storage messages are 23–26. The storage payout binding keeps the ordinary Identity Authorization signer. Requires a new DEVNET genesis (state format changed). | Frozen; implemented; DEVNET `eee26eca…3665` |
| DEC-285 | France-only peer admission applies to public addresses. Local-network addresses that are never publicly routed (loopback, RFC 1918, link-local, IPv6 ULA, including v4-mapped forms) are admitted without Geo data, inbound and outbound. Public addresses still require valid local DB-IP data and a French range and fail closed otherwise. This lets an operator's machines, VMs and LAN form a mesh (battle tests, home networks); it grants no consensus authority and changes no wire format. | Frozen; implemented |
| DEC-286 | The PoA block production loop signs a block only when its candidate pool is non-empty; it wakes on candidate admission, signer changes and finalized-head changes, with timed waits only for the local block interval or transient retry deadline (R5, 2026-10-05). Idle production does not poll. Empty blocks every second made the chain grow by 86,400 blocks per day and every new node had to download and verify them. Block height no longer tracks time: block-counted windows (AccountCreate work epoch of 1,024 blocks, name reveal depth) last longer on a quiet network. A manual single-block finalization may still be empty. | Frozen; implemented |
| DEC-287 | Every Full Node, desktop included, accepts inbound peers: it listens on 29461 by default, or on any free port when that one is taken, and announces the port in HELLO (82 bytes, `listen_port` u16 LE, 0 = not listening). A receiver records the connection IP with that port only as a candidate, connects back, and shares the address with others only after a successful same-network handshake, so unreachable or forged endpoints are never advertised. The bootstrap is an ordinary peer everyone knows in advance and shares inbound peers like any node. No node announces the PoA key. Wire change: all DEVNET nodes update together. | Frozen; implemented |
| DEC-288 | Storage providers come from every known Full Node, not only the eight outbound mesh sessions: each sync pass proves the StorageId of one discovered, unconnected endpoint in a short session on a separate thread (each endpoint at most every 10 min, usable for 30 min). Replicas of one chunk sit at distinct network addresses and never on a loopback address (this machine): distinct StorageIds behind one address are one failure domain. Too few diverse providers keeps content Securing. Memory-only component fixtures, whose providers share loopback, are exempt. | Frozen; implemented |

### Network operations and topology

| ID | Decision | Status |
|---|---|---|
| DEC-289 | Built-in observation uses bounded direct request/reply reports over existing admitted same-network TLS mesh sessions. Reports are untrusted non-canonical process aggregates, with fresh request/session binding, expiry and per-IP/global limits; no NodeID/pseudonym, roles, capability negotiation, StorageId proof, Identity signature or PoA route. Consolidation selects one report per numeric transport-address group and labels partial declarations, never a node census or independent-host proof; canonical operation/register streams and memory/disk resources are not summed. The exact minimized contract and coordinated software-upgrade/privacy acceptance gates are `NETWORK_OBSERVATION_REPORTS.md`. Relayed reporting and deduplication across different addresses remain open. | Frozen target; codec/cache/guard/direct TLS/runtime groups/totals/immutable snapshot implemented; live polling/UI/release open, not deployed |
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
| DEC-257 | All protocol fees transfer atomically from payer System Balance to the unique genesis-granted `cybou` Central Authority allocation before claim, or its claimant's spendable Balance after claim. DEV OnboardingPool starts at 100M CYBOU and only funds onboarding. No batching, fee burn, validator/provider rewards or fee-funded onboarding. State is the only canonical encoding; a new official genesis requires a new NetworkID. | Superseded by DEC-277/DEC-278 (Treasury monetary base, storage rent to providers); removed from code in M5 |

Superseded model (still run by the deployed DEVNET binary until M7):

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
DEV OnboardingPool = 100,000,000 CYBOU (genesis only; never replenished by fees)
100% protocol fee: payer System Balance -> Central Authority spendable Balance
Before claim: fees accumulate in the unique genesis allocation labelled cybou.
After claim: fees credit that allocation claimant's ordinary AccountState Balance.
```

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

## Superseded decision history

The following decisions recorded during development iterations have been superseded or rejected:

| Prior ID | Prior topic | Current status | Superseding decision |
|---|---|---|---|
| DEC-195 | Canonical finality under Network-Root-authorized key per Authority epoch | Superseded | Superseded by DEC-247 (PoA under genesis-authorized key, sole canonical finalizer) |
| DEC-207 | Authority is read-only metric granting no protocol power | Superseded | Superseded by DEC-248 (AUTH > 10,000,000 qualifies Identity for Validation; grants no finality) |
| DEC-208 | Canonical Age/activity/lock accumulators, liveness/storage evidence, and penalty debt | Rejected | Superseded by DEC-217, DEC-248 (AUTH stored directly in AccountState; no derived accumulators or evidence) |
| DEC-209 | Canonical NodeID binding and per-Account service-node registry | Rejected | Superseded by DEC-193, DEC-217 (No protocol device/service-node registry) |
| DEC-210 | Authority-derived Protocol/Storage/Bandwidth budgets, reservations, and tickets | Rejected | Superseded by DEC-217, DEC-248 (No canonical budgets, reservations, or tickets) |
| DEC-211 | Optional signed Validation in active capability story | Superseded | Superseded by DEC-249 (Validation is a signature after independent local execution) |
| DEC-214 | Any full node may issue Validation opinion | Superseded | Superseded by DEC-248, DEC-249 (Only finalized Authority > 10M qualifies an Identity for Validation) |
| DEC-215 | Validator-qualified threshold presentation | Superseded | Superseded by DEC-248, DEC-249 (No threshold policy; one eligible signature marks Validated) |
| DEC-250 | Optional provisional storage admission upon eligible Validation signatures, with purge/rollback on PoA rejection | Rejected | Superseded by DEC-249 (Validation creates no provisional state; storage admission is finalized-RootPublication only) |
| DEC-216 | Validation does not alter canonical state | Consolidated | Superseded by DEC-247, DEC-249 (Validation never changes state; PoA is sole canonical truth) |
| DEC-218 | Official VPS runs one bootstrap relay/cache service only and never finalizes | Superseded | Superseded by DEC-245 (Bootstrap is ordinary full peer with known locator) |
| DEC-220 | Complex bootstrap creation with multi-identity binding | Superseded | Superseded by DEC-244, DEC-245, DEC-246 (Bootstrap is ordinary peer; signed genesis fixes network) |
| DEC-221 | Network replacement with multi-party proof and history archive | Superseded | Superseded by DEC-238, DEC-244 (New network replaces domain cleanly and wipes all local state; no migration) |
| DEC-222 | Bootstrap transport identity pinned through release | Superseded | Superseded by DEC-244, DEC-245 (Bootstrap IP:port + TLS pin for transport only) |
| DEC-224 | Central Authority route session-authenticated without persistent IP | Superseded | Superseded by DEC-258 (No PoA transport authority proof or route) |
| DEC-227 | Bootstrap/Validation as full-node consensus capabilities | Superseded | Superseded by DEC-245, DEC-249 (No bootstrap role announcement; Validation is a signature, not a capability) |
| DEC-228 | Genesis authorizes 1-4 bootstrap Identities | Superseded | Superseded by DEC-246 (Ordinary Identity with initial Authority in genesis; no consensus bootstrap roster) |
| DEC-229 | Genesis bootstrap grant binds AccountID + RecoveryKeyID | Superseded | Superseded by DEC-246 (No bootstrap grants or consensus Identity roster) |
| DEC-230 | Bootstrap authorization follows AccountID through IdentityRotate | Superseded | Superseded by DEC-246 (No special bootstrap AccountID or Identity role) |
| DEC-231 | Initial IP/SPKI locator pins are pre-genesis only | Superseded | Superseded by DEC-244, DEC-245 (Official profiles have known bootstrap locator IP:port + SPKI for transport authentication) |
| DEC-234 | Canonical bootstrap roster fixed by genesis in the former design | Superseded | Superseded by DEC-246 (No genesis bootstrap roster) |
| DEC-236 | Official profiles pin bootstrap locator, TLS SPKI and Network Root R | Superseded | Superseded by DEC-244, DEC-245 (Profiles DEVNET and MAINNET pin NetworkID = Network Public Key, bootstrap IP:port + SPKI) |
| DEC-225 | Only Authority's live session admits operations into canonical pending state | Superseded | Superseded by DEC-251 (PoA owns no canonical pending state; acts as finalizer only) |
| DEC-237 | Bootstrap distributes root-signed OfficialNetworkBindings | Superseded | Superseded by DEC-245 (Bootstrap is ordinary CYBOU full peer; no separate service or consensus binding) |
| DEC-239 | R signs Authority assignments {epoch, activation_height, P} | Superseded | Superseded by DEC-244, DEC-247 (Network genesis signed offline by owner fixes PoA key P) |
| DEC-241 | Only Central Authority has canonical pending state | Superseded | Superseded by DEC-251 (Only finalized state is canonical; no canonical pending state) |
| DEC-242 | Advisory Validation is deferred non-canonical functionality | Superseded | Superseded by DEC-248, DEC-249 (Validation is active signature evidence after local execution) |
| DEC-243 | Root-signed OfficialNetworkBinding fixes generation, epoch, network definition and P | Superseded | Superseded by DEC-244 (NetworkID = Network Public Key; owner signs genesis offline) |
