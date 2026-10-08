# Direct network observation reports

Status: Level 2 normative implementation target under DEC-289, 2026-10-08.
Strict standalone request/reply payload codecs are implemented and tested.
The narrow runtime-owned cache refreshes independently every five seconds.
The runtime exchange guard and direct TLS request/reply transaction are implemented.
The runtime-owned bounded address-group store, session close/expiry wiring and
immutable address-free consolidated snapshot are implemented.
Network/Console partial-coverage summary cards are implemented.
Runtime bounded remote history and Network cohort charts are implemented.
An opt-in service idle-slot acquisition scheduler is implemented.
Ordinary startup leaves acquisition disabled pending coordinated acceptance;
no observation exchange is deployed. The source P2P baseline ends at message 28;
older deployed software remains on its stated baseline and must upgrade before polling.
This document freezes the first direct-report contract, not a global census,
relayed telemetry design or a compliance claim. It follows
[AGENTS.md](../../AGENTS.md), [security governance](SECURITY_GOVERNANCE.md)
and the [processing inventory](DATA_PROCESSING_INVENTORY.md).

## Purpose and authority boundary

Every Full Node implements the same bounded request/reply service. Collectors
use existing admitted same-network TLS mesh sessions; monitoring opens no dedicated
discovery/storage-proof connections. Bootstrap is an ordinary reporting peer.
No role, capability advertisement, NodeID, persistent reporter pseudonym,
Identity signature, StorageId proof or PoA route is introduced.

Reports are non-canonical, untrusted process declarations. TLS associates bytes
with the current connection; it does not prove truthful resources, a stable
reporter identity, an independent machine or a legal processing basis. Preserve
compiled bootstrap transport pins and the existing France/public and LAN admission
rules. Report acceptance grants no placement weight, rent, payout or finality.

## Collection and minimal fields

Each runtime maintains one narrow report cache, refreshed every five seconds
independently of GUI visibility and Identity unlock. Only local aggregate gauges
and counters are read, with no chain scan, provider-object enumeration or audit.
Requests copy this cache; they never initiate CPU averaging, proofs, repair,
history catch-up or benchmark work. Cache failure/age over five seconds produces
unknown metric blocks, not a synchronous collection request.
Collect outside the cache mutex, then publish one coherent cache atomically.
The reply handler copies cached bytes without acquiring chain/provider/peer
locks or retaining a cache lock across socket I/O. A delayed collector reduces
data coverage; it must not stall mesh service to produce a fresh report.

Implemented cache: fixed 191-byte storage, monotonic age from collection start,
unknown startup/failure/backward-time/age over five seconds, and CPU mean age
advanced at read time (CPU alone becomes unknown above its age bound). Collection
uses a dedicated runtime worker, no Identity/GUI callback or network request.
It reads existing counters and the initialized matching-network head metadata, tries the
chain lock without waiting, and omits chart history and disk queries. Missed
refreshes are skipped rather than replayed; shutdown interrupts the timed wait
and joins before runtime storage teardown. These are local implementation facts,
not transport/replay/consolidation acceptance or measured collector-cost evidence.

The report contains:

- initialized finalized cursor: height and tip, an unverified peer claim;
- policy V and provider budget in bytes; stored encrypted lengths and provider
  admitted replica byte lengths rounded down to MiB, excluding filesystem overhead;
- received/sent CYBOU frame bytes over exactly 60 complete seconds, each rounded
  down to KiB; includes service/retry bytes and excludes TLS/TCP overhead;
- most recent completed process CPU mean: online logical processors, actual
  window length, interval count, age and percent rounded to 0.01 percentage points;
- resident process memory rounded down to MiB, including GUI/shared pages.

Rounded zero means less than one reporting unit, not necessarily exact zero.
Local Console retains exact measurements. Remote units/rounding must be visible.
CPU means are known only for windows of 60–120 seconds and age at most 60 seconds;
other windows remain unknown in the report even when locally available. CPU
includes the entire process and is not host, affinity or quota utilization.
Physical stored lengths must not be labelled unique content or verified service.
`provider_used_MiB` comes from `FinalizedChunkStore::UsedBytes()`: accounted
admitted provider replica lengths, not active contractual lease or placement
obligations. It does not establish that those bytes were recently served.

