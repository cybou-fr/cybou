# 24 â€” Current product and protocol decisions

## Uniform Full Node invariant

CYBOU defines exactly one network node type: Full Node. Every Full Node
implements the complete CYBOU P2P baseline: blocks, announcements,
discovery, operation relay, Validation transport and encrypted storage. There
is no capability bitmap and no network role announcement. Storage is intrinsic;
capacity is local policy. Bootstrap is only a known locator of an ordinary Full
Node. Validation requires an Identity with finalized AUTH > 1,000,000. PoA is
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
| DEC-151 | Account creation funds System Balance from OnboardingPool and does not mint supply. | Frozen |
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
| DEC-205 | Development targets 1 remote full replica; Beta targets 2 independent remote full replicas (plus local copy = 3 physical copies total). Local encrypted cache does not count toward remote durability; Beta erasure coding is disabled. | Frozen |
| DEC-206 | Placement, provider health, audit and repair are StorageService policy, not consensus state. | Frozen |
| DEC-212 | StorageId is BLAKE3 of the existing domain-separated STORAGE public key encoding, proven on demand with a fresh challenge bound to TLS exporter and both HELLOs. It proves replica independence only; all Full Nodes implement storage. | Frozen |
| DEC-213 | Local blob retention is a generic node-local pin/cache registry keyed by opaque (holder, reference) tags; GC evicts only unpinned, non-admitted cache entries past a grace period. ChunkStore stays free of application semantics. | Frozen |
| DEC-217 | Authority creates no canonical reservations, tickets, resource budgets, or per-I/O accounting. Provider limits are local policy. | Frozen |
| DEC-244 | Official networks are DEVNET and MAINNET. Network identity is immutable `NetworkID = Network Public Key`. For each NetworkID, exactly one signed genesis is valid and immutable for the lifetime of that network. The Network Private Key is strictly offline, never online, and used solely by the network owner to sign the immutable genesis at network creation. | Frozen |
| DEC-245 | Bootstrap is an ordinary CYBOU full peer running the same executable and CYBOU P2P protocol; it has no network-role announcement, no consensus role, and no special protocol capability. Known IP:port provides transport discovery only. | Frozen |
| DEC-246 | GenesisAllocation may assign initial AUTH to designated ordinary Identities. AccountCreate claims it exactly once. Such an allocation is a genesis decision, not a property of the bootstrap role; there are no consensus bootstrap grants or bootstrap Identity roles. | Frozen |
| DEC-247 | Canonical finality is single-operator hybrid-PQ PoA under the genesis-authorized PoA key. The PoA finalizer MUST execute every candidate independently, trusts no validator or peer state, and remains the sole canonical finalizer without BFT or validator quorums. Validation signatures are never sufficient for finalization. | Frozen |
| DEC-248 | AUTH is a canonical non-transferable account value stored in AccountState and committed by the state root, independent of the 100B CYBOU supply. It changes only through deterministic finalized transitions: GenesisAllocation, +1 AUTH to the authorizing account of finalized utility operations (RootPublication, SystemLock), capped at +1 per account per block, and one PoA-signed `PoaAuthAdjustment` operation with GRANT (+N) or BURN (-N, floor 0) that itself earns no AUTH. There is no AUTH transfer. Finalized AUTH > 1,000,000 makes an Identity eligible to sign Validation; AUTH never grants PoA finalization power. Automatic penalties are not frozen. | Frozen |
| DEC-249 | Every full node independently validates and executes every candidate operation against its latest finalized state and relays only locally valid candidates. Validation is an additional signature by an eligible Identity over NetworkBinding, OperationID, finalized base BlockID and its AccountID, made only after its own node validated the operation. It never substitutes local or PoA execution, never changes state, and creates no provisional state. There is no `validation.enabled`, `min_signatures` policy or block-candidate Validation. | Frozen |
| DEC-252 | For each NetworkID exactly one signed genesis is valid. Genesis is immutable for the lifetime of that network. There is no `genesis_generation`, re-genesis, in-place genesis replacement, or `NetworkTrustStore`. Any different genesis requires a new Network Key and therefore a new NetworkID. | Frozen |
| DEC-253 | Each official network is compiled into the client as public constants: exact Network Public Key (`NetworkID`), immutable signed `NetworkGenesis` object, initial genesis state, and bootstrap locators (IP:port + TLS SPKI). Runtime uses no external official network/genesis file or separate genesis digest profile pin. Bootstrap never supplies genesis. | Frozen |
| DEC-254 | Provisioning generates the Network and ordinary `cybou.cybou` Identity private material once. Secret material lives only under gitignored `/private/`; Git contains public keys, public Identity data, and signed genesis constants. The Network Private Key remains strictly offline. | Frozen |
| DEC-255 | DEVNET is the enabled official profile with bootstrap locator `51.255.46.58:29461`. MAINNET is unprovisioned, has no bootstrap locator, and is disabled in the GUI until its keys, genesis, and bootstrap are created. | Frozen |
| DEC-256 | `cybou.cybou` is an ordinary account-level Identity with AccountID, Recovery, Authorization, KEM, Mail/support, and a distinct PoA signing key role from its mnemonic. It is not a separate PoA Identity entity. Consensus finalization right is determined only by the PoA public key authorized in genesis, never by name or AUTH value. | Frozen |
| DEC-258 | Every network participant is a Full Node implementing the uniform CYBOU P2P baseline without a capability bitmap or network role. Storage is intrinsic and quota-controlled. StorageId is proven on demand only for storage. PoA is private-key possession with durable signing safety; signer toggles never reconnect peers. Peer sync completion is advisory and does not gate Identity creation. | Frozen |

