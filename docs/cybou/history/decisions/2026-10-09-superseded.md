# Superseded CYBOU decisions

Status: HISTORICAL
Scope: Exact prior decision wording, extracted from 1f08f8c5.

Do not implement these statements. Consult [current decisions](../../24_DECISIONS.md).
Replacement chains inside the historical table reflect their original dates;
DEC-284 subsequently removed AUTH and Validation entirely.

## Removed and partially superseded rows

| ID | Prior decision | Prior recorded status |
|---|---|---|
| DEC-151 | Account creation funds System Balance from OnboardingPool and does not mint supply. | Superseded by DEC-277 (Treasury-funded onboarding); removed from code in M5 |
| DEC-246 | GenesisAllocation may assign initial AUTH to designated ordinary Identities. AccountCreate claims it exactly once. Such an allocation is a genesis decision, not a property of the bootstrap role; there are no consensus bootstrap grants or bootstrap Identity roles. | AUTH part superseded by DEC-284 (allocations carry CYBOU only); otherwise frozen |
| DEC-248 | AUTH is a canonical non-transferable account value stored in AccountState and committed by the state root, independent of the 100B CYBOU supply. It changes only through deterministic finalized transitions: GenesisAllocation, +1 AUTH to the authorizing account of finalized utility operations (RootPublication, SystemLock), capped at +1 per account per block, and one PoA-signed `PoaAuthAdjustment` operation with GRANT (+N) or BURN (-N, floor 0) that itself earns no AUTH. There is no AUTH transfer. Finalized AUTH > 10,000,000 makes an Identity eligible to sign Validation; AUTH never grants PoA finalization power. Automatic penalties are not frozen. | Superseded by DEC-284 (no AUTH) |
| DEC-249 | Every full node independently validates and executes every candidate operation against its latest finalized state and relays only locally valid candidates. Validation is an additional signature by an eligible Identity over NetworkBinding, OperationID, finalized base BlockID and its AccountID, made only after its own node validated the operation. It never substitutes local or PoA execution, never changes state, and creates no provisional state. There is no `validation.enabled`, `min_signatures` policy or block-candidate Validation. | Local execution and relay rule frozen; Validation superseded by DEC-284 (removed) |
| DEC-265 | Production Full Node always has a positive storage allocation. Configured capacity of zero is forbidden in production and restricted to memory-only unit tests. Default allocation is automatic based on available storage. Nodes with depleted local disk space reject new inbound admissions without changing node type, consensus authority, or mesh relay participation. | Automatic allocation superseded by DEC-275 (explicit `V >= 15 GiB`); positive-capacity and low-disk rules remain |
| DEC-268 | Resource rate limits and storage allowances are governed by the canonical AUTH ladder. Low-AUTH identities have bounded per-epoch operation rates and storage allowances to mitigate spam and Sybil attacks. Allowances scale progressively with finalized AUTH up to the Validator tier (AUTH > 10,000,000). The concrete, finite ladder is DEC-272. | Superseded: storage by DEC-274, operation rates by DEC-284 |
| DEC-269 | Each newly registered Identity has a finalized 5 GiB remote publication quota at the onboarding tier. The 1:3 reciprocal baseline is a capacity/service objective, not evidence of actual contribution. Local automatic allocation depends on free disk space; it does not guarantee 10–15 GB or enforce measured service reciprocity. | Superseded by DEC-274/DEC-277 (paid storage, Treasury onboarding); removed from code in M5 |
| DEC-270 | Target: randomized mutual storage audits and possible PoA-notarized reliability evidence. Current runtime uses operational GET/hash checks; mutual-audit transport, notarization and canonical reliability coefficients are unimplemented. Their evidence, privacy and accounting design remains open before implementation. | Superseded by DEC-276/DEC-282 (off-chain evidence, PoA settlement; no canonical reliability coefficients) |
| DEC-272 | Consensus-enforced AUTH tier limits. Block execution meters every Identity-authorized operation (Payment, SystemLock, IdentityRotate, NameCommit, NameReveal, RootPublication, RevokePublication) against the parent finalized AUTH: operations per block and per epoch, remote storage quota and largest publication. Counters (`usage`) and the publication register (`publications`, keyed by RootPublication OperationID) are committed by the state root; the section is omitted while empty, so the genesis state root is unchanged. Tiers: T0 <10k: 1/block, 30/epoch, 5 GiB, file 1 GiB; T1 >=10k: 5, 150, 25 GiB, 4 GiB; T2 >=100k: 25, 750, 100 GiB, 16 GiB; T3 >=1M: 100, 3,000, 500 GiB, 64 GiB; Validator >10M: 1,000, 30,000, 2 TiB, 256 GiB. Quota unit is 512 KiB per authorized chunk. `RevokePublication` (author-only, payment fee, no AUTH) frees quota and stops chunk admission; providers purge chunks no other publication authorizes. One `.cybou` name per Identity. | Storage quota and file-size parts superseded by DEC-274, AUTH operation limits by DEC-284; revocation and one-name rule remain frozen |
| DEC-283 | Storage economy delivery: M1 architecture freeze; M2 explicit capacity; M3 evidence; M4 shadow accounting on current DEVNET with no CYBOU moved; M5 consensus economics; M6 adversarial, storage-failure, monetary-conservation, provider-concentration and Sybil simulations as release gates; M7 new DEVNET (new Network Root, NetworkID and signed genesis with `onboarding_bonus = 20,000`, rate 5 CYBOU/GiB/day/replica, unit 512 KiB, replicas 2, period 86,400 s) only under explicit operator authorization; M8 product Beta. Consensus changes wait for M7; the current DEVNET genesis is never modified. `Protected` then requires an active publication, active funded lease, two remote obligations at distinct storage/economic identities and fresh evidence. | Frozen target |
| DEC-273 | Relay proof-of-work for every user operation. Work = SHA-256(`CYBOU/OP-WORK` || NetworkBinding || OperationID || nonce u64 LE) with leading zero bits >= the author tier difficulty (22/21/20/19/18 bits for T0..Validator; NameCommit/NameReveal +4). Every Full Node, PoA included, checks it before candidate execution and relay; `OP_META` carries the nonce beside the exact bytes. The work is pre-finalization evidence only: it is not part of the block, the block hash or the state, and finalized history is never re-checked for it. AccountCreate (consensus PoW) and PoaAuthAdjustment (genesis PoA signature) are exempt. | Tiered difficulty superseded by DEC-284 (flat 22 bits, names +4); work rule frozen |
| DEC-288 | Storage providers come from every known Full Node, not only the eight outbound mesh sessions: each sync pass proves the StorageId of one discovered, unconnected endpoint in a short session on a separate thread (each endpoint at most every 10 min, usable for 30 min). Replicas of one chunk sit at distinct network addresses and never on a loopback address (this machine): distinct StorageIds behind one address are one failure domain. Too few diverse providers keeps content Securing. Memory-only component fixtures, whose providers share loopback, are exempt. | Frozen; implemented |
| DEC-257 | All protocol fees transfer atomically from payer System Balance to the unique genesis-granted `cybou` Central Authority allocation before claim, or its claimant's spendable Balance after claim. DEV OnboardingPool starts at 100M CYBOU and only funds onboarding. No batching, fee burn, validator/provider rewards or fee-funded onboarding. State is the only canonical encoding; a new official genesis requires a new NetworkID. | Superseded by DEC-277/DEC-278 (Treasury monetary base, storage rent to providers); removed from code in M5 |

## Prior monetary deployment claim

Superseded model (still run by the deployed DEVNET binary until M7):

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
DEV OnboardingPool = 100,000,000 CYBOU (genesis only; never replenished by fees)
100% protocol fee: payer System Balance -> Central Authority spendable Balance
Before claim: fees accumulate in the unique genesis allocation labelled cybou.
After claim: fees credit that allocation claimant's ordinary AccountState Balance.
```



## Earlier history


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