Exclude wall-clock sample time, runtime uptime/start identifier, lifetime byte
totals, processor model, executable/build identifier, disk path/free space,
peer lists/endpoints, storage keys, AccountIDs, names, recipients, object IDs,
candidate contents, local-production counts and signer/safety-journal status.
Disk availability and memory must not be summed as network capacity. Omitting
signer fields avoids explicitly advertising the PoA location; other activity may
still permit inference, so anonymity is not claimed.

## Request/reply layout

Message codes: `GET_OBSERVATION = 27`, `OBSERVATION = 28`. No version,
schema discriminator, capability bitmap, TLV, strings, arrays or fragmentation.
Frame encoders/decoders accept only the exact 64/191-byte observation sizes;
the TLS reader rejects an invalid declared size before allocating the body.
All integers are unsigned little-endian; payload consumption is exact.

Request is exactly 64 bytes: `NetworkBinding[32] || challenge[32]`.
Challenge is freshly generated by the local CSPRNG, never reused on a session.
Only one observation request may await a reply on a given session.

Reply is exactly 191 bytes:

| Block, in order | Fixed fields | Bytes |
|---|---|---:|
| Binding | NetworkBinding[32], echoed challenge[32], cache_age_ms:u32 | 68 |
| Cursor | known:u8, height:u64, tip[32] | 41 |
| Storage | known:u8, V_bytes:u64, stored_MiB:u64, provider_budget_bytes:u64, provider_used_MiB:u64 | 33 |
| Traffic | known:u8, window_ms:u32, received_KiB:u64, sent_KiB:u64 | 21 |
| CPU | known:u8, processors:u32, window_ms:u32, intervals:u32, age_ms:u32, mean_basis_points:u16 | 19 |
| Memory | known:u8, resident_MiB:u64 | 9 |

`known` is 0 or 1, metric availability rather than a capability announcement.
Unknown blocks have all remaining bytes zero. A stale cache uses age 5001 and
all unknown blocks; otherwise age is 0–5000. Other ages are invalid. The cursor
is known only after initialization; a height-zero initialized cursor may be valid.
Known traffic has window 60000. Known CPU has processors 1–65536, window
60000–120000, intervals 1–120000, age 0–60000 and basis points 0–10000.
Out-of-range local CPU becomes unknown, never clamped to a synthetic percentage.
Known storage has V at least 15 GiB and budget exactly floor(2V/3), computed
without overflow. Use above V/budget is legal reporting (e.g. policy reduction),
so do not reject it or wrap headroom. Known MiB/KiB fields must fit after byte
conversion (at most UINT64_MAX shifted right by 20/10 respectively).
Unit conversions/sums use checked arithmetic;
unrepresentable aggregates are unknown, not wrapped or saturated totals.

## Scheduling, replay and abuse bounds

- Start only after successful same-network HELLO. Require the payload binding
  to match that connection's network. A matching fresh outstanding challenge
  and the exact same live session are required before accepting a reply.
  Consume and erase the pending challenge on acceptance or timeout; keep no
  challenge history or diagnostic payload log.
- Request at most once per numeric transport IP per 30 seconds across all ports
  and sessions. Responder additionally serves at most 32 requests per 30 seconds
  per runtime; collector sends at most 16 per 30 seconds, with four outstanding
  observation requests globally. No retries before the per-IP interval expires.
- The optional session-owner transaction has a two-second total deadline.
  The exchange guard retains a five-second upper bound for pending challenges;
  owner cleanup removes them earlier on completion/timeout. A duplicate, late,
  unsolicited, foreign-session or mismatched-challenge reply never replaces a sample. Malformed frames follow
  existing local protocol-abuse handling; unavailable/rate-limited reports do not
  penalize consensus, payments, storage placement or Identity.
- Bound the shared per-IP rate limiter at 128 entries, retaining cooldown state
  across session churn for 90 seconds. Expire old entries; when full reject a
  new observation address rather than evict an unexpired cooldown.
- Schedule through the existing session transaction owner, between transactions.
  Never add a concurrent frame reader/writer or interrupt a block/operation/
  storage transaction. Busy sessions may skip a poll; expose incomplete coverage.
  A request does not authenticate or exercise PoA/storage private keys.

