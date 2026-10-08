# Built-in Network observability

Status: active Level 4 delivery plan and Level 5 metric proposal, 2026-10-08.
User priority: continuously collect and consolidate actual network data in
CYBOU and present it on Network, with Console details. This is standard
production functionality, without Grafana, Prometheus, an external collector
or generated load. Collection continues while the page is hidden; presentation
pauses. This plan does not claim that complete collectors or remote reporting are
implemented and introduces no wire messages or canonical state fields.

## Metrics on Network

Navigation and page title are Network / Réseau. DEVNET remains the active
compiled network label; France admission remains a scoped detail.
A shared bounded observation model feeds summary cards, time-series charts,
peer details, Monitor and read-only Console independently of Identity unlock.

| Metric | Source and meaning | Evidence boundary |
|---|---|---|
| Finalized operations/min | Unique operations in independently verified blocks, over declared 1/5/15-minute windows | One chain stream, never summed across peers; historical sync separate from live observation; declare completeness and verified height |
| Transfers | Actual CYBOU transport received/sent bytes/s, with storage PUT/GET payload counters separately | Local first, then reporting cohort; summed send+receive counts both ends, not unique useful delivery |
| Data served | Actual encrypted ChunkBlobStore bytes locally and across deduplicated reporting nodes | Physical copies include replicas/cache; not unique logical content or verified remote durability |
| Register and rented volume | Active finalized publications, authorized billing units and active lease units at a verified cursor | Units times 512 KiB are billed allocation, not measured plaintext or physical bytes; never sum identical registers from peers |
| Storage capacity | V, physical use/headroom, provider budget floor(2V/3), obligations/headroom and actual disk free space | Policy and physical limits stay separate; cohort sums are declared capacity, not complete network totals |
| Potential usable storage | Headroom constrained by provider budget, physical store, disk reserve and eligible remote replica pairs | Scoped estimate only after placement/deduplication evidence exists; sum V divided by two cannot establish usable Beta capacity |
| Load | CYBOU process CPU/memory, storage I/O, queue depth/age and admission/transfer errors; host metrics separately | Normalize CPU by available processors; mean with sample count; capacity utilization = sum used / sum capacity; no synthetic universal load score |
| Node health | Running/initialized, safety halt, admission readiness, session reachability and storage failures | Reasons and age; remote resources self-reported; silence or old block alone cannot establish a network outage |
| Potential operation throughput | Accepted historical sustained workload ceiling with latency/failure gates, topology/build/date | Separate from present rate; idle observations or spare CPU cannot establish a maximum; unavailable ceiling stays Unknown |

Every value carries NetworkBinding, source, scope, window/time, sample count,
completeness and stale/unknown state. A complete quiet window may be zero;
gaps, restart and catch-up must not silently become zero. A network p95 requires
real latency samples or mergeable distributions, not averaged per-node p95.
Queues remain volatile local observations, not canonical pending state.

## Collection and consolidation design

Instrument existing commit, relay and storage-transfer paths with bounded
passive counters. Use monotonic elapsed time for rates and UTC for display.
Count accepted finalized operations once; duplicate delivery never increments.
Conflict replacement invalidates/reconciles the affected window. Historical
catch-up has its own series rather than a burst of current network production.

Proposed initial retention: five-second buckets for 30 minutes and one-minute
buckets for 24 hours, bounded in memory; restart starts a new window. Measure
sampler cost and cap cardinality. Persistence needs a separate privacy/storage
design. Refresh reads the shared snapshot without scanning history, enumerating
foreign objects, triggering storage proofs, audits, repair or benchmarks.

Remote consolidation is required work. Before transport changes, freeze a
Level 1/2 design for minimized reports: fixed bounds, polling/rate limits,
expiration, network/session binding, replay handling and deduplication across
session churn/relayed reports. No new protocol NodeID, capability bitmap,
provider registry or PoA route. Distinguish direct and relayed provenance;
authentication establishes a reporter, not truthful disk/CPU/service values
or independent physical hosts. Bootstrap has no special aggregation authority.

