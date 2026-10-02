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
| DEC-195 | Canonical finality is single-operator hybrid-PQ PoA under the Network-Root-authorized key for each Authority epoch; full nodes validate independently, without BFT or a ValidatorSet. | Frozen |
| DEC-197 | ROOT/INDEX metadata contains private ordered ChunkIDs; DATA contains application bytes; ChunkID is full BLAKE3 of stored ciphertext. | Frozen |
| DEC-198 | Mail has no consensus operation or per-message canonical state; private Mail is discovered through generic RootPublication. | Frozen |
| DEC-199 | The physical ChunkStore is one encrypted content-addressed network store and has no user-facing own/foreign semantic classification. | Frozen |
| DEC-200 | Each unlocked Identity uses a separate encrypted rebuildable Application DB; GUI never browses provider ChunkStore contents. | Frozen |
| DEC-201 | One RootPublication may authorize chunks from multiple private encrypted trees; only the main root is capsule-addressed. This does not create a new wire entity. | Frozen |
| DEC-202 | Recoverable publisher content uses an application-layer self capsule. | Frozen |
| DEC-203 | Files persistent private history uses a minimal ordered mutation model (`UPSERT_ITEM`, `DELETE_ITEM`) over canonical PoA order. | Frozen |
| DEC-204 | Identity rotation must protect required historical KEM recovery material before rotation when clean recovery needs old epochs. | Frozen |
| DEC-205 | Development targets 1 remote full replica; Beta targets 2 independent remote full replicas. Local encrypted cache does not count (it is normally a further physical copy); Beta erasure coding is disabled. | Frozen |
| DEC-206 | Placement, provider health, audit and repair are StorageService policy, not consensus state. | Frozen |
| DEC-212 | A storage provider is identified by `ProviderID = BLAKE3(provider public key)`, proven per CYP2 session; placement stores ProviderID plus last endpoint and the replica target counts distinct ProviderIDs. | Frozen |
| DEC-213 | Local blob retention is a generic node-local pin/cache registry keyed by opaque (holder, reference) tags; GC evicts only unpinned, non-admitted cache entries past a grace period. ChunkStore stays free of application semantics. | Frozen |
| DEC-207 | Authority is a read-only metric derived from finalized account history; it grants no PoA or resource power. | Frozen |
| DEC-217 | Authority creates no canonical reservations, tickets, grants, rewards, or penalties. Provider limits are local policy. | Frozen |

### Network bootstrap and operations