Fresh challenges prevent delivery replay across sessions/requests. They do not
prevent a dishonest endpoint from attaching old or fabricated measurements to
a fresh challenge. Cache age and every resource value remain declarations.
No new signatures, exporter construction or cryptographic domains are needed.

Implemented helper: `p2p::ObservationExchange` issues CSPRNG challenges, validates
the local admitted-HELLO/network context, consumes a reply only for its exact
pending session/challenge, and expires it at the five-second deadline. Malformed
payloads follow codec exceptions; valid mismatches return no report. It retains
at most four pending challenges, 128 shared numeric-address budget entries, and
16/32 send/serve timestamps in rolling 30-second windows. IPv4-mapped addresses
normalize to IPv4; ports are absent. Session close removes its challenge without
refunding the address cooldown; unused address metadata expires after 90 seconds.
Clock regression refuses work without resetting budgets. No accepted report,
raw payload/address log or persistent identifier is retained by this helper.

Runtime owns one shared guard. `PeerSession` uses the actual socket address and
matching admitted HELLO, with a nonrecycled process-local connection handle that
never enters the payload. `RequestObservation` is a synchronous transaction for
the existing owner; skipped requests do no I/O. Pending cleanup runs on every
return. A missing, malformed, wrong-type or mismatched response closes the client
socket, preventing a late observation body from contaminating the next transaction.
`ServeNext` answers valid admitted requests from the cache; a rate refusal drops
the request without closing the server session or assigning an abuse penalty.
Cache collection runs guard expiry; default clock reads occur under the guard
mutex, so concurrent collection/transactions do not look like clock regression.
The guard does not prove admission or truthful measurements independently of its
caller. No automatic poller calls this transaction yet. Scheduled polling,
busy-session fairness, remote UI/chart wiring, collector overhead and
coordinated deployed-software acceptance remain work.

## Grouping, expiry and consolidation

Use at most 32 volatile address-group slots, keyed by the actual socket's
numeric remote IP, normalizing IPv4-mapped IPv6. Ignore announced/forwarded
addresses. Ports and multiple TLS/storage sessions do not multiply groups.
Each group selects one existing session and retains it until it closes or is
unavailable; use local session order for selection, never reported capacity.
On replacement, discard the former report before accepting the new one.
Normalize local loopback transports into one local-address group and exclude
that group from remote sums; the local runtime remains a separate observation.

A report is fresh for 90 seconds after local monotonic receipt; closed sessions
invalidate immediately. Process restarts/session churn cannot add a second live
sample to the same address group. Do not subtract cumulative counters across
sessions: traffic uses the declared complete window. Expired slots lose their
payload; group/cooldown metadata expires within 90 seconds of last activity.
On runtime restart or network change clear all state. No report is forwarded,
requested on behalf of others, persisted, exported or logged with raw fields.

Display `fresh reporting address groups`, missing/expired/limited coverage and
local receipt age. Counts describe selected reports, not Full Node count:
NAT/shared IP can merge several machines; multiple IPs can represent one machine.
Even address-group sums cannot prove unique processes, disks or failure domains.
No address history is retained in charts, and generic diagnostics must not write
challenges/report payloads/IP labels. Existing live peer UI has its own inventory.

Permitted totals: sum selected reported V/budget/stored-copy units/admitted provider bytes,
clearly labelled declarations from this partial reporting set, with component
counts and rounding. Keep receive and send rates separate: adding them double
counts transfers across reporting endpoints. Storage utilization is sum reported
stored units / sum V over the same known storage reports. CPU is the arithmetic
mean of known reported normalized means, with contributor count and min/max
window/age, not network CPU utilization; memory is per report only. Different
known-field subsets must not silently share a denominator.

Finalized op/min and canonical register totals come from one locally verified
chain; never sum peer heights, block counts or operation rates. A remote cursor
matching a locally known block verifies only that reference, not remote storage,
measurement truth or current global freshness. Unknown ceiling, usable replica
capacity, unique logical content and whole-network coverage remain unknown.
When selected sessions/contributors change, start a new aggregate chart segment;
do not present membership changes as traffic growth or zero-filled history.