Remote totals remain unavailable until that design and implementation exist.
Then show fresh deduplicated reporting-cohort count and expired/missing report
count; fraction of the whole network stays unknown without a census source.
Observations grant no placement weight, payouts or finality power. Avoid public
endpoint histories, Identity links, filenames, recipients and content IDs in
time series. Canonical streams/registers must never be added across reporters.

## Small implementation batches

1. **O1 Local collector:** passive counters/windows, canonical register aggregates
   at a declared cursor, local resource/storage measurements. Verify idle/gaps,
   restart, catch-up, duplicate delivery, conflict replacement, concurrency and
   collection overhead. Missing measurements remain Unknown.
2. **O2 Network and Console:** cards/charts from O1 and matching proposed
   `health`, `metrics`, `capacity` read-only commands; map/Advanced retained,
   FR/EN, themes, stale/unknown states. `health` has its first bounded package
   below; `metrics` has its traffic package below; `capacity` remains planned.
3. **O3 Remote report design:** close normative trust/privacy/deduplication and
   abuse gates before modifying P2P. No new network, genesis or signer needed.
4. **O4 Cohort aggregation:** accepted reports, bounded aggregation and
   per-node/mean/cohort charts. Verify duplicates/replay, expiry, churn,
   inconsistent/dishonest reports and partial visibility.
5. **O5 Capacity estimates:** service-eligible storage with replica constraints;
   optional accepted historical throughput ceiling beside actual current rate.
   Benchmarks validate ceilings separately and are not needed for monitoring.

### O1 first bounded package (2026-10-08)

Implemented: UTC observation timestamp, monotonic runtime uptime, actual local
candidate-pool count and serialized bytes in the shared diagnostics snapshot.
The existing background diagnostics refresh collects them independently of
Network visibility. Network Advanced Overview displays uptime, sample time and
queue load; read-only `health` exposes the same sample while locked or unlocked.
Unknown sample/initialization remains explicit; no synthetic health score.
The bounded pool is read under its existing lock without scanning history.

Still open: rate windows/retention, transfer counters, CPU/memory/I/O, canonical
register aggregates, charts, remote reports/consolidation and capacity estimates.
`health` is implemented; the traffic package below adds `metrics`. `capacity`
remains planned.

### O1 passive traffic package (2026-10-08)

Production inbound, mesh-outbound and storage-pool sessions share one runtime
meter. Successful TLS application reads/writes count actual bytes once per
progress result, including partial transfers, invalid input, retries and service
frames. TLS handshake/record and TCP/IP overhead are excluded. Frame-stream
bytes are neither NIC traffic nor unique content delivered.

A fixed 61-slot one-second ring provides the preceding 60 complete seconds;
the current partial second is excluded. Rates are Unknown before 60 seconds.
Continuous passive collection makes complete idle windows a measured zero.
Reads cannot reset counters. Totals survive session closure/reconnect but reset
with the runtime. Fixed memory, one short mutex, no per-peer identifiers.

Network Advanced Overview and public read-only `metrics` show received/sent
rates; Console also shows exact lifetime totals and observation time. Separate
PUT/GET payload rates, charts, operation rates, resources and remote aggregation
remain open. This ring is not the proposed 24-hour history collector.

Existing CI and Beta acceptance gates remain. Work in bounded packages;
collector preparation does not require a live load test or signer restart.

### O1 finalized-operation windows (2026-10-08)

Successful canonical commits now feed a fixed 901-slot one-second ring for
1/5/15-minute windows. Counts are operations, not block-height deltas. Local
production and direct accepted announcements form the observed series;
historical batch sync/default imports form a separate history series. Only
successful commits count, so repeated/rejected blocks do not increase totals.
A replacement at an existing height resets all operation windows and totals;
restart starts new measurements. Empty blocks add zero operations.

