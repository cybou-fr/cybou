# 24 — Current product and protocol decisions

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
| DEC-212 | A storage provider is identified by `ProviderID = BLAKE3(provider public key)`, proven per CYP2 session; placement stores ProviderID plus last endpoint and the replica target counts distinct ProviderIDs. | Frozen |
| DEC-213 | Local blob retention is a generic node-local pin/cache registry keyed by opaque (holder, reference) tags; GC evicts only unpinned, non-admitted cache entries past a grace period. ChunkStore stays free of application semantics. | Frozen |
| DEC-217 | Authority creates no canonical reservations, tickets, grants, rewards, or penalties. Provider limits are local policy. | Frozen |
| DEC-244 | Official networks are DEVNET and MAINNET. Network identity is immutable `NetworkID = Network Public Key`. The Network Private Key is strictly offline, never online, and used solely by the network owner to sign genesis/re-genesis. | Frozen |
| DEC-245 | Bootstrap is an ordinary CYBOU full peer running the same executable and CYP2 protocol; it has no `CAP_BOOTSTRAP`, no consensus role, and no special protocol capability. Known IP:port provides transport discovery only. | Frozen |
| DEC-246 | Genesis may assign initial Authority to designated ordinary Identities (e.g., DEV bootstrap Identity initial Authority = 1,000,001). There are no consensus bootstrap grants or bootstrap Identity roles. | Frozen |
| DEC-247 | Canonical finality is single-operator hybrid-PQ PoA under the genesis-authorized PoA key. The PoA finalizer executes every candidate independently, trusts no validator or peer state, and remains the sole canonical finalizer without BFT or validator quorums. | Frozen |
| DEC-248 | Authority is a deterministic property derived exclusively from PoA-finalized history. Finalized Authority > 1,000,000 qualifies an Identity to sign provisional Validation attestations. Authority never grants PoA finalization power. | Frozen |
| DEC-249 | Validation is optional, non-canonical pre-finalization evidence evaluated locally against latest finalized state eligibility. In any conflict between provisional Validation and PoA, provisional state is discarded, provisional effects are rolled back, and PoA finality is adopted unconditionally. | Frozen |
| DEC-250 | Storage admission is finality-first by default. Nodes enabling provisional validation policy may optionally admit and stage chunks upon sufficient eligible Validation signatures, purging and rolling back on PoA rejection. | Frozen |

### Network operations and topology

| ID | Decision | Status |
|---|---|---|
| DEC-219 | The Central Authority desktop operates the genesis-authorized PoA key after unlock and local chain verification. The Network Private Key is separate and strictly offline. | Frozen target |
| DEC-226 | Authority mobility does not permit concurrent independent signers sharing one PoA key; one active signer and durable anti-equivocation safety remain required. | Frozen target |
| DEC-232 | All public inbound and outbound P2P admission is France-only for every peer capability; the rule is local networking policy, not consensus state. | Frozen target |
| DEC-233 | Known VPN/proxy/Tor filtering is optional local policy using local data. It never affects consensus, Identity, Authority, or PoA. | Frozen target |
| DEC-235 | Central Authority is identified only by the PoA key. Its live route is session-authenticated and discarded at disconnect; no persistent Authority endpoint or NodeID is stored. | Frozen target |
| DEC-238 | Cross-network migration does not exist. A newer valid official network replaces all local network-bound state, wiping everything (chain, genesis, Identity, vault, AccountID, balances, names, Mail, Files, application DB, peer DB, storage metadata). | Frozen target |
| DEC-240 | Ordinary peers form a direct P2P mesh after initial discovery. Bootstrap is an initial rendezvous peer, not a mandatory traffic intermediary. | Frozen target |
| DEC-251 | PoA owns no special canonical pending state and acts purely as a finalizer. Candidate operations or blocks propagate across ordinary peers via bounded volatile relay queues. Only finalized state is canonical; pending and provisional states are never canonical. | Frozen target |