Implemented component: `p2p::ObservationGroups` keeps at most 32 volatile slots,
selects a connection before recording an exchange-accepted report, and preserves
that selection against competing ports/sessions. Close/replacement clears the
sample immediately; report receipt and metadata inactivity expire at 90 seconds.
CPU mean age advances on read and expires independently. Local loopbacks collapse
and are excluded from all remote totals. Snapshots contain no address/handle/
challenge/binding/row ID; a volatile cohort revision marks selection and known-
contributor changes for cohort chart segments. Checked sums, matched storage denominators,
separate traffic directions and counted arithmetic CPU means follow the rules
above; zero, missing and overflow remain distinct. The component does not verify
measurement truth or perform network I/O.

Runtime integration: each runtime creates its own empty store before starting the
background collector. The collector also expires group payloads/metadata. Explicit
owner calls to `RequestObservation` select the socket-address group before any
request; same-address competing sessions skip I/O, as do cooldown refusals. Only
exchange-accepted replies are recorded. Internal socket closure or session teardown
invalidates the selected report immediately; weak session references do not keep
runtime state alive. A live session cannot switch between collector runtimes.

`GetNetworkObservation()` returns `shared_ptr<const NetworkObservationSnapshot>`:
local cache metric blocks, separate remote group snapshot, local network binding
and presentation UTC time. No address/handle/challenge or stable row ID appears.
Local cache age and remote receipt/CPU ages remain explicit; sequential reads
are not a globally atomic measurement. Local data never enters remote sums.
Reading copies bounded data and expires stale groups, with no I/O, history scan,
Identity unlock or synchronous collection. Previously returned values are immutable.
Diagnostics carry the immutable snapshot through the existing background desktop
model to Network Advanced summary cards and public read-only Console capacity/metrics.
Both use matching EN/FR formatting with per-metric contributor counts, receipt/cache
ages and missing/limited coverage. Foreign-network/invalid-clock snapshots display
Unknown; absence and overflow never become zero. Snapshot monotonic capture age
advances displayed receipt/CPU ages; if background delivery stalls past a report
expiry, the stale aggregate becomes Unknown rather than retaining frozen values. No endpoint labels, memory/disk
sums or throughput ceiling.

The existing runtime collector samples the group snapshot about every five seconds
into at most 180 gauge points spanning at most 15 minutes. Sampling is independent
of page visibility; no catch-up/backfill, extra collector, I/O, disk history or
reporter identifiers. Unknown remains absent; exact aggregate byte fields and
per-metric counts/window/age metadata retain their meaning. Frame rates are still
declared 60-second measurements and CPU means keep their reported windows, not
five-second averages. Clock regression clears history; restart/network replacement
creates an empty ring. Reads expire old history without adding samples.

Network charts show declared capacity/stored copies, separate frame directions
and reported process CPU means. Lines break on volatile cohort changes, unknown
values and missed sampling spans; real elapsed times keep gaps visible. Point
selection exposes counts and ages through keyboard/mouse and accessible text.
No operation/memory/disk sums or potential ceiling. Charts cannot manufacture
reporting coverage.

## Idle-slot acquisition implementation

`CybouNetworkServiceConfig::observation_polling` is a local opt-in, default false;
existing desktop/headless startup does not enable it. No wire announcement,
capability negotiation, version probe, CLI/Console switch or new connection.
The existing outbound service owner invokes acquisition only during the pause
after normal sync/relay/fanout work and its update callback. A same-cycle
UP_TO_DATE result with no applied blocks and completed known-peer pass is an
idle scheduling hint, never a global freshness proof or consensus prerequisite.
Full-batch catch-up continues immediately without optional work.

Runtime tries the owner and chain mutexes without waiting, skips catching-up,
nonempty candidate pool and busy/contended local storage scheduler, and offers
at most one round-robin session every five seconds. A known pending finalized
fanout frontier takes precedence. Only existing admitted outbound sessions are
used; inbound and storage-session acquisition are not wired, so coverage stays
partial. Existing selected-group/per-IP/global guards decide whether to send.
A skipped request does no I/O and never opens a retry connection.