| ID | Decision | Status |
|---|---|---|
| DEC-219 | The Central Authority desktop operates the current root-authorized PoA key P after unlock and local chain verification. The Network Root R is a separate key and never signs blocks. | Frozen target; implementation/cutover pending |
| DEC-225 | Any full node may relay exact operations hop by hop using bounded volatile queues; only the Authority's live session admits them to canonical pending state. No durable shared pending-operation pool or distributed mempool. | Frozen target |
| DEC-226 | Authority mobility does not permit concurrent independent signers sharing one PoA key; one active signer and durable anti-equivocation safety remain required. | Frozen target |
| DEC-232 | All public inbound and outbound P2P admission is France-only for every peer capability; the rule is local networking policy, not consensus state. | Frozen target |
| DEC-233 | Known VPN/proxy/Tor filtering is optional local policy using local data. It never affects consensus, Identity, Authority, or PoA. | Frozen target |
| DEC-235 | Central Authority is identified only by the PoA key. Its live route is session-authenticated and discarded at disconnect; no persistent Authority endpoint or NodeID is stored. | Frozen target |
| DEC-236 | Each official network profile (DEVNET, TESTNET, MAINNET) pins bootstrap locator(s), TLS SPKI and an immutable Network Root public key R. | Frozen target |
| DEC-237 | Bootstrap distributes root-signed OfficialNetworkBindings and initial peers. It is rendezvous infrastructure, not an authority, consensus participant, validator, or Identity entity. | Frozen target |
| DEC-238 | Cross-network migration does not exist. A newer valid official network replaces all local network-bound state, wiping everything (chain, genesis, Identity, vault, AccountID, balances, names, Mail, Files, application DB, peer DB, storage metadata). | Frozen target |
| DEC-239 | R signs each Authority assignment `{epoch, activation_height, P}`. Increasing authority_epoch changes P without wiping the network; historical blocks use the assignment active at their height. A PoA key cannot appoint its successor. | Frozen target |
| DEC-240 | Ordinary peers form a direct P2P mesh after initial discovery. Bootstrap is an initial rendezvous service, not a mandatory traffic intermediary. | Frozen target |
| DEC-241 | Only the Central Authority has canonical pending state. Operation relay through ordinary peers is bounded and volatile; there is no distributed mempool. | Frozen target |
| DEC-242 | Advisory Validation is deferred non-canonical functionality and is not part of the active network lifecycle or node capabilities. | Frozen target |
| DEC-243 | A root-signed OfficialNetworkBinding fixes generation, authority_epoch, exact network definition and current P. R alone authenticates official network/Authority assignments; R never finalizes blocks and P cannot replace a network. | Frozen target |

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
| DEC-208 | Canonical Age/activity/lock accumulators, liveness/storage evidence, and penalty debt | Rejected | Superseded by DEC-207, DEC-217 (Authority is read-only derived metric; no canonical accumulators or evidence) |
| DEC-209 | Canonical NodeID binding and per-Account service-node registry | Rejected | Superseded by DEC-193, DEC-217 (No protocol device/service-node registry) |
| DEC-210 | Authority-derived Protocol/Storage/Bandwidth budgets, reservations, and tickets | Rejected | Superseded by DEC-207, DEC-217 (No canonical budgets, reservations, or tickets) |
| DEC-211 | Optional signed Validation in active capability story | Deferred | Superseded by DEC-242 (Validation deferred; not part of active node capabilities or network lifecycle) |
| DEC-214 | Any full node may issue Validation opinion | Deferred | Superseded by DEC-242 (Deferred non-canonical functionality) |
| DEC-215 | Validator-qualified threshold presentation | Deferred | Superseded by DEC-242 (Deferred non-canonical functionality) |
| DEC-216 | Validation does not alter canonical state | Deferred | Superseded by DEC-242 (Deferred non-canonical functionality) |
| DEC-218 | Official VPS runs one bootstrap relay/cache service only and never finalizes | Superseded | Superseded by DEC-237 (Bootstrap is rendezvous / signed state distributor) |
| DEC-220 | Complex bootstrap creation with multi-identity binding | Superseded | Superseded by DEC-236, DEC-237, DEC-243 (Bootstrap EMPTY/BOUND with activation code + root-signed binding) |
| DEC-221 | Network replacement with multi-party proof and history archive | Superseded | Superseded by DEC-237, DEC-238 (Newer generation replaces network and wipes all local state; no migration) |
| DEC-222 | Bootstrap transport identity pinned through release | Superseded | Superseded by DEC-236, DEC-237, DEC-243 (IP:port + TLS/SPKI transport pin; state verified by R) |
| DEC-223 | Headless finalizer/provider processes LAB-only; official VPS not a finalizer or provider | Superseded | Superseded by DEC-237, DEC-240 (All nodes run same core; bootstrap is known rendezvous service) |
| DEC-224 | Central Authority route session-authenticated without persistent IP | Consolidated | Superseded by DEC-235 (Ephemeral session route invariant preserved and clarified) |
| DEC-227 | Bootstrap/Validation as full-node consensus capabilities | Superseded | Superseded by DEC-237, DEC-242 (Bootstrap is known rendezvous; Validation is deferred; capabilities are storage + PoA) |
| DEC-228 | Genesis authorizes 1-4 bootstrap Identities | Superseded | Superseded by DEC-237 (Bootstrap is rendezvous infrastructure, not consensus Identity) |
| DEC-229 | Genesis bootstrap grant binds AccountID + RecoveryKeyID | Superseded | Superseded by DEC-237 (No bootstrap grants or consensus Identity roster) |
| DEC-230 | Bootstrap authorization follows AccountID through IdentityRotate | Superseded | Superseded by DEC-237 (No bootstrap AccountID or CAP_BOOTSTRAP Identity role) |
| DEC-231 | Initial IP/SPKI locator pins are pre-genesis only | Superseded | Superseded by DEC-236, DEC-237 (Official profiles have known bootstrap locator IP:port + SPKI for transport authentication) |
| DEC-234 | Canonical bootstrap roster fixed by genesis in v1 | Superseded | Superseded by DEC-237 (No genesis bootstrap roster) |
