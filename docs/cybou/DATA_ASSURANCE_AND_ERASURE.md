# Data assurance and erasure review

Status: implementation assessment and design proposal, 2026-10-04.
Original assessment baseline: `262198ba317e483a4a85f0265c4e90b20ddf013f`.
Current code and governance evidence reviewed at `f0e9293` (2026-10-04).
Code fixes and regression coverage were committed in `a3f05aa`.
This document does not introduce a wire format, consensus operation, service,
network, key role or change to frozen decisions. Proposed mechanisms below are
unimplemented and require an architecture decision before implementation.

Source alignment note (2026-10-05): the original baseline below is dated.
The active storage economy now implements provider-signed receipts, off-chain
audit evidence and payout-binding verification; these are not proposals to
start from zero. Canonical mutual reliability, independent failure-domain
evidence, remote purge receipts and crypto-erasure remain unestablished.
Current implementation truth is `26_IMPLEMENTATION_STATUS.md`; desktop evidence
presentation and its remaining APIs follow `DESKTOP_UX_DELIVERY_PLAN.md`.

## Evidence contract

A security status must identify the property, evidence, verification scope and
freshness. Unknown, stale and failed checks are distinct from success. A local
observation is not a canonical fact. Finality, remote admission, availability,
durability, recoverability and deletion must not be treated as synonyms.

The trusted client and its running environment are assumptions. A cryptographic
check cannot prove that the endpoint is uncompromised or that no key ever leaked.

## Current guarantees and gaps

| Property | Existing mechanism and evidence | What it establishes | Boundary or gap | Required validation |
|---|---|---|---|---|
| Content confidentiality | `encrypted_chunk.cpp`: ContentKey, per-chunk salt, HKDF, ChaCha20-Poly1305, network/header AAD; `root_publication.cpp`: KEM-wrapped key | Providers receive encrypted content; authorized clients can authenticate decryption | Does not establish absence of endpoint compromise, key export or traffic metadata leakage | Trace plaintext/key flow through staging, transport, logging and failure paths; reject altered ciphertext/AAD |
| Integrity | Exact encrypted bytes hashed to ChunkID; finalized authorization Merkle root; AEAD verification | Retrieved bytes match the authorized encrypted object and decrypt authentically | Does not establish availability or authorship merely from a hash | Corrupted chunk, wrong inclusion proof and wrong decryption context must fail before application use |
| Publication authenticity/finality | Identity operation authorization and locally executed PoA-finalized block | Authorized publication included in canonical state | PoA may censor or stop; a peer ACK is not finality | Verify inclusion and state transition locally; keep pending/unknown distinct |
| Remote admission | `StorageService` admission validates a receipt signed by the proven StorageId, bound to network/publication/chunk/size, and saves it with local evidence before counting a new placement | Signed admission evidence from the recorded provider | A receipt is not proof of continued possession, independent failure domains or canonical reliability | Verify signer/binding/persistence failures; GET plus ChunkID checks establish subsequent possession only at the observed time |
| Replica diversity | On-demand storage-key proof; placement deduplicates StorageId | Different proven storage keys | Different keys do not prove separate disks, hosts, operators or failure domains | One key on multiple endpoints counts once; investigate correlated-provider selection |
| Availability | `Audit` reads all recorded chunks; `AuditNextPlacement` rotates through a bounded subset; GET plus ChunkID verification | Checked chunks were retrievable and intact during the check | Bounded pass does not check the whole object at once; no continuous-availability proof or persisted per-replica freshness guarantee established by this review | Report checked scope and time; exercise timeout, corruption and partial-object failure |
| Repair/recoverability | `StorageService` removes failed placements and attempts replacement from valid local/remote bytes | Can restore the target when a valid source and admitting destination are reachable | Lost final replica is unrecoverable; cache is evictable and not a remote replica; finality alone says nothing about recoverability | Restore after a provider failure; fail honestly when no valid source exists; test clean-machine recovery separately |
| Reciprocal storage | Explicit local capacity and paid finalized storage leases | Local capacity policy plus canonical paid lease | Allocation and signed promises would not prove actual 1:3 service contribution | Define measured contribution, Identity binding and Sybil resistance before claiming enforced reciprocity |
| Revocation | Finalized author-only RevokePublication; admission rejects revoked publications | Publication no longer authorizes new admission; author's lease closes after the current period | Finalization depends on PoA; historical block bytes remain and failed purge retains physical byte accounting | Verify author checks, finality, lease closure and rejection of revoked admission |
| Provider purge | `FinalizedChunkStore::PurgePublication` removes associations and unshared admitted blobs | Compliant provider attempts managed deletion after revocation | Shared chunks remain; failed unlink retains byte accounting and a durable purge marker for retry; no remote purge receipt; hidden copies are unknowable | Covered by locked-blob, restart-boundary, missed-revocation and shared-reference regressions; filesystem power-loss durability remains unverified |
| Per-object crypto-erasure | No complete mechanism established | No current crypto-erasure guarantee | Historical self/recipient capsules and retained KEM material can recover ContentKey if corresponding bytes survive | Attempt unwrap from historical capsules after catalog deletion, revocation and rotation |
| Mail deletion | Local semantic deletion and author revocation of eligible publications | Removes managed references according to implemented policy | Cannot revoke plaintext or keys already held by recipients | Separate deletion of own copy, publication revocation and recipient retention |

