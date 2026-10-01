# 24 — Current product and protocol decisions

This register contains active decisions only. Superseded architecture remains in
Git history.

## Product

| ID | Decision | Status |
|---|---|---|
| DEC-173 | Mail attachments use the shared encrypted content substrate; Mail is not a separate network transport. | Frozen |
| DEC-179 | Mail and Files use familiar Gmail/Google Drive interaction patterns without copying their branding or centralized trust assumptions. | Frozen |
| DEC-180 | Normal UI presents user actions/outcomes; protocol details use progressive disclosure. | Frozen |
| DEC-181 | Finality alone is not `Sent`/`Protected`; remote durability is required. | Frozen |
| DEC-185 | Files is the Beta file-management surface; Backup is post-Beta. | Frozen |
| DEC-194 | Mail and Files are product surfaces over one Identity, finalized state and one encrypted content substrate. | Frozen |

## Identity and onboarding

| ID | Decision | Status |
|---|---|---|
| DEC-150 | Account creation is permissionless and uses protocol-native anti-Sybil work. | Frozen |
| DEC-151 | Account creation funds System Balance from OnboardingPool and does not mint supply. | Frozen |
| DEC-152 | Identity Recovery, Authorization, KEM, PoA, Release Signing and Treasury use separate key roles. | Frozen |
| DEC-165 | AccountID is a random stable nonzero 256-bit identifier independent of mnemonic and keys. | Frozen |
| DEC-169 | `.cybou` names are protocol names finalized through commit/work/reveal. | Frozen |
| DEC-193 | Device is not a protocol Identity entity; one account has one current authorization/KEM key set. | Frozen |

## Core protocol

| ID | Decision | Status |
|---|---|---|
| DEC-195 | Canonical finality is genesis-bound single-operator hybrid-PQ PoA with independently validating full nodes; no BFT/ValidatorSet runtime. | Frozen |
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
| DEC-213 | Local blob retention is a generic node-local pin/cache registry keyed by opaque (holder, reference) tags; GC evicts only unpinned, non-admitted cache entries past a grace period. ChunkStore stays free of application semantics. | Frozen |
| DEC-212 | A storage provider is identified by `ProviderID = BLAKE3(provider public key)`, proven per CYP2 session; placement stores ProviderID plus last endpoint and the replica target counts distinct ProviderIDs. | Frozen |
| DEC-207 | Authority is a read-only metric derived from finalized account history; it grants no PoA or resource power. | Frozen |
| DEC-208 | Canonical Age/activity/lock accumulators, liveness/storage evidence, and penalty debt. | Superseded / rejected |
| DEC-209 | Canonical NodeID binding and per-Account service-node registry. | Superseded / rejected |
| DEC-210 | Authority-derived Protocol/Storage/Bandwidth budgets, reservations, and tickets. | Superseded / rejected |
| DEC-211 | Optional signed Validation is advisory evidence verified and trusted locally; PoA alone establishes finality. | Frozen |
| DEC-214 | Any full node may issue a Validation opinion. There is no ValidatorSet, registry, protocol role, or admission operation. | Frozen |
| DEC-215 | A recipient may label a signer with locally derived Authority >= 1,000,000 as validator-qualified. This is local presentation policy only. | Frozen |
| DEC-216 | Validation does not alter canonical state, PoA admission, finality, resource allocation, or remote storage authorization. | Frozen |
| DEC-217 | Authority creates no canonical reservations, tickets, grants, rewards, or penalties. Provider limits are local policy. | Frozen |

## Network bootstrap and operations

| ID | Decision | Status |
|---|---|---|
| DEC-218 | The official VPS runs one bootstrap relay/cache service only; it holds no PoA key, user Identity, or storage-provider role and never finalizes. | Frozen target; implementation/cutover pending |
| DEC-219 | The Central Authority is the ordinary desktop Identity whose role-specific `POA_FINALIZER` public key is committed by genesis; its desktop runs PoA finalization after unlock and local chain verification. | Frozen target; implementation/cutover pending |
| DEC-220 | Bootstrap network creation is allowed only from authenticated `EMPTY`, with a one-use activation code and proof of possession of the proposed genesis key; the binding is atomic and generation-numbered. | Frozen target; implementation/cutover pending |
| DEC-221 | Replacing a bound network requires authorization by the current Authority key, proof by the new key, and both signatures when the key changes; clients reject generation rollback. | Frozen target; implementation/cutover pending |
| DEC-222 | A bootstrap transport identity is separate from PoA/Identity keys and must be pinned through a trusted release or approved out-of-band source. A self-asserted network key from bootstrap is not a trust anchor. | Frozen target; implementation/cutover pending |
| DEC-223 | Headless finalizer/provider processes are LAB/test topology only; the official DEV VPS target is not a finalizer or provider. | Frozen target; implementation/cutover pending |
| DEC-224 | Central Authority identity is possession of the genesis PoA key, not an IP, hostname, endpoint, or persistent NodeID. Bootstrap authenticates its current route per fresh live CYP2 session and forgets it at disconnect. | Frozen target; implementation/cutover pending |
| DEC-225 | Bootstrap may relay to a live authenticated finalizer session and use bounded transient memory buffering, but keeps no durable shared pending-operation pool or Authority location record. | Frozen target; implementation/cutover pending |
| DEC-226 | Authority mobility does not permit concurrent independent signers sharing one PoA key; one active signer and durable anti-equivocation safety remain required. | Frozen target; implementation/cutover pending |

## Fixed economics

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
4 fee units -> 3 Security + 1 Onboarding
```

Runtime policy governance is not part of the current target. Network parameters
remain immutable.