| DEC-259 | Runtime and StateStore take verified signed genesis as their sole network definition; its signed specification digest is the chain height-zero tip and durable signing-journal anchor. No synthetic genesis block identifier exists. | Frozen |
| DEC-260 | CYBOU P2P has compact IDs 1–26, batched consecutive GET_BLOCKS with strict BLOCKS_END count, and one shared OP_META/OP_DATA/OP_RESULT path for submission and polling. Configured peers are one ordered endpoint/optional-pin vector; discovered peers remain bounded and separate. | Frozen |
| DEC-261 | PublicationService stages directly into pinned local encrypted blobs and saves the ordered authorization leaf list once in encrypted Application DB. A transient O(N) Merkle tree generates individual O(log N) proofs; no persisted proof levels or separate staging service exists. | Frozen |
| DEC-262 | RootPublication wire layout, encrypted ROOT/INDEX schema and private Mail/Files/RecoveryBridge schema are fixed-order bounded binary layouts. Reject unknown types and trailing bytes; no generic CBOR parser or legacy decoder remains. BLAKE3 is unchanged. | Frozen |
| DEC-263 | Native Hash256 is an opaque canonical 32-byte value; raw-byte order, comparison and forward hex agree. No numeric padding or reverse-byte display. Unused Bitcoin blob, span, hex, util and compat substrate is removed. | Frozen |
| DEC-264 | Single-current-baseline architecture: if only one supported form of a structure, wire message, state, or schema exists, it has no version identifier. Version fields appear only when multiple formats actually coexist or temporary migration is required, and are removed once the migration window closes. Code does not record development history in type names or wire bytes. Cryptographic domain separation strings transition to eternal unversioned names exclusively during coordinated network genesis resets before MAINNET. | Frozen |
| DEC-265 | Production Full Node always has a positive storage allocation. Configured capacity of zero is forbidden in production and restricted to memory-only unit tests. Default allocation is automatic based on available storage. Nodes with depleted local disk space reject new inbound admissions without changing node type, consensus authority, or mesh relay participation. | Frozen |
| DEC-266 | Official network provisioning (Network Root key generation, NetworkID derivation, and genesis signing) is strictly offline tooling (`cybou-provision`), not a production `cybou` command or runtime capability. Production binaries contain only compiled public official network constants and cannot generate or re-provision official networks. | Frozen |

### Network operations and topology

| ID | Decision | Status |
|---|---|---|
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
| DEC-257 | All protocol fees transfer atomically from payer System Balance to the unique genesis-granted `cybou` Central Authority allocation before claim, or its claimant's spendable Balance after claim. DEV OnboardingPool starts at 100M CYBOU and only funds onboarding. No batching, fee burn, validator/provider rewards or fee-funded onboarding. State is the only canonical encoding; a new official genesis requires a new NetworkID. | Frozen |

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
DEV OnboardingPool = 100,000,000 CYBOU (genesis only; never replenished by fees)
100% protocol fee: payer System Balance -> Central Authority spendable Balance
Before claim: fees accumulate in the unique genesis allocation labelled cybou.
After claim: fees credit that allocation claimant's ordinary AccountState Balance.
```

Runtime policy governance is not part of the current target. Network parameters
remain immutable.

---

## Superseded decision history

The following decisions recorded during development iterations have been superseded or rejected:

| Prior ID | Prior topic | Current status | Superseding decision |
|---|---|---|---|
| DEC-195 | Canonical finality under Network-Root-authorized key per Authority epoch | Superseded | Superseded by DEC-247 (PoA under genesis-authorized key, sole canonical finalizer) |
| DEC-207 | Authority is read-only metric granting no protocol power | Superseded | Superseded by DEC-248 (AUTH > 1,000,000 qualifies Identity for Validation; grants no finality) |
| DEC-208 | Canonical Age/activity/lock accumulators, liveness/storage evidence, and penalty debt | Rejected | Superseded by DEC-217, DEC-248 (AUTH stored directly in AccountState; no derived accumulators or evidence) |
| DEC-209 | Canonical NodeID binding and per-Account service-node registry | Rejected | Superseded by DEC-193, DEC-217 (No protocol device/service-node registry) |
| DEC-210 | Authority-derived Protocol/Storage/Bandwidth budgets, reservations, and tickets | Rejected | Superseded by DEC-217, DEC-248 (No canonical budgets, reservations, or tickets) |
| DEC-211 | Optional signed Validation in active capability story | Superseded | Superseded by DEC-249 (Validation is a signature after independent local execution) |
| DEC-214 | Any full node may issue Validation opinion | Superseded | Superseded by DEC-248, DEC-249 (Only finalized Authority > 1M qualifies an Identity for Validation) |
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
