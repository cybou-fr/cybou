# Data, key and retention inventory

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Reviewed: 2026-10-04, code baseline `d335011`. Governing reference:
[`SECURITY_GOVERNANCE.md`](SECURITY_GOVERNANCE.md).
This is an engineering inventory of reviewed paths, not a completed GDPR
processing register, DPIA or determination of controller/processor roles.
Identifiers and ciphertext need contextual personal-data assessment; being
public, hashed or encrypted does not settle that assessment.

## Reviewed storage and observation paths

| Data / purpose | Location and visibility | Current lifecycle evidence | Remaining requirement |
|---|---|---|---|
| Account identity, public key roles, nonce/epoch, balances and names; authorize execution | Finalized state and distributed block history; visible to nodes | Current keys/state advance deterministically; rotation does not remove old blocks. Sources: `identity_registry.h`, `state.h`, `state_store.cpp` | Classify linkability, purposes and lawful retention of historical identifiers; active-state removal must not be described as historical erasure |
| Publication roots, chunk count, opaque recipient capsules; authorize storage and recover keys | RootPublication operation in finalized history | `root_publication.h` stores root, authorization root, count and capsules. Author AccountID is in operation authorization. Revocation removes active authorization, not historical bytes | Assess observable publication timing, size/count and key epochs. Capsules contain KEM profile, epoch, encapsulation and wrapped key, not an explicit recipient AccountID list; do not overstate what peers can identify |
| Encrypted content; network storage | Common ChunkStore on local and remote Full Nodes | Exact stored bytes keyed by ChunkID; provider admission requires active finality and Merkle proof. Sources: `chunk_blob_store.cpp`, `finalized_chunk_store.cpp` | Define retention purpose and obligations for each operator relationship; peer copies outside controlled purge remain a boundary |
| Provider associations, proofs, byte accounting and pending purge | Provider metadata alongside local blob store; not the user's semantic catalog | Association removal plus durable pending-purge marker; unshared unlink retries; shared content retained. Source: `finalized_chunk_store.cpp` | Validate supported-filesystem failure/power-loss scope; no remote deletion receipt or adversarial-copy erasure is established |
| Local encrypted cache and staging pins; upload/retrieval | Common ChunkStore and retention registry | `chunk_retention.cpp` keeps reference pins and cache last-use timestamps. `node_runtime.cpp` GC uses a 10-minute grace period plus budget/removal limits; admitted/pinned chunks are excluded | Grace period is an eviction safeguard, not a maximum retention period. Confirm maintenance cadence and disk budget per installation |
| Mail/Files catalog, drafts, filenames, folders, semantic identifiers and local preferences | Separate encrypted Application DB per unlocked Identity | `application_service.cpp` has Mail/Files/draft/recovery indexes and scan checkpoints; `private_application_store.cpp` encrypts rows. DB is rebuildable, but local drafts/preferences need not be recoverable from network publications | Map exact fields and exports; define local-draft retention and user-controlled backup. Removing a DB row is not proof of physical media sanitization |
| Local draft-to-outgoing-message handoff | Encrypted Identity Application DB (`mail/draft-send/<draft-id>`, `mail/draft-send-content/<draft-id>`) | Binds a saved draft to one private message/publication job ID for restart and stale-compose duplicate prevention; a private SHA-256 content fingerprint excludes save timestamps and refuses altered payload as a resume of an existing job; no message text, key or path is stored in these bindings | Successful handoff retains the binding after draft removal; explicit discard removes it. Define cleanup with durable job retention before changing this behavior; never publish/export this local relationship |
| ContentKeys, plaintext and downloaded/exported content | Unlocked application memory and user-selected output paths | Decryption/retrieval authenticates chunks; recovery capsules/bridges may restore access. Sources: `encrypted_chunk_tree.cpp`, `keystore.cpp`, application services | Complete GUI/download/temp-file/crash-dump trace. This pass does not establish that plaintext never reaches disk, clipboard, swap or external software |
| Recovery entropy, AccountID and derived signing/KEM secrets | Password-protected portable vault; unlocked keystore memory | `identity_vault.cpp`, `identity_service.cpp`, `keystore.cpp` handle save/restore and separate roles. Historical KEM seeds imported through bridges preserve old access | Inventory mnemonic exports, vault backups and OS/user copies; protect confidentiality while preserving required recovery. No private material was read for this inventory |
| RecoveryBridge and historical-key recovery | Encrypted published content plus in-memory imported historical KEM seeds | Clean-node tests restore old content from current mnemonic and surviving bridge; deleting one publication leaves unrelated old-epoch content recoverable | Specify historical-key bounds and backup/rollback policy before promising per-object crypto-erasure |
| Storage placement endpoints and StorageIds | Encrypted Identity Application DB | `storage_service.cpp` saves placement records; lost DB can rebuild through provider-held proofs. Placements are not canonical reliability records | Set retention/freshness and access/export rules; distinct keys do not establish physical independence |
| Provider storage receipts and rolling evidence; storage-economy shadow accounting | Encrypted Identity Application DB (`storage/receipt/...`, `storage/evidence/...`) | `storage_service.cpp` keeps one signed receipt per counted replica (erased when the replica is dropped) and per-StorageId counters, check times, verified unit-seconds and shadow reward, bounded to 1024 providers with oldest-activity eviction. Off-chain only; nothing is paid | Define retention once M5 settlement consumes evidence |
| Peer endpoints, session state and Geo admission inputs | Live P2P manager, runtime configuration and local Geo data | `p2p/peer_manager.h/.cpp` handles connections/discovery; sessions/frontiers clear on DisconnectAll. Configured locators survive through configuration | This pass does not establish a separate durable learned-peer database or a global endpoint retention rule. Review deployment config, diagnostics and external service logs |
| Node event diagnostics; troubleshoot and monitor | Local append-only event file when configured | `event_record.cpp` allowlists fields. MINIMAL suppresses account_id, nonce, peer, storage_id, chunk_id and operation_id. DETAILED can retain these fields; every record has time/run/sequence. Writer appends and flushes, with no rotation/expiry in that class | Trace external rotation/backup and service journaling separately. Assign an operational retention period and access controls; allowlisted/public fields are not automatically non-personal |
| Candidate operations and operation journals; retry safely | Bounded volatile operation pool; local Identity coordinator journal | `operation_pool.h` caps default pool at 256 operations / 8 MiB and per-peer at 32 / 1 MiB. `identity_operation_coordinator.cpp` persists operation intent for safe retry | Pool limits are memory bounds, not a retention schedule. Inventory durable journal fields, resolved-record cleanup and user support exports |
| PoA signing journal and equivocation safety evidence | Local signer history and safety records | `poa_signing_journal.h/.cpp` retains durable signing intent and fails closed on conflicts | Never apply ordinary log expiry to signer safety history. Define protected recovery/backup and access policy that preserves one active signer and signing history |