Network Advanced displays one-minute observed finalized op/min; `metrics`
exposes all three windows, local-production contribution, history import counts
and totals since observation reset. Incomplete windows are Unknown. Complete
idle local windows can be zero without claiming global network inactivity.
Current partial seconds are excluded; collection uses the existing chain lock
and does not scan stored blocks or depend on GUI visibility.

Blocks contain no creation timestamp. Arrival-time announcements can be delayed
old blocks; their classification is local provenance, not a freshness proof.
These rates must not be relabeled network production speed or potential ceiling.
The current retained ring supports rate windows; historical charts/export,
network availability coverage and broader consolidation remain open.

### O2 bounded observation charts (2026-10-08)

The runtime now supplies up to 180 completed five-second intervals, covering
15 minutes, independently of page visibility or diagnostic polling. Traffic
points contain received/sent frame bytes; operation points contain observed
and locally produced operations, excluding history imports. Missing startup
intervals are absent, not padded with zeros. Complete idle intervals are zero.
Both raw rings retain 905 seconds so the oldest completed interval survives
while the newest five-second interval is still incomplete.

Network Advanced Overview renders two native Qt plots. Traffic values are B/s
over each five-second interval; operations are normalized to op/min over that
interval. These differ from the one-minute summary windows; short burst peaks
do not establish network production time or a performance ceiling. Legend
lines use distinct colors and solid/dashed patterns. Mouse selection and
Left/Right/Home/End expose exact interval values through tooltip and accessible
description; a focus border supports keyboard navigation. Selection is retained
by interval time while it remains in the bounded history.

No external chart service, disk history, Identity metadata, endpoint labels,
network census or new wire fields. Restart clears both histories; canonical
replacement clears operation history. Unavailable observations clear the plots.
This delivers the first 15-minute in-memory charts; the proposed 30-minute /
24-hour retention, exporting, resource series and remote totals remain open.

### O1 instantaneous process memory (2026-10-08)

Runtime diagnostics read the local executable's resident bytes from the OS:
Windows working set or Linux RSS via the fixed `/proc/self/statm` record.
Failure and unsupported platforms return an absent value. This is a current
gauge, not a peak, average, allocation total or host memory measurement. Shared
resident pages and the desktop GUI are included. It is not unique physical RAM
and must not be summed across nodes as a network memory total.

Network Advanced Overview shows the sample in human-readable units; read-only
`metrics` shows exact bytes and the same snapshot's UTC time. No Identity unlock,
process enumeration, external service or retained per-process labels. The
background snapshot collection remains independent of page visibility.
The CPU package below adds interval normalization and completed means; resource
history, host memory,
storage I/O and remote resource reports remain open.

### O1 process CPU intervals and completed means (2026-10-08)

The OS continuously accounts cumulative CPU time for the whole CYBOU process.
Runtime diagnostic reads sample that counter (Windows kernel+user process time;
Linux process CPU clock). Percent is the counter delta divided by monotonic
elapsed time and OS online logical processor count, times 100. This denominator
is explicit: it does not represent affinity, container quotas or host load.
The first reading has no interval. Read failures, counter/time regression or
a changed processor count reset the baseline and mean, with Unknown output.

A fixed-memory accumulator publishes elapsed-time-weighted means when a window
reaches at least 60 seconds. Actual duration and interval count are exposed;
the latest completed mean retains its age until the next window completes.
Sparse diagnostic reads produce longer real windows, not invented five-second
samples or an exact rolling-minute promise. Reads less than one millisecond
apart add no interval and retain the baseline.
The existing desktop background sampling remains independent of Network
visibility. Runtime restart resets the accumulator. Sampling is serialized.

Network Advanced and Console `metrics` display interval CPU, completed mean,
denominator, measured durations, interval count and mean age in EN/FR. GUI CPU
is included. Neither CPU percentage nor spare CPU estimates network capacity.
CPU charts, affinity/quota-aware measurements, host load, storage I/O and remote
consolidation remain open.