The transaction deadline is the earlier of two seconds and the remaining normal
service pause (normally 250 ms); the worker sleeps only its unused pause afterward.
Expired budgets consume no challenge/cooldown or bytes. Timeout/mismatch still
closes TLS to prevent late-body contamination; the manager removes that session
without an observation abuse penalty and ordinary mesh recovery follows.
There is no concurrent reader/writer and no interrupt of an in-flight transaction.
Idle checks are conservative hints, not reservations: new work arriving after
admission can wait for the remaining optional deadline. This is bounded owner
occupancy, not proof of zero latency/cost or fair progress under every workload.
Deploy/enable only after the coordinated software and governance acceptance below.

## Acceptance and release gates

### Risk and processing record

| Asset / concern | Engineering control | Residual risk / acceptance owner |
|---|---|---|
| Live transport address with resource/activity declarations | Direct admitted sessions only; fixed rounded fields; volatile 90-second retention; no raw logs/export or endpoint-labelled chart history | Addresses and CPU/memory patterns can still link activity. Processing roles, purpose/access assessment and operational risk reviewer must be assigned before deployment under the governing inventory |
| Measurement/aggregate integrity | Fresh session challenge, exact ranges, checked arithmetic, replacement/expiry and explicit declared/partial labels | Authenticated transport is not resource truth; address grouping is not unique-host deduplication. Maintainer acceptance must exercise lies, aliases, NAT and shared disks |
| Mesh availability and resource use | Fixed payloads/cache, per-IP/global budgets, bounded slots and cooldowns, low-priority transaction scheduling | Busy/limited sessions reduce coverage. Maintainer measures cost and block/relay/storage progress; operator accepts the stated coverage limits |
| PoA/content confidentiality | No signer, Identity, StorageId, content identifiers, per-user queue or lifetime counters in the report | Traffic/cursor patterns may still support inference. Security/privacy reviewer checks minimized serialization/logs; no anonymity assertion |
| Upgrade continuity | Keep current baseline untouched until coordinated software upgrade; no automatic unknown-message probes or parallel legacy decoder | Mixed deployed binaries reject extension messages. Operator records upgrade scope and recovery while retaining network/genesis, chain and signing history |

These are engineering controls/target owners, not an assigned sign-off or a
determination of legal applicability. Governing processing/control assessment
and an accountable operational risk owner remain release gates for any deployment.

Before implementation acceptance, verify exact layout/truncation/trailing-byte
rejection, statuses/ranges/overflow, unit boundaries, startup/idle/stale cache,
network/challenge/session mismatch, duplicate/late replies, reconnect/ports,
IPv4-mapped grouping, local exclusion, expiry, per-IP/global/churn limits,
bounded memory, fairness and busy-session progress. Test conflicting/lied values,
partial known subsets, weighted utilization, receive/send separation, membership
changes and no sum of memory/disk/chain streams. Verify locked/headless collection,
minimal logs and absence of Identity/PoA/content identifiers in serialized data.

Local completed PUT/GET payload counters now feed diagnostics, Network Advanced
and Console metrics separately from frame traffic. These are not remote report
fields; the fixed request/reply layout is unchanged.
Next bounded packages: remaining resource/register gauges. Idle-slot polling is implemented but default-off; deployment
acceptance, measured collector/mesh overhead and busy-workload fairness remain.
Auto polling waits for coordinated deployed-software acceptance. No extra wire
fields or persistent observation history are part of this sequence.
Measure collector cost and check protocol/traffic accounting overhead. Existing
local telemetry remains available until real reports are implemented.

This changes the active P2P message set. Coordinated DEVNET software upgrade is
required before polling deployed peers: older current binaries reject unknown
message codes. No automatic probe, dual wire decoder, negotiation fallback,
genesis replacement, key provisioning or chain/signing-history reset. MAINNET
release additionally requires the governing privacy/security processing review,
an assigned operational risk owner and scoped deployment evidence. Transport
implementation tests do not close those governance gates.

Relayed reports, persistent deduplication across addresses, network census and
proof of independent hosts remain open design work. Never fill those gaps with
new roles, a NodeID registry, StorageId used outside storage, or a PoA route.