### Fixed economics

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
4 fee units -> 3 Security + 1 Onboarding
```

Runtime policy governance is not part of the current target. Network parameters
remain immutable.

---

## Superseded decision history

The following decisions recorded during development iterations have been superseded or rejected:

| Prior ID | Prior topic | Current status | Superseding decision |
|---|---|---|---|
| DEC-195 | Canonical finality under Network-Root-authorized key per Authority epoch | Superseded | Superseded by DEC-247 (PoA under genesis-authorized key, sole canonical finalizer) |
| DEC-207 | Authority is read-only metric granting no protocol power | Superseded | Superseded by DEC-248 (Authority qualifies Identity for provisional Validation when >1,000,000; grants no finality) |
| DEC-208 | Canonical Age/activity/lock accumulators, liveness/storage evidence, and penalty debt | Rejected | Superseded by DEC-217, DEC-248 (Authority derived from finalized state; no canonical accumulators or evidence) |
| DEC-209 | Canonical NodeID binding and per-Account service-node registry | Rejected | Superseded by DEC-193, DEC-217 (No protocol device/service-node registry) |
| DEC-210 | Authority-derived Protocol/Storage/Bandwidth budgets, reservations, and tickets | Rejected | Superseded by DEC-217, DEC-248 (No canonical budgets, reservations, or tickets) |
| DEC-211 | Optional signed Validation in active capability story | Superseded | Superseded by DEC-249 (Validation is provisional pre-finalization evidence; superseded by DEC-249) |
| DEC-214 | Any full node may issue Validation opinion | Superseded | Superseded by DEC-248, DEC-249 (Only finalized Authority > 1M qualifies an Identity for Validation) |
| DEC-215 | Validator-qualified threshold presentation | Superseded | Superseded by DEC-248, DEC-249 (Threshold is local peer policy over eligible signatures) |
| DEC-216 | Validation does not alter canonical state | Consolidated | Superseded by DEC-247, DEC-249 (Provisional validation discarded on PoA conflict; PoA is sole canonical truth) |
| DEC-218 | Official VPS runs one bootstrap relay/cache service only and never finalizes | Superseded | Superseded by DEC-245 (Bootstrap is ordinary full peer with known locator) |
| DEC-220 | Complex bootstrap creation with multi-identity binding | Superseded | Superseded by DEC-244, DEC-245, DEC-246 (Bootstrap is ordinary peer; signed genesis fixes network) |
| DEC-221 | Network replacement with multi-party proof and history archive | Superseded | Superseded by DEC-238, DEC-244 (New network replaces domain cleanly and wipes all local state; no migration) |
| DEC-222 | Bootstrap transport identity pinned through release | Superseded | Superseded by DEC-244, DEC-245 (Bootstrap IP:port + TLS pin for transport only) |
| DEC-223 | Headless finalizer/provider processes LAB-only; official VPS not a finalizer or provider | Superseded | Superseded by DEC-240, DEC-245 (All nodes run same core; bootstrap is ordinary full peer) |
| DEC-224 | Central Authority route session-authenticated without persistent IP | Consolidated | Superseded by DEC-235 (Ephemeral session route invariant preserved and clarified) |
| DEC-227 | Bootstrap/Validation as full-node consensus capabilities | Superseded | Superseded by DEC-245, DEC-249 (No CAP_BOOTSTRAP; Validation is provisional pre-finalization) |
| DEC-228 | Genesis authorizes 1-4 bootstrap Identities | Superseded | Superseded by DEC-246 (Ordinary Identity with initial Authority in genesis; no consensus bootstrap roster) |
| DEC-229 | Genesis bootstrap grant binds AccountID + RecoveryKeyID | Superseded | Superseded by DEC-246 (No bootstrap grants or consensus Identity roster) |
| DEC-230 | Bootstrap authorization follows AccountID through IdentityRotate | Superseded | Superseded by DEC-246 (No bootstrap AccountID or CAP_BOOTSTRAP Identity role) |
| DEC-231 | Initial IP/SPKI locator pins are pre-genesis only | Superseded | Superseded by DEC-244, DEC-245 (Official profiles have known bootstrap locator IP:port + SPKI for transport authentication) |
| DEC-234 | Canonical bootstrap roster fixed by genesis in v1 | Superseded | Superseded by DEC-246 (No genesis bootstrap roster) |
| DEC-236 | Official profiles pin bootstrap locator, TLS SPKI and Network Root R | Superseded | Superseded by DEC-244, DEC-245 (Profiles DEVNET and MAINNET pin NetworkID = Network Public Key, bootstrap IP:port + SPKI) |
| DEC-225 | Only Authority's live session admits operations into canonical pending state | Superseded | Superseded by DEC-251 (PoA owns no canonical pending state; acts as finalizer only) |
| DEC-237 | Bootstrap distributes root-signed OfficialNetworkBindings | Superseded | Superseded by DEC-245 (Bootstrap is ordinary CYBOU full peer; no separate service or consensus binding) |
| DEC-239 | R signs Authority assignments {epoch, activation_height, P} | Superseded | Superseded by DEC-244, DEC-247 (Network genesis signed offline by owner fixes PoA key P) |
| DEC-241 | Only Central Authority has canonical pending state | Superseded | Superseded by DEC-251 (Only finalized state is canonical; no canonical pending state) |
| DEC-242 | Advisory Validation is deferred non-canonical functionality | Superseded | Superseded by DEC-248, DEC-249 (Validation is active provisional pre-finalization evidence) |
| DEC-243 | Root-signed OfficialNetworkBinding fixes generation, epoch, network definition and P | Superseded | Superseded by DEC-244 (NetworkID = Network Public Key; owner signs genesis offline) |