Sources: `src/cybou/encrypted_chunk.cpp`, `root_publication.h/.cpp`,
`publication_service.cpp`, `application_service.cpp`, `storage_service.cpp`,
`finalized_chunk_store.cpp`; `ROOT_PUBLICATION.md`,
`APPLICATION_DATA_PLANE.md`, `19_SECURITY_THREAT_MODEL.md` and
`26_IMPLEMENTATION_STATUS.md`.

The current architecture targets independent replicas and notarized mutual
audits. This assessment identifies evidence limits and implementation gaps; it
does not silently replace DEC-205/212/269/270 or the Level-0 authority.

## Current key and recovery graph

```text
Identity mnemonic / retained KEM seed
  -> KEM private key for an applicable epoch
  -> historical RootPublication self capsule
  -> main ContentKey
  -> encrypted application root
  -> child ContentKeys / file and attachment trees

IdentityRecoveryBridge
  -> historical KEM seeds
  -> older publication capsules
```

`PublicationService::BuildAndSubmit` creates the owner capsule; recovery bridges also
carry a capsule for the future epoch. `ApplicationService` opens capsules and
imports historical seeds from recovery bridges. Files mutations and Mail can
reference child trees, so the erase boundary is an object graph, not necessarily
one RootPublication. Shared references must be resolved before revoking content.

Deletion from the current application catalog or current Application DB does
not erase historical wraps. IdentityRotate must not be presented as per-object
erasure: recovery deliberately preserves historical material. Moving capsules
off-chain would reduce immutable key-delivery metadata, but retained off-chain
copies would still be decryptable under a recoverable key.

## Design recommendation: preserve current behavior, bound the claim

For the current baseline, retain the existing self-capsule recovery path and
describe deletion as semantic deletion plus finalized revocation and managed
purge. Do not report CRYPTO-ERASED. This preserves clean-machine recovery without
inventing a new protocol or changing the existing genesis/history.

Separate evidence for:

- Local reference removed: the application no longer presents the item.
- Revocation pending/finalized: canonical admission changes only at finality.
- Provider purge requested/acknowledged: future direct requests and receipts
  are unimplemented; acknowledgment would be an authenticated statement, not
  proof that hidden copies do not exist.
- Recovery material removed: report only the controlled locations actually
  handled; saved historical capsules remain a recovery path today.

Already downloaded data should remain readable during loss of finality if the
client has the necessary bytes and keys. This is a required scenario to test,
not a claim that every access path has been verified here. Direct remote purge
without finality is a separate proposed protocol requiring authorization,
rotation, replay, shared-reference and re-admission rules.

## Candidate design for stronger per-object erasure

The necessary property is that every managed route to an object's key depends
on at least one secret that is independently random, deletable, and not
recoverable from mnemonic plus retained public history. A random ContentKey
wrapped directly to a mnemonic-recoverable KEM key does not meet this property.

One candidate is a per-object random erase secret controlling an authenticated
key envelope. Persistent ciphertext must not contain another path to ContentKey
that bypasses that secret. This is a key-lifecycle requirement, not a selected
cryptographic construction; no custom combiner or wire encoding is specified.