Source paths in this table are relative to `src/cybou/` unless explicitly named.

## Passive local monitoring inventory (2026-10-09)

DEC-289 and NETWORK_OBSERVABILITY_PLAN.md freeze Beta monitoring to local runtime
metrics and evidence from the existing chain/P2P/storage protocols. No remote
resource report, address-group cache, request challenge store or cohort history
is retained by production. Superseded remote processing designs are in Git.

Local frame/payload byte windows, operation windows and process/disk diagnostics
remain volatile, bounded and without endpoint/content/Identity labels in metric
history. The existing peer/session, application storage-evidence, event-log and
signing-journal inventories above retain their own scopes and obligations. This
source change does not erase or change their records or retention.

OS logs, VPS journald, external backups, browser/web analytics, support systems
and recipient exports require separate inventories; none is assumed absent
because it is outside these files.

## Deletion and retention boundaries

DEC-290 direct storage usage adds a narrow numeric response inventory: each
admitted TLS storage relationship carries reported provider capacity and admitted bytes; existing storage
identity proof is reused for deduplication, without a new metric signature. No filename, ChunkID, AccountID or CPU/RAM is
exported. The existing bounded routing/probe map retains endpoint, StorageId,
optional payout binding and last direct numeric sample in RAM, without a new DB.
Only samples up to 90 seconds old enter totals; existing placement identity proofs
retain their separate 30-minute lifetime. TLS delivers indicative declarations,
not audited service or a physical node census. GUI receives aggregate numeric
values, coverage and age, not a new per-content provider index. This inventory
does not claim legal certification or complete deployment/third-party retention.

- Catalog deletion changes semantic application state. It does not itself
  delete historical chain records or all ciphertext copies.
- Finalized author revocation removes active publication authorization and
  releases canonical quota. Provider physical byte accounting is released only
  after successful unlink or confirmed absence, with shared references retained.
- Cache GC is conditional on budget, grace, pins, admission and maintenance;
  the ten-minute grace does not imply deletion after ten minutes.
- A retained mnemonic, capsule, bridge, vault backup or exported key may restore
  decryption where ciphertext survives. Compliant-provider purge does not prove
  cryptographic erasure.
- Logging modes reduce or increase diagnostic detail; neither defines the
  applicable processing purpose, lawful basis, access policy or expiry date.

## Outstanding operational decisions

The logging review and proposed rotation/export contract are recorded in
[`LOGGING_RETENTION_AND_EXPORT.md`](LOGGING_RETENTION_AND_EXPORT.md).
Minimal JSON mode does not filter stdout/stderr; live service retention and
backup configuration were not inspected by that review.

Retention decisions should be justified by processing purpose and applicable
requirements using the [CNIL retention guidance](https://www.cnil.fr/fr/passer-laction/les-durees-de-conservation-des-donnees)
and [logging guidance](https://www.cnil.fr/fr/securite-tracer-les-operations).
Those references do not establish one universal expiry for cache, event logs,
vault backups, chain history and signing-safety records.

| Decision | Current status | Required evidence before claiming readiness |
|---|---|---|
| Operator/controller/processor roles for desktop, official service and storage relationships | Open; no roles assigned by this inventory | Named actors, purposes, deployment scope and relationship assessment |
| Processing register and DPIA applicability | Open | Field-level data flows, affected persons, purposes, lawful bases, rights and risk assessment |
| Diagnostic log lifetime, access and support-export policy | Open | Defaults for deployed service/desktop, external rotation, backup expiry and redacted export workflow |
| Chain metadata minimization and rights handling | Open | Public-field necessity/linkability assessment and a documented response to requests involving immutable history |
| Vault/bridge/backup retention and recovery | Partial component evidence | Controlled key-copy inventory, rollback risks and restore exercises with stated RPO/RTO |
| Signing safety history | Safety retention required; operational procedure open | Backup/restore procedure preserving durable anti-equivocation history; ordinary log cleanup must exclude it |

Do not introduce guessed retention durations, silently delete journals, or
assign legal responsibilities from a node type. The next review should trace
deployment logging and GUI export paths, then record accountable decisions and
acceptance checks under the governing baseline.

Folder import discovery temporarily holds a bounded list of local source paths,
filenames and parent relationships in memory (10,000 entries, depth 64). It
creates no file/publication state until discovery succeeds. Cancellation or page
destruction requests worker stop; an outstanding filesystem call must return
before its private discovery memory is released. No source-path discovery log
or import manifest is persisted. Already handed-off uploads keep the existing
publication job retention and recovery rules.
