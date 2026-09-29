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
| DEC-212 | A storage provider is identified by `ProviderID = BLAKE3(provider public key)`, proven per CYP2 session; placement stores ProviderID plus last endpoint and the replica target counts distinct ProviderIDs. | Frozen |
| DEC-207 | Identity Authority supersedes the earlier Proof-of-Trust design and never grants PoA finalization power. | Frozen target |
| DEC-208 | Authority uses immutable rules; initial canonical sources are Age, capped finalized Activity and voluntary System Balance contribution. Liveness/Storage activate only with canonical evidence. | Frozen target |
| DEC-209 | A future service NodeID may bind to AccountID for contribution accounting but is not an Identity device credential. | Frozen target |
| DEC-210 | Anti-abuse policy uses generic Authority-derived Protocol, Storage and Bandwidth budgets rather than Mail/File-specific consensus quotas. | Frozen target |
| DEC-211 | Provisional validation remains future research: signed claims about operations against a finalized base may be distributed as non-canonical evidence, but never substitute for PoA finality or authorize remote storage. | Future |

## Fixed economics

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
4 fee units -> 3 Security + 1 Onboarding
```

Runtime policy governance is not part of the current target. Network parameters
remain immutable.