The erase secret would need a mutable, encrypted private recovery store with
durability, authenticated updates and rollback detection. A snapshot encrypted
only under mnemonic-derived material that contains the erase secret defeats
irreversible deletion when that snapshot is retained. Removing the latest copy
does not destroy old snapshots. A tombstone can instruct a compliant client to
refuse recovery, but is not cryptographic destruction.

This creates an explicit tradeoff:

| Recovery input | Consequence |
|---|---|
| Mnemonic plus permanent historical wraps | Current clean-machine recovery; no irreversible erasure while usable wraps and ciphertext survive |
| Mnemonic plus an independently maintained mutable recovery secret/store | Potential managed erasure; recovery also depends on that store and its surviving backups |
| Local-only erase secret | Simple managed key deletion; loss of all secret copies also loses otherwise intact content |

Recommended research direction: the mutable recovery-store model, with the
backup/rollback threat model specified before choosing a construction. Do not
add selectable product modes or change the runtime yet. If the necessary backup
guarantees cannot be met, retain the narrower managed-deletion contract.

Even a future design can only claim erasure within declared control boundaries:
exported plaintext/keys, recipient copies and adversarial snapshots are outside
that guarantee. Recipients retaining Mail access need their own independent
deletion decision. Provider purge remains useful storage hygiene regardless of
whether crypto-erasure is available.

### Required lifecycle and acceptance scenarios

1. Creation: inventory every main/child key, envelope, intent, index, cache and
   backup copy; no hidden alternate recovery path.
2. Recovery: rebuild active objects on a clean machine using the explicitly
   required inputs; detect stale store state before displaying success.
3. Rotation: preserve required active recovery without reintroducing deleted
   object secrets through bridges or old snapshots.
4. Deletion: journal the intent, remove controlled secret copies, revoke and
   purge managed content with retries; a crash at any boundary remains pending
   until the corresponding evidence exists.
5. Rollback: try an old vault/store snapshot, historical capsule, cached root
   and retained recovery bridge. Document exactly which combinations still
   decrypt; do not label policy refusal as crypto-erasure.
6. Sharing: deleting one reference must not destroy another live reference;
   owner deletion cannot claim destruction of a recipient's delivered copy.
7. Failure: inaccessible recovery store, finalizer outage, failed blob removal
   and lost last replica produce distinct actionable states.

## Ordered implementation work

1. Fix block fanout for genesis frontier and empty recent cache after restart;
   exercise both push catch-up and ordinary pull synchronization.
2. Remove unused offline-generated AccountIDs without regenerating private
   material, keys, signed genesis or compiled network identity.
3. Align implementation/public claims with evidence limits. Review frozen
   replica/audit language explicitly rather than changing lower-level documents
   into a competing architecture.
4. Verify purge crash/failure behavior and repair/recovery scenarios above.
5. Validate the implemented provider-signed receipts and off-chain evidence:
   network/publication/chunk/size binding, signer proof, persistence, replay
   bounds and revocation semantics. Signatures do not prove continued storage;
   subsequent checks need declared scope and freshness. Any further canonical
   register requires an accounting/evidence decision; do not introduce per-audit
   records contrary to the frozen storage economy through a UI feature.
6. Resolve mutable recovery-store durability and erasure semantics before
   changing RootPublication or discovery. Review consensus/history compatibility
   explicitly; any provisioning or cutover remains separately authorized.

Canonical periodic storage proofs, global penalties, new finality recovery
authority and hash-profile changes remain separate designs. None is introduced
by this assessment.

## Provider-signed receipt proposal (not active protocol)

The frozen storage-economy target adopts an off-chain `StorageReceipt` as part
of storage evidence (DEC-276); the constraints below still govern its design.

A receipt would be portable evidence that the holder of a storage key accepted
a specific encrypted chunk for a finalized publication. It would not establish
physical independence, future availability, an honest disk write, or consensus
authority. No receipt wire format, signature domain or accounting rule is
introduced by this proposal.

The first implementation candidate is one receipt per admitted chunk, with
bounded local retention. Batching should follow measured overhead rather than
introducing ambiguous partial coverage. The signed statement must bind:

- NetworkBinding and finalized RootPublication OperationID;
- ChunkID and exact encrypted byte length;
- provider StorageId and the public verification key that derives it;
- requester-generated random challenge, accepted only for its outstanding PUT;
- finalized base BlockID and an explicit retention end height.

The retention interval is a proposed provider obligation, not a new permission
to store: finalized revocation ends publication authorization regardless of the
receipt interval. A height interval does not promise wall-clock retention when
finalization stops. Before choosing an interval, define behavior during a PoA
outage, expiry renewal and the resource bound on retained obligations.

Provider procedure: independently verify finality and Merkle authorization,
check capacity, durably persist the exact bytes and publication association,
then sign and return the statement. The durable write ordering and fsync
requirements must be specified and tested for each supported filesystem.
An honest implementation follows this ordering; a signature alone cannot
prove that a dishonest provider followed it. A retry with a new challenge must
recheck live authorization and physical bytes even if metadata says admitted.
Never turn an old receipt into successful new admission after revocation.

Requester procedure: verify exact field consumption, size bounds, network,
publication, chunk, length, challenge and signature against the authenticated
storage relationship. Match the key to StorageId; endpoints and TLS identities
are not signer authority. Reject malformed, cross-network, wrong-request and
expired responses. Count one storage key once across endpoint aliases. Retain
the evidence in the encrypted rebuildable Application DB, with explicit
unknown status after its loss until fresh evidence is collected. Existing
GET/hash audit and repair remain necessary, and a failed check invalidates the
local availability estimate even while an earlier receipt remains authentic.

Receipts initially have no canonical accounting effect: they cannot increase
quota, allocate rewards, impose penalties, admit chunks, or change
finality. In particular, a receipt plus a requester-reported timeout is not
objective evidence of global misconduct. No routine receipt is put on chain
by this proposal. The frozen mutual-proof/notarial target remains a separate
design requiring deterministic evidence and accounting rules before code.

Acceptance gates: valid receipt round trip; tamper every bound field; replay
across requests/networks/publications; endpoint alias deduplication; bounded
parsing and retention; failed write never signs; restart at each durable
boundary; retry after revoke; expired obligation; honest provider losing bytes
after signing; corrupt GET; requester DB loss and recovery. Review the existing
storage key signing primitive and cryptographic domain separately before
freezing exact bytes. This proposal does not authorize a genesis change.

## Implementation follow-through

Committed in `a3f05aa` on 2026-10-04: block fanout uses canonical history
and the session's HELLO/BLOCK_RESULT frontier, with a bounded per-peer budget.
Genesis height zero is valid. Recent-block and announced-block caches are
removed, so reopening persisted history requires no new block to begin fanout.
Regression coverage checks push and pull from heights zero and one to height
40, both with live history and after reopening its persistent store. Wire,
genesis, keys and canonical execution rules are unchanged.
Local verification: the complete core suite passed and the headless `cybou`
executable built successfully. Desktop CI and deployment were not performed.

Offline AccountID cleanup is committed in `a3f05aa`: provisioning no longer generates
AccountIDs or emits them in new secret files, summaries or public constants.
AccountID is created by Identity onboarding. Existing private files were not
rewritten; obsolete AccountID lines there are not used by verification or
onboarding. All remaining compiled public arrays are byte-identical to the
reviewed baseline. The rebuilt offline tool verified existing DEVNET material
without signing or creating a network, and a before/after byte comparison
confirmed that the private files were unchanged.

Initial public-claim alignment is committed in `a3f05aa` in README, `www/llms.txt`, the French and
English compliance strings and their static HTML fallback. The texts distinguish
distinct storage keys from physical independence, GET/hash checks from the
unimplemented mutual-audit protocol, and finalized revocation/managed purge from
crypto-erasure. They no longer claim legal immunity, guaranteed uninterrupted
repair or that destroying a local key satisfies erasure despite historical
capsules. Implementation status and the threat model record these limitations
without changing the frozen architecture target. JavaScript syntax and matching
translation/fallback content were checked; no website publication was performed.

Managed purge hardening is committed in `a3f05aa`: publication associations are removed and
unshared chunks receive a durable pending-purge record before unlink. Provider
byte accounting is retained on physical removal failure and released only when
removal succeeds or the blob is confirmed absent. A restart resumes the pending
record, including a crash after unlink but before the metadata update. Startup
also reconciles associations against finalized publications to recover a missed
revocation event. Bounded rotating retry passes run in ordinary network
maintenance and cache collection; a newly authorized admission atomically
cancels a pending deletion of the same chunk. Shared chunks remain until their
last authorizing association is removed. The unused direct chunk-pruning API,
which bypassed publication associations, is removed. This does not add direct
purge requests, deletion receipts or crypto-erasure.
The complete Windows core suite passed after this change and headless `cybou`
built successfully. Coverage includes shared publication references, durable
restart recovery, a missed revocation event, a Windows handle denying deletion,
and cancellation of a pending purge when another valid publication admits the
same chunk. The crash test models persisted boundaries; it is not a power-loss
or filesystem durability certification. Linux and desktop builds were not run
for this change.

Additional storage regression coverage removes every local publication chunk,
disconnects one of four providers and requires repair to two reachable exact
copies for every chunk. A separate case disconnects every provider, checks that
audit drops Protected and fetch returns unavailable, removes the Application
DB, then rebuilds placement from remote authorization proofs after providers
return. Every recovered encrypted chunk is compared byte-for-byte with its
original; the finalized height remains unchanged. This tests a fresh placement
DB with retained Identity and node state, not complete clean-machine Identity
or application-semantic recovery. The receipt proposal above remains design
work only.

The rotation recovery regression now uses a separate Full Node runtime with
zero local chunk bytes, a new vault restored from the current mnemonic, and a
new Application DB. Only the verified finalized history is copied into that
runtime. With every provider offline, scanning remains incomplete and exposes
neither the old file nor a recovered bridge. After providers return, another
scan imports the historical KEM seed through the bridge, restores the file
catalog and downloads the original plaintext byte-for-byte without advancing
finalized height. The original vault and Application DB are not supplied to the
restored services. This is component-level recovery with an in-process provider
transport and supplied history, not a desktop-installation or live peer-sync
acceptance test. It also demonstrates why retained bridges and capsules prevent
a claim that loss of the previous local vault erases old content.

That regression also covers deletion after rotation: two separate publications
use the old KEM epoch; the author rebuilds the rotation-invalidated Application
DB, publishes a deletion for one file, and signs revocation of the file's
original publication without its old local job. Synced compliant providers
return no bytes for any chunk of that revoked publication. Another empty Full
Node restores the current mnemonic and rebuilds from finalized history plus
remote storage: the deleted file is absent, the bridge remains discoverable,
and the other old-epoch file decrypts to its exact original plaintext. Recovery
does not advance finality. The test exercises author-signed protocol revocation,
not the desktop's automatic selection of unreferenced publications. Purge is
checked on compliant fixture providers; retained adversarial copies or the
earlier machine's cache are outside this assertion.

Documentation and UI alignment reviewed against the committed `f0e9293` code:
StorageId establishes a cryptographic storage identity, while independent
replicas remain the Beta target requiring separate diversity evidence. The
canonical publication register does not store placements or mutual-audit
reliability today. Capacity reciprocity remains an objective, not measured
contribution. The current guarantee table now reflects committed purge retries,
and implementation status distinguishes local verification from CI/deployment.
Website FR/EN strings and static fallbacks no longer claim impenetrability,
Cloud Act immunity, certified OpenSSL/product status or absence of a finalizer
failure point. Mail/Files deletion dialogs describe managed purge and retained
copies. Mail security details no longer synthesize separate Verified results
from a content status; network confirmation requires the operation's finalized
state and a reported finalized height. These changes do not add an evidence
snapshot API, remote deletion proof or a new consensus operation.

Validation of this alignment: Windows desktop `cybou` built successfully in
`build_simplified_gui`; JavaScript syntax, translation XML and all 14 changed
FR/EN string pairs with their static French HTML fallbacks were checked.
Dialogs were not visually exercised and no website or binary deployment was
performed. Runtime consensus/storage behavior is unchanged; Mail's confirmation
display now uses operation evidence rather than content durability status.
